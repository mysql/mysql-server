/*
 Copyright (c) 2012, Oracle and/or its affiliates. All rights reserved.
 
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
#ifndef NDBMEMCACHE_ENGINE_ERRORS_H
#define NDBMEMCACHE_ENGINE_ERRORS_H

#include <ndberror.h>

/* 
   The NDB Engine for Memcached uses error codes 29000 - 29999 
*/



/*** Errors 290xx and 291xx are reported as "Scheduler Error" ***/

/* 2900x: general scheduler error codes */
extern ndberror_struct AppError29001_ReconfLock;
extern ndberror_struct AppError29002_NoNDBs;

/* 2902x: blocking NDB operations in worker thread */
extern ndberror_struct AppError29023_SyncClose;
extern ndberror_struct AppError29024_autogrow;

/*** Errors 29200 and up are reported as "Memcached Error" ***/

#endif
