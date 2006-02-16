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

#ifndef KERNEL_RECORDS_HPP
#define KERNEL_RECORDS_HPP

/**
 * Record types
 */
#define PGMAN_PAGE_REQUEST      1

#define LGMAN_LOG_BUFFER_WAITER 2
#define LGMAN_LOG_SYNC_WAITER   3

#define DBTUP_PAGE_REQUEST      4
#define DBTUP_EXTENT_INFO       5

/**
 * Resource groups
 */

/**
 * Operations for dd
 *    PGMAN_PAGE_REQUEST
 *    LGMAN_LOG_BUFFER_WAITER
 *    LGMAN_LOG_SYNC_WAITER
 *    DBTUP_PAGE_REQUEST
 */
#define RG_DISK_OPERATIONS      1

/**
 * Records for dd
 *   DBTUP_EXTENT_INFO
 */
#define RG_DISK_RECORDS         2

#endif
