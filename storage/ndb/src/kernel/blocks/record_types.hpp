/*
   Copyright (c) 2006, 2010, Oracle and/or its affiliates. All rights reserved.

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

#ifndef KERNEL_RECORDS_HPP
#define KERNEL_RECORDS_HPP

/**
 * Resource groups
 */

/**
 * Operations for dd
 *    PGMAN_PAGE_REQUEST
 *    LGMAN_LOG_WAITER
 *    DBTUP_PAGE_REQUEST
 */
#define RG_DISK_OPERATIONS      1

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
 * 
 */
#define RG_RESERVED             0
#define RG_COUNT                9

/**
 * Record types
 */
#define RT_PGMAN_PAGE_REQUEST      MAKE_TID( 1, RG_DISK_OPERATIONS)
#define RT_LGMAN_LOG_WAITER        MAKE_TID( 2, RG_DISK_OPERATIONS)
#define RT_DBTUP_PAGE_REQUEST      MAKE_TID( 3, RG_DISK_OPERATIONS)

#define RT_DBTUP_EXTENT_INFO       MAKE_TID( 1, RG_DISK_RECORDS)
#define RT_DBDICT_FILE             MAKE_TID( 2, RG_DISK_RECORDS)
#define RT_DBDICT_FILEGROUP        MAKE_TID( 3, RG_DISK_RECORDS)
#define RT_LGMAN_FILE              MAKE_TID( 4, RG_DISK_RECORDS)
#define RT_LGMAN_FILEGROUP         MAKE_TID( 5, RG_DISK_RECORDS)
#define RT_TSMAN_FILE              MAKE_TID( 6, RG_DISK_RECORDS)
#define RT_TSMAN_FILEGROUP         MAKE_TID( 7, RG_DISK_RECORDS)

#define RT_DBTUP_PAGE              MAKE_TID( 1, RG_DATAMEM)
#define RT_DBTUP_PAGE_MAP          MAKE_TID( 2, RG_DATAMEM)

#define RT_JOB_BUFFER              MAKE_TID( 1, RG_JOBBUFFER)

#define RT_FILE_BUFFER             MAKE_TID( 1, RG_FILE_BUFFERS)

#define RT_SPJ_REQUEST             MAKE_TID( 1, RG_QUERY_MEMORY)
#define RT_SPJ_TREENODE            MAKE_TID( 2, RG_QUERY_MEMORY)
#define RT_SPJ_ARENA_BLOCK         MAKE_TID( 3, RG_QUERY_MEMORY)
#define RT_SPJ_DATABUFFER          MAKE_TID( 4, RG_QUERY_MEMORY)
#define RT_SPJ_SCANFRAG            MAKE_TID( 5, RG_QUERY_MEMORY)

#endif
