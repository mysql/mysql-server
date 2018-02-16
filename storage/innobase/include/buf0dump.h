/*****************************************************************************

Copyright (c) 2011, 2018, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file include/buf0dump.h
 Implements a buffer pool dump/load.

 Created April 08, 2011 Vasil Dimov
 *******************************************************/

#ifndef buf0dump_h
#define buf0dump_h

#include "univ.i"

/** Wakes up the buffer pool dump/load thread and instructs it to start
 a dump. This function is called by MySQL code via buffer_pool_dump_now()
 and it should return immediately because the whole MySQL is frozen during
 its execution. */
void buf_dump_start();

/** Wakes up the buffer pool dump/load thread and instructs it to start
 a load. This function is called by MySQL code via buffer_pool_load_now()
 and it should return immediately because the whole MySQL is frozen during
 its execution. */
void buf_load_start();

/** Aborts a currently running buffer pool load. This function is called by
 MySQL code via buffer_pool_load_abort() and it should return immediately
 because the whole MySQL is frozen during its execution. */
void buf_load_abort();

/** This is the main thread for buffer pool dump/load. It waits for an
event and when waked up either performs a dump or load and sleeps
again. */
void buf_dump_thread();

/** Generate the path to the buffer pool dump/load file.
@param[out]	path		generated path
@param[in]	path_size	size of 'path', used as in snprintf(3). */
void buf_dump_generate_path(char *path, size_t path_size);

#endif /* buf0dump_h */
