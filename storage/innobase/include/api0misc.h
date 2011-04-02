/******************************************************
Interface between Innobase and client. This file contains
the functions that don't have a proper home yet.

(c) 2008 Oracle Corpn./Innobase Oy
*******************************************************/

#include "univ.i"
#include "os0file.h"
#include "que0que.h"
#include "trx0trx.h"

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
