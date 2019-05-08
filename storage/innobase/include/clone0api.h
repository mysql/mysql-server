/*****************************************************************************

Copyright (c) 2018, Oracle and/or its affiliates. All Rights Reserved.

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

/** @file include/clone0api.h
 Innodb Clone Interface

 *******************************************************/

#ifndef CLONE_API_INCLUDE
#define CLONE_API_INCLUDE

#include "univ.i"
#ifndef UNIV_HOTBACKUP
#include "handler.h"

/** Get capability flags for clone operation
@param[out]	flags	capability flag */
void innodb_clone_get_capability(Ha_clone_flagset &flags);

/** Begin copy from source database
@param[in]	hton	handlerton for SE
@param[in]	thd	server thread handle
@param[in,out]	loc	locator
@param[in,out]	loc_len	locator length
@param[out]	task_id	task identifier
@param[in]	type	clone type
@param[in]	mode	mode for starting clone
@return error code */
int innodb_clone_begin(handlerton *hton, THD *thd, const byte *&loc,
                       uint &loc_len, uint &task_id, Ha_clone_type type,
                       Ha_clone_mode mode);

/** Copy data from source database in chunks via callback
@param[in]	hton	handlerton for SE
@param[in]	thd	server thread handle
@param[in]	loc	locator
@param[in]	loc_len	locator length in bytes
@param[in]	task_id	task identifier
@param[in]	cbk	callback interface for sending data
@return error code */
int innodb_clone_copy(handlerton *hton, THD *thd, const byte *loc, uint loc_len,
                      uint task_id, Ha_clone_cbk *cbk);

/** Acknowledge data to source database
@param[in]	hton	handlerton for SE
@param[in]	thd	server thread handle
@param[in]	loc	locator
@param[in]	loc_len	locator length in bytes
@param[in]	task_id	task identifier
@param[in]	in_err	inform any error occurred
@param[in]	cbk	callback interface for receiving data
@return error code */
int innodb_clone_ack(handlerton *hton, THD *thd, const byte *loc, uint loc_len,
                     uint task_id, int in_err, Ha_clone_cbk *cbk);

/** End copy from source database
@param[in]	hton	handlerton for SE
@param[in]	thd	server thread handle
@param[in]	loc	locator
@param[in]	loc_len	locator length in bytes
@param[in]	task_id	task identifier
@param[in]	in_err	error code when ending after error
@return error code */
int innodb_clone_end(handlerton *hton, THD *thd, const byte *loc, uint loc_len,
                     uint task_id, int in_err);

/** Begin apply to destination database
@param[in]	hton		handlerton for SE
@param[in]	thd		server thread handle
@param[in,out]	loc		locator
@param[in,out]	loc_len		locator length
@param[out]	task_id		task identifier
@param[in]	mode		mode for starting clone
@param[in]	data_dir	target data directory
@return error code */
int innodb_clone_apply_begin(handlerton *hton, THD *thd, const byte *&loc,
                             uint &loc_len, uint &task_id, Ha_clone_mode mode,
                             const char *data_dir);

/** Apply data to destination database in chunks via callback
@param[in]	hton	handlerton for SE
@param[in]	thd	server thread handle
@param[in]	loc	locator
@param[in]	loc_len	locator length in bytes
@param[in]	task_id	task identifier
@param[in]	in_err	inform any error occurred
@param[in]	cbk	callback interface for receiving data
@return error code */
int innodb_clone_apply(handlerton *hton, THD *thd, const byte *loc,
                       uint loc_len, uint task_id, int in_err,
                       Ha_clone_cbk *cbk);

/** End apply to destination database
@param[in]	hton	handlerton for SE
@param[in]	thd	server thread handle
@param[in]	loc	locator
@param[in]	loc_len	locator length in bytes
@param[in]	task_id	task identifier
@param[in]	in_err	error code when ending after error
@return error code */
int innodb_clone_apply_end(handlerton *hton, THD *thd, const byte *loc,
                           uint loc_len, uint task_id, int in_err);

/** Initialize Clone system
@return inndodb error code */
dberr_t clone_init();

/** Uninitialize Clone system */
void clone_free();

/** Mark clone system for abort to disallow database clone
@param[in]	force	abort running database clones
@return true if successful. */
bool clone_mark_abort(bool force);

/** Mark clone system as active to allow database clone. */
void clone_mark_active();

#else                         /* !UNIV_HOTBACKUP */
#define clone_mark_abort(_P_) /*clone_mark_abort()*/
#define clone_mark_active()   /*clone_mark_active()*/
#endif                        /* !UNIV_HOTBACKUP */

/** Fix cloned non-Innodb tables during recovery.
@param[in,out]  thd     current THD
@return true if error */
bool fix_cloned_tables(THD *thd);

#endif /* CLONE_API_INCLUDE */
