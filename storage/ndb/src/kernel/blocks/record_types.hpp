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
 * 
 */
#define RG_RESERVED             0
#define RG_COUNT                3

/**
 * Record types
 */
#define RT_PGMAN_PAGE_REQUEST      MAKE_TID( 1, RG_DISK_OPERATIONS)
#define RT_LGMAN_LOG_WAITER        MAKE_TID( 2, RG_DISK_OPERATIONS)
#define RT_DBTUP_PAGE_REQUEST      MAKE_TID( 3, RG_DISK_OPERATIONS)

#define RT_DBTUP_EXTENT_INFO       MAKE_TID( 4, RG_DISK_RECORDS)
#define RT_DBDICT_FILE             MAKE_TID( 5, RG_DISK_RECORDS)
#define RT_DBDICT_FILEGROUP        MAKE_TID( 6, RG_DISK_RECORDS)
#define RT_LGMAN_FILE              MAKE_TID( 7, RG_DISK_RECORDS)
#define RT_LGMAN_FILEGROUP         MAKE_TID( 8, RG_DISK_RECORDS)
#define RT_TSMAN_FILE              MAKE_TID( 9, RG_DISK_RECORDS)
#define RT_TSMAN_FILEGROUP         MAKE_TID(10, RG_DISK_RECORDS)

#endif
