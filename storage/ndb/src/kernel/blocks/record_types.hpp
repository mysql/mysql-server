/*
   Copyright (c) 2006, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef KERNEL_RECORDS_HPP
#define KERNEL_RECORDS_HPP

#define JAM_FILE_ID 347


/**
 * Resource groups
 */

/**
 * Transaction memory == "operation records" needed to access/modify data in DB
 */
#define RG_TRANSACTION_MEMORY   1

/**
 * Records for dd
 *   DBTUP_EXTENT_INFO
 */
#define RG_DISK_RECORDS         2

/**
 * Records for data memory
 */
#define RG_DATAMEM              3

/**
 * Records for job buffers (multi-threaded ndbd only).
 */
#define RG_JOBBUFFER            4

/**
 * File-thread buffers
 */
#define RG_FILE_BUFFERS         5

/**
 * Transporter buffers
 */
#define RG_TRANSPORTER_BUFFERS  6

/**
 * Disk page buffer
 */
#define RG_DISK_PAGE_BUFFER     7

/**
 * Query memory
 */
#define RG_QUERY_MEMORY         8

/**
 * Schema transaction memory
 */
#define RG_SCHEMA_TRANS_MEMORY  9

/**
 *
 */
#define RG_RESERVED             0
#define RG_COUNT                10

/**
 * Record types
 */
#define RT_PGMAN_PAGE_REQUEST      MAKE_TID( 1, RG_TRANSACTION_MEMORY)
#define RT_LGMAN_LOG_WAITER        MAKE_TID( 2, RG_TRANSACTION_MEMORY)
#define RT_DBTUP_PAGE_REQUEST      MAKE_TID( 3, RG_TRANSACTION_MEMORY)
#define RT_DBTUP_COPY_PAGE         MAKE_TID( 4, RG_TRANSACTION_MEMORY)
#define RT_NDBFS_BUILD_INDEX_PAGE  MAKE_TID( 5, RG_TRANSACTION_MEMORY)
#define RT_NDBFS_INIT_FILE_PAGE    MAKE_TID( 6, RG_TRANSACTION_MEMORY)
#define RT_SUMA_EVENT_BUFFER       MAKE_TID( 7, RG_TRANSACTION_MEMORY)
#define RT_SUMA_TRIGGER_BUFFER     MAKE_TID( 8, RG_TRANSACTION_MEMORY)
#define RT_DBTC_FRAG_LOCATION      MAKE_TID( 9, RG_TRANSACTION_MEMORY)

#define RT_DBTUP_EXTENT_INFO       MAKE_TID( 1, RG_DISK_RECORDS)
#define RT_DBDICT_FILE             MAKE_TID( 2, RG_DISK_RECORDS)
#define RT_DBDICT_FILEGROUP        MAKE_TID( 3, RG_DISK_RECORDS)
#define RT_LGMAN_FILE              MAKE_TID( 4, RG_DISK_RECORDS)
#define RT_LGMAN_FILEGROUP         MAKE_TID( 5, RG_DISK_RECORDS)
#define RT_TSMAN_FILE              MAKE_TID( 6, RG_DISK_RECORDS)
#define RT_TSMAN_FILEGROUP         MAKE_TID( 7, RG_DISK_RECORDS)
#define RT_PGMAN_FILE              MAKE_TID( 8, RG_DISK_RECORDS)

#define RT_DBTUP_PAGE              MAKE_TID( 1, RG_DATAMEM)
#define RT_DBTUP_PAGE_MAP          MAKE_TID( 2, RG_DATAMEM)
#define RT_DBACC_DIRECTORY         MAKE_TID( 3, RG_DATAMEM)
#define RT_DBACC_PAGE              MAKE_TID( 4, RG_DATAMEM)

#define RT_JOB_BUFFER              MAKE_TID( 1, RG_JOBBUFFER)

#define RT_FILE_BUFFER             MAKE_TID( 1, RG_FILE_BUFFERS)

#define RT_SPJ_REQUEST             MAKE_TID( 1, RG_QUERY_MEMORY)
#define RT_SPJ_TREENODE            MAKE_TID( 2, RG_QUERY_MEMORY)
#define RT_SPJ_ARENA_BLOCK         MAKE_TID( 3, RG_QUERY_MEMORY)
#define RT_SPJ_DATABUFFER          MAKE_TID( 4, RG_QUERY_MEMORY)
#define RT_SPJ_SCANFRAG            MAKE_TID( 5, RG_QUERY_MEMORY)

#define RT_DBDICT_SCHEMA_TRANS_ARENA MAKE_TID( 1, RG_SCHEMA_TRANS_MEMORY)
#define RT_DBDICT_SCHEMA_TRANSACTION MAKE_TID( 2, RG_SCHEMA_TRANS_MEMORY)
#define RT_DBDICT_SCHEMA_OPERATION   MAKE_TID( 3, RG_SCHEMA_TRANS_MEMORY)
#define RT_DBDICT_CREATE_TABLE       MAKE_TID( 4, RG_SCHEMA_TRANS_MEMORY)
#define RT_DBDICT_DROP_TABLE         MAKE_TID( 5, RG_SCHEMA_TRANS_MEMORY)
#define RT_DBDICT_ALTER_TABLE        MAKE_TID( 6, RG_SCHEMA_TRANS_MEMORY)
#define RT_DBDICT_CREATE_TRIGGER     MAKE_TID( 7, RG_SCHEMA_TRANS_MEMORY)
#define RT_DBDICT_DROP_TRIGGER       MAKE_TID( 8, RG_SCHEMA_TRANS_MEMORY)
#define RT_DBDICT_CREATE_INDEX       MAKE_TID( 9, RG_SCHEMA_TRANS_MEMORY)
#define RT_DBDICT_DROP_INDEX         MAKE_TID( 10, RG_SCHEMA_TRANS_MEMORY)
#define RT_DBDICT_ALTER_INDEX        MAKE_TID( 11, RG_SCHEMA_TRANS_MEMORY)
#define RT_DBDICT_BUILD_INDEX        MAKE_TID( 12, RG_SCHEMA_TRANS_MEMORY)
#define RT_DBDICT_INDEX_STAT         MAKE_TID( 13, RG_SCHEMA_TRANS_MEMORY)
#define RT_DBDICT_CREATE_FILEGROUP   MAKE_TID( 14, RG_SCHEMA_TRANS_MEMORY)
#define RT_DBDICT_CREATE_FILE        MAKE_TID( 15, RG_SCHEMA_TRANS_MEMORY)
#define RT_DBDICT_DROP_FILEGROUP     MAKE_TID( 16, RG_SCHEMA_TRANS_MEMORY)
#define RT_DBDICT_DROP_FILE          MAKE_TID( 17, RG_SCHEMA_TRANS_MEMORY)
#define RT_DBDICT_CREATE_HASH_MAP    MAKE_TID( 18, RG_SCHEMA_TRANS_MEMORY)
#define RT_DBDICT_COPY_DATA          MAKE_TID( 19, RG_SCHEMA_TRANS_MEMORY)
#define RT_DBDICT_CREATE_NODEGROUP   MAKE_TID( 20, RG_SCHEMA_TRANS_MEMORY)
#define RT_DBDICT_DROP_NODEGROUP     MAKE_TID( 21, RG_SCHEMA_TRANS_MEMORY)
#define RT_DBDICT_OP_SECTION_BUFFER  MAKE_TID( 22, RG_SCHEMA_TRANS_MEMORY)


#undef JAM_FILE_ID

#endif
