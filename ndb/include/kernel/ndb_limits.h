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

#ifndef NDB_LIMITS_H
#define NDB_LIMITS_H

#define RNIL    0xffffff00

/**
 * Note that actual value = MAX_NODES - 1,
 *  since NodeId = 0 can not be used
 */
#define MAX_NDB_NODES 49
#define MAX_NODES     64

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
 * The maximum number of log execution rounds at system restart
 */
#define MAX_LOG_EXEC 4

/**
 * The maximum number of tuples per page
 **/
#define MAX_TUPLES_PER_PAGE 8191
#define MAX_TUPLES_BITS 13 		/* 13 bits = 8191 tuples per page */
//#define MAX_NO_OF_TUPLEKEY 16 Not currently used
#define MAX_TABLES 1600
#define MAX_TAB_NAME_SIZE 128
#define MAX_ATTR_NAME_SIZE 32
#define MAX_ATTR_DEFAULT_VALUE_SIZE 128
#define MAX_ATTRIBUTES_IN_TABLE 128
#define MAX_ATTRIBUTES_IN_INDEX 32
#define MAX_TUPLE_SIZE_IN_WORDS 2013
#define MAX_FIXED_KEY_LENGTH_IN_WORDS 8
#define MAX_KEY_SIZE_IN_WORDS 1023
#define MAX_FRM_DATA_SIZE 6000

#define MIN_ATTRBUF ((MAX_ATTRIBUTES_IN_TABLE/24) + 1)
/*
 * Number of Records to fetch per SCAN_NEXTREQ in a scan in LQH. The
 * API can order a multiple of this number of records at a time since
 * fragments can be scanned in parallel.
 */
#define MAX_PARALLEL_OP_PER_SCAN 16
/*
 * Maximum number of Parallel Scan queries on one hash index fragment
 */
#define MAX_PARALLEL_SCANS_PER_FRAG 12
/*
 * Maximum parallel ordered index scans per primary table fragment.
 * Implementation limit is (256 minus 12).
 */
#define MAX_PARALLEL_INDEX_SCANS_PER_FRAG 32

/**
 * Computed defines
 */
#define MAXNROFATTRIBUTESINWORDS (MAX_ATTRIBUTES_IN_TABLE / 32)

/*
 * Ordered index constants.  Make configurable per index later.
 */
#define MAX_TTREE_NODE_SIZE 64		// total words in node
#define MAX_TTREE_PREF_SIZE 4		// words in min prefix
#define MAX_TTREE_NODE_SLACK 3		// diff between max and min occupancy

/*
 * Blobs.
 */
#define NDB_BLOB_HEAD_SIZE 2            // sizeof(NdbBlob::Head) >> 2

#endif
