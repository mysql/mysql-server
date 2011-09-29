/*****************************************************************************

Copyright (c) 2008, 2011, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA

*****************************************************************************/

/**********************************************************************
This file contains the functions that don't have a proper home yet.
************************************************************************/

#include "univ.i"
#include "os0file.h"
#include "que0que.h"
#include "trx0trx.h"

/** Whether binlog is enabled for applications using InnoDB APIs */
extern my_bool                  ib_binlog_enabled;

/**********************************************************************
Create a temporary file using the OS specific function. */
UNIV_INTERN
int
ib_create_tempfile(
/*===============*/
	const char*	filename);	/*!< in: temp filename prefix */
	
/********************************************************************
Handles user errors and lock waits detected by the database engine.
@return	TRUE if it was a lock wait and we should continue running the query thread */
UNIV_INTERN
ibool
ib_handle_errors(
/*=============*/
	enum db_err*	new_err,	/*!< out: possible new error
					encountered in lock wait, or if
					no new error, the value of
					trx->error_state at the entry of this
					function */
	trx_t*		trx,		/*!< in: transaction */
	que_thr_t*	thr,		/*!< in: query thread */
	trx_savept_t*	savept);	/*!< in: savepoint or NULL */

/*************************************************************************
Sets a lock on a table.
@return	error code or DB_SUCCESS */
UNIV_INTERN
enum db_err
ib_trx_lock_table_with_retry(
/*=========================*/
	trx_t*		trx,		/*!< in/out: transaction */
	dict_table_t*	table,		/*!< in: table to lock */
	enum lock_mode	mode);		/*!< in: lock mode */

/*************************************************************************
Updates the table modification counter and calculates new estimates
for table and index statistics if necessary. */
UNIV_INTERN
void
ib_update_statistics_if_needed(
/*===========================*/
	dict_table_t*	table);	/*!< in/out: table */
