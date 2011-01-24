/*****************************************************************************

Copyright (c) 1996, 2011, Oracle and/or its affiliates. All Rights Reserved.

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

/**************************************************//**
@file trx/trx0trx.c
The transaction

Created 3/26/1996 Heikki Tuuri
*******************************************************/

#include "trx0trx.h"

#ifdef UNIV_NONINL
#include "trx0trx.ic"
#endif

#include "trx0undo.h"
#include "trx0rseg.h"
#include "log0log.h"
#include "que0que.h"
#include "lock0lock.h"
#include "trx0roll.h"
#include "usr0sess.h"
#include "read0read.h"
#include "srv0srv.h"
#include "srv0start.h"
#include "thr0loc.h"
#include "btr0sea.h"
#include "os0proc.h"
#include "trx0xa.h"
#include "ha_prototypes.h"
#include "srv0mon.h"

/** Dummy session used currently in MySQL interface */
UNIV_INTERN sess_t*		trx_dummy_sess = NULL;

/** Number of transactions currently allocated for MySQL: protected by
trx_sys->lock */
UNIV_INTERN ulint		trx_n_mysql_transactions = 0;

#ifdef UNIV_PFS_MUTEX
/* Key to register the mutex with performance schema */
UNIV_INTERN mysql_pfs_key_t	trx_mutex_key;
/* Key to register the mutex with performance schema */
UNIV_INTERN mysql_pfs_key_t	trx_undo_mutex_key;
#endif /* UNIV_PFS_MUTEX */

/*************************************************************//**
Set detailed error message for the transaction. */
UNIV_INTERN
void
trx_set_detailed_error(
/*===================*/
	trx_t*		trx,	/*!< in: transaction struct */
	const char*	msg)	/*!< in: detailed error message */
{
	ut_strlcpy(trx->detailed_error, msg, sizeof(trx->detailed_error));
}

/*************************************************************//**
Set detailed error message for the transaction from a file. Note that the
file is rewinded before reading from it. */
UNIV_INTERN
void
trx_set_detailed_error_from_file(
/*=============================*/
	trx_t*	trx,	/*!< in: transaction struct */
	FILE*	file)	/*!< in: file to read message from */
{
	os_file_read_string(file, trx->detailed_error,
			    sizeof(trx->detailed_error));
}

/****************************************************************//**
Creates and initializes a transaction object. It must be explicitly
started with trx_start_if_not_started() before using it. The default
isolation level is TRX_ISO_REPEATABLE_READ.
@return transaction instance, should never be NULL */
static
trx_t*
trx_create(void)
/*============*/
{
	trx_t*	trx;

	trx = mem_zalloc(sizeof(*trx));

	mutex_create(trx_mutex_key, &trx->mutex, SYNC_TRX);

	trx->magic_n = TRX_MAGIC_N;

	trx->state = TRX_STATE_NOT_STARTED;

	trx->isolation_level = TRX_ISO_REPEATABLE_READ;

	trx->no = IB_ULONGLONG_MAX;

	trx->support_xa = TRUE;

	trx->check_foreigns = TRUE;
	trx->check_unique_secondary = TRUE;

	trx->dict_operation = TRX_DICT_OP_NONE;

	mutex_create(trx_undo_mutex_key, &trx->undo_mutex, SYNC_TRX_UNDO);

	trx->error_state = DB_SUCCESS;

	trx->lock.que_state = TRX_QUE_RUNNING;

	trx->lock.lock_heap = mem_heap_create_typed(
		256, MEM_HEAP_FOR_LOCK_HEAP);

	trx->search_latch_timeout = BTR_SEA_TIMEOUT;

	trx->global_read_view_heap = mem_heap_create(256);

	trx->xid.formatID = -1;

	trx->op_info = "";

	/* Remember to free the vector explicitly in trx_free(). */
	trx->autoinc_locks = ib_vector_create(
		mem_heap_create(sizeof(ib_vector_t) + sizeof(void*) * 4), 4);

	return(trx);
}

/********************************************************************//**
Creates a transaction object for background operations by the master thread.
@return	own: transaction object */
UNIV_INTERN
trx_t*
trx_allocate_for_background(void)
/*=============================*/
{
	trx_t*	trx;

	trx = trx_create();
	trx->sess = trx_dummy_sess;

	return(trx);
}

/********************************************************************//**
Creates a transaction object for MySQL.
@return	own: transaction object */
UNIV_INTERN
trx_t*
trx_allocate_for_mysql(void)
/*========================*/
{
	trx_t*	trx;

	trx = trx_allocate_for_background();

	rw_lock_x_lock(&trx_sys->lock);

	++trx_n_mysql_transactions;

	ut_d(trx->in_mysql_trx_list = TRUE);
	UT_LIST_ADD_FIRST(mysql_trx_list, trx_sys->mysql_trx_list, trx);

	rw_lock_x_unlock(&trx_sys->lock);

	trx->mysql_thread_id = os_thread_get_curr_id();

	trx->mysql_process_no = os_proc_get_number();

	return(trx);
}

/********************************************************************//**
Releases the search latch if trx has reserved it. */
UNIV_INTERN
void
trx_search_latch_release_if_reserved(
/*=================================*/
	trx_t*	   trx) /*!< in: transaction */
{
	if (trx->has_search_latch) {
		rw_lock_s_unlock(&btr_search_latch);

		trx->has_search_latch = FALSE;
	}
}

/********************************************************************//**
Frees a transaction object. */
static
void
trx_free(
/*=====*/
	trx_t*	trx)	/*!< in, own: trx object */
{
	if (UNIV_UNLIKELY(trx->declared_to_be_inside_innodb)) {
		ut_print_timestamp(stderr);
		fputs("  InnoDB: Error: Freeing a trx which is declared"
		      " to be processing\n"
		      "InnoDB: inside InnoDB.\n", stderr);
		trx_print(stderr, trx, 600);
		putc('\n', stderr);

		/* This is an error but not a fatal error. We must keep
		the counters like srv_conc_n_threads accurate. */
		srv_conc_force_exit_innodb(trx);
	}

	if (UNIV_UNLIKELY(trx->n_mysql_tables_in_use != 0
			  || trx->mysql_n_tables_locked != 0)) {

		ut_print_timestamp(stderr);
		fprintf(stderr,
			"  InnoDB: Error: MySQL is freeing a thd\n"
			"InnoDB: though trx->n_mysql_tables_in_use is %lu\n"
			"InnoDB: and trx->mysql_n_tables_locked is %lu.\n",
			(ulong)trx->n_mysql_tables_in_use,
			(ulong)trx->mysql_n_tables_locked);
		trx_print(stderr, trx, 600);
		ut_print_buf(stderr, trx, sizeof(trx_t));
		putc('\n', stderr);
	}

	trx_mutex_enter(trx);

	ut_a(trx->magic_n == TRX_MAGIC_N);

	trx->magic_n = 11112222;

	ut_a(trx->state == TRX_STATE_NOT_STARTED);

	mutex_free(&trx->undo_mutex);

	ut_a(trx->insert_undo == NULL);
	ut_a(trx->update_undo == NULL);

	if (trx->undo_no_arr != NULL) {
		trx_undo_arr_free(trx->undo_no_arr);
	}

	ut_a(trx->lock.wait_lock == NULL);
	ut_a(trx->lock.wait_thr == NULL);

	ut_a(!trx->has_search_latch);

	ut_a(trx->dict_operation_lock_mode == 0);

	if (trx->lock.lock_heap) {
		mem_heap_free(trx->lock.lock_heap);
	}

	ut_a(UT_LIST_GET_LEN(trx->lock.trx_locks) == 0);

	if (trx->global_read_view_heap) {
		mem_heap_free(trx->global_read_view_heap);
	}

	trx->global_read_view = NULL;

	ut_a(trx->read_view == NULL);

	ut_a(ib_vector_is_empty(trx->autoinc_locks));
	/* We allocated a dedicated heap for the vector. */
	ib_vector_free(trx->autoinc_locks);

	trx_mutex_exit(trx);

	mutex_free(&trx->mutex);

	mem_free(trx);
}

/********************************************************************//**
Frees a transaction object of a background operation of the master thread. */
UNIV_INTERN
void
trx_free_for_background(
/*====================*/
	trx_t*	trx)	/*!< in, own: trx object */
{
	trx_free(trx);
}

/********************************************************************//**
Frees a transaction object for MySQL. */
UNIV_INTERN
void
trx_free_for_mysql(
/*===============*/
	trx_t*	trx)	/*!< in, own: trx object */
{
	rw_lock_x_lock(&trx_sys->lock);

	ut_ad(trx->in_mysql_trx_list);
	ut_d(trx->in_mysql_trx_list = FALSE);
	UT_LIST_REMOVE(mysql_trx_list, trx_sys->mysql_trx_list, trx);

	ut_ad(trx_sys_validate_trx_list());

	--trx_n_mysql_transactions;

	rw_lock_x_unlock(&trx_sys->lock);

	trx_free_for_background(trx);
}

/****************************************************************//**
Inserts the trx handle in the trx system trx list in the right position.
The list is sorted on the trx id so that the biggest id is at the list
start. This function is used at the database startup to insert incomplete
transactions to the list. */
static
void
trx_list_insert_ordered(
/*====================*/
	trx_t*	trx)	/*!< in: trx handle */
{
	trx_t*	trx2;

	ut_a(srv_is_being_started);
	ut_ad(!trx->in_trx_list);
	ut_ad(trx->state != TRX_STATE_NOT_STARTED);
	ut_ad(trx->is_recovered);

	for (trx2 = UT_LIST_GET_FIRST(trx_sys->trx_list);
	     trx2 != NULL;
	     trx2 = UT_LIST_GET_NEXT(trx_list, trx2)) {

		ut_ad(trx2->in_trx_list);

		if (trx->id >= trx2->id) {

			ut_ad(trx->id > trx2->id);
			break;
		}
	}

	if (trx2 != NULL) {
		trx2 = UT_LIST_GET_PREV(trx_list, trx2);

		if (trx2 == NULL) {
			UT_LIST_ADD_FIRST(trx_list, trx_sys->trx_list, trx);
		} else {
			UT_LIST_INSERT_AFTER(trx_list, trx_sys->trx_list,
					     trx2, trx);
		}
	} else {
		UT_LIST_ADD_LAST(trx_list, trx_sys->trx_list, trx);
	}

	ut_ad(!trx->in_trx_list);
	ut_d(trx->in_trx_list = TRUE);
}

/****************************************************************//**
Resurrect the transactions that were doing inserts the time of the
crash, they need to be undone.
@return trx_t instance  */
static
trx_t*
trx_resurrect_insert(
/*=================*/
	trx_undo_t*	undo,		/*!< in: entry to UNDO */
	trx_rseg_t*	rseg)		/*!< in: rollback segment */
{
	trx_t*		trx;

	trx = trx_allocate_for_background();

	trx->rseg = rseg;
	trx->xid = undo->xid;
	trx->id = undo->trx_id;
	trx->insert_undo = undo;
	trx->is_recovered = TRUE;

	/* This is single-threaded startup code, we do not need the
	protection of trx->mutex or trx_sys->lock here. */

	if (undo->state != TRX_UNDO_ACTIVE) {

		/* Prepared transactions are left in the prepared state
		waiting for a commit or abort decision from MySQL */

		if (undo->state == TRX_UNDO_PREPARED) {

			fprintf(stderr,
				"InnoDB: Transaction " TRX_ID_FMT " was in the"
				" XA prepared state.\n", trx->id);

			if (srv_force_recovery == 0) {

				trx->state = TRX_STATE_PREPARED;
			} else {
				fprintf(stderr,
					"InnoDB: Since innodb_force_recovery"
					" > 0, we will rollback it anyway.\n");

				trx->state = TRX_STATE_ACTIVE;
			}
		} else {
			trx->state = TRX_STATE_COMMITTED_IN_MEMORY;
		}

		/* We give a dummy value for the trx no; this should have no
		relevance since purge is not interested in committed
		transaction numbers, unless they are in the history
		list, in which case it looks the number from the disk based
		undo log structure */

		trx->no = trx->id;
	} else {
		trx->state = TRX_STATE_ACTIVE;

		/* A running transaction always has the number
		field inited to IB_ULONGLONG_MAX */

		trx->no = IB_ULONGLONG_MAX;
	}

	if (undo->dict_operation) {
		trx_set_dict_operation(trx, TRX_DICT_OP_TABLE);
		trx->table_id = undo->table_id;
	}

	if (!undo->empty) {
		trx->undo_no = undo->top_undo_no + 1;
	}

	return(trx);
}

/****************************************************************//**
Prepared transactions are left in the prepared state waiting for a
commit or abort decision from MySQL */
static
void
trx_resurrect_update_in_prepared_state(
/*===================================*/
	trx_t*			trx,	/*!< in,out: transaction */
	const trx_undo_t*	undo)	/*!< in: update UNDO record */
{
	/* This is single-threaded startup code, we do not need the
	protection of trx->mutex or trx_sys->lock here. */

	if (undo->state == TRX_UNDO_PREPARED) {
		fprintf(stderr,
			"InnoDB: Transaction " TRX_ID_FMT 
			" was in the XA prepared state.\n", trx->id);

		if (srv_force_recovery == 0) {

			trx->state = TRX_STATE_PREPARED;
		} else {
			fprintf(stderr,
				"InnoDB: Since innodb_force_recovery"
				" > 0, we will rollback it anyway.\n");

			trx->state = TRX_STATE_ACTIVE;
		}
	} else {
		trx->state = TRX_STATE_COMMITTED_IN_MEMORY;
	}
}

/****************************************************************//**
Resurrect the transactions that were doing updates the time of the
crash, they need to be undone. */
static
void
trx_resurrect_update(
/*=================*/
	trx_t*		trx,	/*!< in/out: transaction */
	trx_undo_t*	undo,	/*!< in/out: update UNDO record */
	trx_rseg_t*	rseg)	/*!< in/out: rollback segment */
{
	trx->rseg = rseg;
	trx->xid = undo->xid;
	trx->id = undo->trx_id;
	trx->update_undo = undo;
	trx->is_recovered = TRUE;

	/* This is single-threaded startup code, we do not need the
	protection of trx->mutex or trx_sys->lock here. */

	if (undo->state != TRX_UNDO_ACTIVE) {
		trx_resurrect_update_in_prepared_state(trx, undo);

		/* We give a dummy value for the trx number */

		trx->no = trx->id;

	} else {
		trx->state = TRX_STATE_ACTIVE;

		/* A running transaction always has the number field inited to
		IB_ULONGLONG_MAX */

		trx->no = IB_ULONGLONG_MAX;
	}

	if (undo->dict_operation) {
		trx_set_dict_operation(trx, TRX_DICT_OP_TABLE);
		trx->table_id = undo->table_id;
	}

	if (!undo->empty && undo->top_undo_no >= trx->undo_no) {

		trx->undo_no = undo->top_undo_no + 1;
	}
}

/****************************************************************//**
Creates trx objects for transactions and initializes the trx list of
trx_sys at database start. Rollback segment and undo log lists must
already exist when this function is called, because the lists of
transactions to be rolled back or cleaned up are built based on the
undo log lists. */
UNIV_INTERN
void
trx_lists_init_at_db_start(void)
/*============================*/
{
	ulint		i;

	ut_a(srv_is_being_started);

	UT_LIST_INIT(trx_sys->trx_list);

	/* Look from the rollback segments if there exist undo logs for
	transactions */

	for (i = 0; i < TRX_SYS_N_RSEGS; ++i) {
		trx_undo_t*	undo;
		trx_rseg_t*	rseg;

		rseg = trx_sys->rseg_array[i];

		if (rseg == NULL) {
			continue;
		}

		/* Ressurrect transactions that were doing inserts. */
		for (undo = UT_LIST_GET_FIRST(rseg->insert_undo_list);
		     undo != NULL;
		     undo = UT_LIST_GET_NEXT(undo_list, undo)) {
			trx_t*	trx;

			trx = trx_resurrect_insert(undo, rseg);

			trx_list_insert_ordered(trx);
		}

		/* Ressurrect transactions that were doing updates. */
		for (undo = UT_LIST_GET_FIRST(rseg->update_undo_list);
		     undo != NULL;
		     undo = UT_LIST_GET_NEXT(undo_list, undo)) {
			trx_t*	trx;
			ibool	trx_created;

			/* Check the trx_sys->trx_list first. */
			rw_lock_s_lock(&trx_sys->lock);
			trx = trx_get_on_id(undo->trx_id);
			rw_lock_s_unlock(&trx_sys->lock);

			if (trx == NULL) {
				trx = trx_allocate_for_background();
				trx_created = TRUE;
			} else {
				trx_created = FALSE;
			}

			trx_resurrect_update(trx, undo, rseg);

			if (trx_created) {
				trx_list_insert_ordered(trx);
			}
		}
	}
}

/******************************************************************//**
Assigns a rollback segment to a transaction in a round-robin fashion.
Skips the SYSTEM rollback segment if another is available.
@return	assigned rollback segment */
UNIV_INLINE
trx_rseg_t*
trx_assign_rseg(void)
/*=================*/
{
	ulint		i;
	trx_rseg_t*	rseg;

	/* This breaks true round robin but that should be OK.
	Our aim is to reduce the contention of the trx_sys_t::lock.  */
	i = trx_sys->latest_rseg;
	i %= TRX_SYS_N_RSEGS;

	do {
		rseg = trx_sys->rseg_array[i];
		ut_a(rseg == NULL || i == rseg->id);

		i = (rseg == NULL) ? 0 : i + 1;

	} while (rseg == NULL);

	rw_lock_x_lock(&trx_sys->lock);

	trx_sys->latest_rseg = i % TRX_SYS_N_RSEGS;

	ut_ad(trx_sys_validate_trx_list());

	rw_lock_x_unlock(&trx_sys->lock);

	return(rseg);
}

/****************************************************************//**
Starts a transaction. */
static
void
trx_start_low(
/*==========*/
	trx_t*	trx)	/*!< in: transaction */
{
	trx_rseg_t*	rseg;

	ut_ad(trx->rseg == NULL);

	ut_ad(trx_state_eq(trx, TRX_STATE_NOT_STARTED));
	ut_ad(!trx->is_recovered);
	ut_ad(UT_LIST_GET_LEN(trx->lock.trx_locks) == 0);

	/* The initial value for trx->no: IB_ULONGLONG_MAX is used in
	read_view_open_now: */

	rseg = trx_assign_rseg();

	trx->no = IB_ULONGLONG_MAX;

	trx->start_time = ut_time();

	ut_a(trx->rseg == NULL);
	trx->rseg = rseg;

	rw_lock_x_lock(&trx_sys->lock);

	/* If this transaction came from trx_allocate_for_mysql(),
	trx->in_mysql_trx_list would hold. In that case, the trx->state
	change must be protected by the trx_sys->lock, so that
	lock_print_info_all_transactions() will have a consistent view. */
	trx->state = TRX_STATE_ACTIVE;

	trx->id = trx_sys_get_new_trx_id();

	UT_LIST_ADD_FIRST(trx_list, trx_sys->trx_list, trx);
	ut_ad(!trx->in_trx_list);
	ut_d(trx->in_trx_list = TRUE);

	ut_ad(trx_sys_validate_trx_list());

	rw_lock_x_unlock(&trx_sys->lock);

	MONITOR_INC(MONITOR_TRX_ACTIVE);
}

/****************************************************************//**
Commits a transaction. */
UNIV_INTERN
void
trx_commit(
/*=======*/
	trx_t*	trx)	/*!< in: transaction */
{
	mtr_t			mtr;
	trx_named_savept_t*	savep;
	ib_uint64_t		lsn = 0;
	page_t*			update_hdr_page;

	ut_ad(trx->in_trx_list);
	ut_ad(!trx_state_eq(trx, TRX_STATE_COMMITTED_IN_MEMORY));

	if (trx->insert_undo != NULL || trx->update_undo != NULL) {
		trx_rseg_t*	rseg;

		rseg = trx->rseg;

		mtr_start(&mtr);

		/* Change the undo log segment states from TRX_UNDO_ACTIVE
		to some other state: these modifications to the file data
		structure define the transaction as committed in the file
		based world, at the serialization point of the log sequence
		number lsn obtained below. */

		mutex_enter(&rseg->mutex);

		if (trx->insert_undo != NULL) {
			trx_undo_set_state_at_finish(trx->insert_undo, &mtr);
		}

		if (trx->update_undo != NULL) {

			rw_lock_x_lock(&trx_sys->lock);

			trx->no = trx_sys_get_new_trx_id();

			rw_lock_x_unlock(&trx_sys->lock);

			/* It is not necessary to obtain trx->undo_mutex here
			because only a single OS thread is allowed to do the
			transaction commit for this transaction. */

			update_hdr_page = trx_undo_set_state_at_finish(
				trx->update_undo, &mtr);

			/* We have to do the cleanup for the update log while
			holding the rseg mutex because update log headers
			have to be put to the history list in the order of
			the trx number. */

			trx_undo_update_cleanup(trx, update_hdr_page, &mtr);
		}

		mutex_exit(&rseg->mutex);

		/* Update the latest MySQL binlog name and offset info
		in trx sys header if MySQL binlogging is on or the database
		server is a MySQL replication slave */

		if (trx->mysql_log_file_name
		    && trx->mysql_log_file_name[0] != '\0') {
			trx_sys_update_mysql_binlog_offset(
				trx->mysql_log_file_name,
				trx->mysql_log_offset,
				TRX_SYS_MYSQL_LOG_INFO, &mtr);
			trx->mysql_log_file_name = NULL;
		}

		/* The following call commits the mini-transaction, making the
		whole transaction committed in the file-based world, at this
		log sequence number. The transaction becomes 'durable' when
		we write the log to disk, but in the logical sense the commit
		in the file-based data structures (undo logs etc.) happens
		here.

		NOTE that transaction numbers, which are assigned only to
		transactions with an update undo log, do not necessarily come
		in exactly the same order as commit lsn's, if the transactions
		have different rollback segments. To get exactly the same
		order we should hold the trx sys mutex up to this point.
		However, if a transaction T2 is able to see modifications
		made by a transaction T1, T2 will always get a bigger
		transaction number and a bigger commit lsn than T1. */

		/*--------------*/
		mtr_commit(&mtr);
		/*--------------*/
		lsn = mtr.end_lsn;
	}

	trx->must_flush_log_later = FALSE;

	lock_trx_release_locks(trx);

	/* Remove the transaction from the list of active transactions
	now that it no longer holds any user locks. */
	rw_lock_x_lock(&trx_sys->lock);
	ut_ad(trx->in_trx_list);
	ut_d(trx->in_trx_list = FALSE);
	UT_LIST_REMOVE(trx_list, trx_sys->trx_list, trx);
	/* If this transaction came from trx_allocate_for_mysql(),
	trx->in_mysql_trx_list would hold. In that case, the
	trx->state change must be protected by trx_sys->lock, so that
	lock_print_info_all_transactions() will have a consistent view. */
	trx->state = TRX_STATE_NOT_STARTED;
	ut_ad(trx_sys_validate_trx_list());
	rw_lock_x_unlock(&trx_sys->lock);

	if (trx->global_read_view != NULL) {
		read_view_remove(trx->global_read_view);

		mem_heap_empty(trx->global_read_view_heap);

		trx->global_read_view = NULL;
	}

	trx->read_view = NULL;

	if (lsn) {
		if (trx->insert_undo != NULL) {

			trx_undo_insert_cleanup(trx);
		}

		/* NOTE that we could possibly make a group commit more
		efficient here: call os_thread_yield here to allow also other
		trxs to come to commit! */

		/*-------------------------------------*/

		/* Depending on the my.cnf options, we may now write the log
		buffer to the log files, making the transaction durable if
		the OS does not crash. We may also flush the log files to
		disk, making the transaction durable also at an OS crash or a
		power outage.

		The idea in InnoDB's group commit is that a group of
		transactions gather behind a trx doing a physical disk write
		to log files, and when that physical write has been completed,
		one of those transactions does a write which commits the whole
		group. Note that this group commit will only bring benefit if
		there are > 2 users in the database. Then at least 2 users can
		gather behind one doing the physical log write to disk.

		If we are calling trx_commit() under prepare_commit_mutex, we
		will delay possible log write and flush to a separate function
		trx_commit_complete_for_mysql(), which is only called when the
		thread has released the mutex. This is to make the
		group commit algorithm to work. Otherwise, the prepare_commit
		mutex would serialize all commits and prevent a group of
		transactions from gathering. */

		if (trx->flush_log_later) {
			/* Do nothing yet */
			trx->must_flush_log_later = TRUE;
		} else if (srv_flush_log_at_trx_commit == 0) {
			/* Do nothing */
		} else if (srv_flush_log_at_trx_commit == 1) {
			if (srv_unix_file_flush_method == SRV_UNIX_NOSYNC) {
				/* Write the log but do not flush it to disk */

				log_write_up_to(lsn, LOG_WAIT_ONE_GROUP,
						FALSE);
			} else {
				/* Write the log to the log files AND flush
				them to disk */

				log_write_up_to(lsn, LOG_WAIT_ONE_GROUP, TRUE);
			}
		} else if (srv_flush_log_at_trx_commit == 2) {

			/* Write the log but do not flush it to disk */

			log_write_up_to(lsn, LOG_WAIT_ONE_GROUP, FALSE);
		} else {
			ut_error;
		}

		trx->commit_lsn = lsn;
	}

	/* Free all savepoints, starting from the first. */
	savep = UT_LIST_GET_FIRST(trx->trx_savepoints);
	trx_roll_savepoints_free(trx, savep);

	trx->rseg = NULL;
	trx->undo_no = 0;
	trx->last_sql_stat_start.least_undo_no = 0;

	ut_ad(trx->lock.wait_thr == NULL);
	ut_ad(UT_LIST_GET_LEN(trx->lock.trx_locks) == 0);
	ut_ad(!trx->in_trx_list);
	/* trx->in_mysql_trx_list would hold between
	trx_allocate_for_mysql() and trx_free_for_mysql(). It does not
	hold for recovered transactions or system transactions. */
}

/****************************************************************//**
Cleans up a transaction at database startup. The cleanup is needed if
the transaction already got to the middle of a commit when the database
crashed, and we cannot roll it back. */
UNIV_INTERN
void
trx_cleanup_at_db_startup(
/*======================*/
	trx_t*	trx)	/*!< in: transaction */
{
	ut_ad(trx->is_recovered);

	if (trx->insert_undo != NULL) {

		trx_undo_insert_cleanup(trx);
	}

	trx->rseg = NULL;
	trx->undo_no = 0;
	trx->last_sql_stat_start.least_undo_no = 0;

	rw_lock_x_lock(&trx_sys->lock);

	UT_LIST_REMOVE(trx_list, trx_sys->trx_list, trx);
	ut_ad(trx->in_trx_list);
	ut_d(trx->in_trx_list = FALSE);

	rw_lock_x_unlock(&trx_sys->lock);

	/* Change the transaction state without mutex protection, now
	that it no longer is in the trx_list. Recovered transactions
	are never placed in the mysql_trx_list. */
	ut_ad(trx->is_recovered);
	ut_ad(!trx->in_trx_list);
	ut_ad(!trx->in_mysql_trx_list);
	trx->state = TRX_STATE_NOT_STARTED;
}

/********************************************************************//**
Assigns a read view for a consistent read query. All the consistent reads
within the same transaction will get the same read view, which is created
when this function is first called for a new started transaction.
@return	consistent read view */
UNIV_INTERN
read_view_t*
trx_assign_read_view(
/*=================*/
	trx_t*	trx)	/*!< in: active transaction */
{
	ut_ad(trx->state == TRX_STATE_ACTIVE);

	if (trx->read_view != NULL) {
		return(trx->read_view);
	}

	if (!trx->read_view) {

		trx->read_view = read_view_open_now(
			trx->id, trx->global_read_view_heap);

		trx->global_read_view = trx->read_view;
	}

	return(trx->read_view);
}

/****************************************************************//**
Prepares a transaction for commit/rollback. */
UNIV_INTERN
void
trx_commit_or_rollback_prepare(
/*===========================*/
	trx_t*	trx)		/*!< in/out: transaction */
{
	/* We are reading trx->state without holding trx_sys->lock
	here, because the commit or rollback should be invoked for a
	running (or recovered prepared) transaction that is associated
	with the current thread. */

	switch (trx->state) {
	case TRX_STATE_NOT_STARTED:
		trx_start_low(trx);
		/* fall through */
	case TRX_STATE_ACTIVE:
	case TRX_STATE_PREPARED:
		/* If the trx is in a lock wait state, moves the waiting
		query thread to the suspended state */

		if (trx->lock.que_state == TRX_QUE_LOCK_WAIT) {

			ut_a(trx->lock.wait_thr != NULL);
			trx->lock.wait_thr->state = QUE_THR_SUSPENDED;
			trx->lock.wait_thr = NULL;

			trx->lock.que_state = TRX_QUE_RUNNING;
		}

		ut_a(trx->lock.n_active_thrs == 1);
		return;
	case TRX_STATE_COMMITTED_IN_MEMORY:
		break;
	}

	ut_error;
}

/*********************************************************************//**
Creates a commit command node struct.
@return	own: commit node struct */
UNIV_INTERN
commit_node_t*
trx_commit_node_create(
/*===================*/
	mem_heap_t*	heap)	/*!< in: mem heap where created */
{
	commit_node_t*	node;

	node = mem_heap_alloc(heap, sizeof(*node));
	node->common.type  = QUE_NODE_COMMIT;
	node->state = COMMIT_NODE_SEND;

	return(node);
}

/***********************************************************//**
Performs an execution step for a commit type node in a query graph.
@return	query thread to run next, or NULL */
UNIV_INTERN
que_thr_t*
trx_commit_step(
/*============*/
	que_thr_t*	thr)	/*!< in: query thread */
{
	commit_node_t*	node;

	node = thr->run_node;

	ut_ad(que_node_get_type(node) == QUE_NODE_COMMIT);

	if (thr->prev_node == que_node_get_parent(node)) {
		node->state = COMMIT_NODE_SEND;
	}

	if (node->state == COMMIT_NODE_SEND) {
		trx_t*	trx;

		node->state = COMMIT_NODE_WAIT;

		trx = thr_get_trx(thr);

		ut_a(trx->lock.wait_thr == NULL);
		ut_a(trx->lock.que_state != TRX_QUE_LOCK_WAIT);

		trx_commit_or_rollback_prepare(trx);

		trx->lock.que_state = TRX_QUE_COMMITTING;

		trx_commit(trx);

		ut_ad(trx->lock.wait_thr == NULL);

		trx->lock.que_state = TRX_QUE_RUNNING;

		thr = NULL;
	} else {
		ut_ad(node->state == COMMIT_NODE_WAIT);

		node->state = COMMIT_NODE_SEND;

		thr->run_node = que_node_get_parent(node);
	}

	return(thr);
}

/**********************************************************************//**
Does the transaction commit for MySQL.
@return	DB_SUCCESS or error number */
UNIV_INTERN
ulint
trx_commit_for_mysql(
/*=================*/
	trx_t*	trx)	/*!< in/out: transaction */
{
	/* Because we do not do the commit by sending an Innobase
	sig to the transaction, we must here make sure that trx has been
	started. */

	ut_a(trx);

	switch (trx->state) {
	case TRX_STATE_NOT_STARTED:
		/* Update the info whether we should skip XA steps that eat
		CPU time.

		For the duration of the transaction trx->support_xa is
		not reread from thd so any changes in the value take
		effect in the next transaction. This is to avoid a
		scenario where some undo log records generated by a
		transaction contain XA information and other undo log
		records, generated by the same transaction do not. */
		trx->support_xa = thd_supports_xa(trx->mysql_thd);

		trx_start_low(trx);
		/* fall through */
	case TRX_STATE_ACTIVE:
	case TRX_STATE_PREPARED:
		trx->op_info = "committing";
		trx_commit(trx);
		MONITOR_INC(MONITOR_TRX_COMMIT);
		MONITOR_DEC(MONITOR_TRX_ACTIVE);
		trx->op_info = "";
		return(DB_SUCCESS);
	case TRX_STATE_COMMITTED_IN_MEMORY:
		break;
	}
	ut_error;
	return(DB_CORRUPTION);
}

/**********************************************************************//**
If required, flushes the log to disk if we called trx_commit_for_mysql()
with trx->flush_log_later == TRUE.
@return	0 or error number */
UNIV_INTERN
ulint
trx_commit_complete_for_mysql(
/*==========================*/
	trx_t*	trx)	/*!< in: trx handle */
{
	ib_uint64_t	lsn	= trx->commit_lsn;

	ut_a(trx);

	trx->op_info = "flushing log";

	if (!trx->must_flush_log_later) {
		/* Do nothing */
	} else if (srv_flush_log_at_trx_commit == 0) {
		/* Do nothing */
	} else if (srv_flush_log_at_trx_commit == 1) {
		if (srv_unix_file_flush_method == SRV_UNIX_NOSYNC) {
			/* Write the log but do not flush it to disk */

			log_write_up_to(lsn, LOG_WAIT_ONE_GROUP, FALSE);
		} else {
			/* Write the log to the log files AND flush them to
			disk */

			log_write_up_to(lsn, LOG_WAIT_ONE_GROUP, TRUE);
		}
	} else if (srv_flush_log_at_trx_commit == 2) {

		/* Write the log but do not flush it to disk */

		log_write_up_to(lsn, LOG_WAIT_ONE_GROUP, FALSE);
	} else {
		ut_error;
	}

	trx->must_flush_log_later = FALSE;

	trx->op_info = "";

	return(0);
}

/**********************************************************************//**
Marks the latest SQL statement ended. */
UNIV_INTERN
void
trx_mark_sql_stat_end(
/*==================*/
	trx_t*	trx)	/*!< in: trx handle */
{
	ut_a(trx);

	switch (trx->state) {
	case TRX_STATE_PREPARED:
	case TRX_STATE_COMMITTED_IN_MEMORY:
		break;
	case TRX_STATE_NOT_STARTED:
		trx->undo_no = 0;
		/* fall through */
	case TRX_STATE_ACTIVE:
		trx->last_sql_stat_start.least_undo_no = trx->undo_no;
		return;
	}

	ut_error;
}

/**********************************************************************//**
Prints info about a transaction.
Caller must hold trx_sys->lock. */
UNIV_INTERN
void
trx_print_low(
/*==========*/
	FILE*		f,
			/*!< in: output stream */
	const trx_t*	trx,
			/*!< in: transaction */
	ulint		max_query_len,
			/*!< in: max query length to print,
			or 0 to use the default max length */
	ulint		n_lock_rec,
			/*!< in: lock_number_of_rows_locked(&trx->lock) */
	ulint		n_lock_struct,
			/*!< in: length of trx->lock.trx_locks */
	ulint		heap_size)
			/*!< in: mem_heap_get_size(trx->lock.lock_heap) */
{
	ibool		newline;
	const char*	op_info;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(rw_lock_own(&trx_sys->lock, RW_LOCK_SHARED));
#endif /* UNIV_SYNC_DEBUG */

	fprintf(f, "TRANSACTION " TRX_ID_FMT, (ullint) trx->id);

	/* trx->state cannot change from or to NOT_STARTED while we
	are holding the trx_sys->lock. It may change from ACTIVE to
	PREPARED or COMMITTED. */
	switch (trx->state) {
	case TRX_STATE_NOT_STARTED:
		fputs(", not started", f);
		goto state_ok;
	case TRX_STATE_ACTIVE:
		fprintf(f, ", ACTIVE %lu sec",
			(ulong)difftime(time(NULL), trx->start_time));
		goto state_ok;
	case TRX_STATE_PREPARED:
		fprintf(f, ", ACTIVE (PREPARED) %lu sec",
			(ulong)difftime(time(NULL), trx->start_time));
		goto state_ok;
	case TRX_STATE_COMMITTED_IN_MEMORY:
		fputs(", COMMITTED IN MEMORY", f);
		goto state_ok;
	}
	fprintf(f, ", state %lu", (ulong) trx->state);
	ut_ad(0);
state_ok:
#ifdef UNIV_LINUX
	fprintf(f, ", process no %lu", trx->mysql_process_no);
#endif
	fprintf(f, ", OS thread id %lu",
		(ulong) os_thread_pf(trx->mysql_thread_id));

	/* prevent a race condition */
	op_info = trx->op_info;

	if (*op_info) {
		putc(' ', f);
		fputs(op_info, f);
	}

	if (trx->is_recovered) {
		fputs(" recovered trx", f);
	}

	if (trx->id == 0) {
		fputs(" purge trx", f);
	}

	if (trx->declared_to_be_inside_innodb) {
		fprintf(f, ", thread declared inside InnoDB %lu",
			(ulong) trx->n_tickets_to_enter_innodb);
	}

	putc('\n', f);

	if (trx->n_mysql_tables_in_use > 0 || trx->mysql_n_tables_locked > 0) {
		fprintf(f, "mysql tables in use %lu, locked %lu\n",
			(ulong) trx->n_mysql_tables_in_use,
			(ulong) trx->mysql_n_tables_locked);
	}

	newline = TRUE;

	/* trx->lock.que_state of an ACTIVE transaction may change
	while we are not holding trx->mutex. We perform a dirty read
	for performance reasons. */

	switch (trx->lock.que_state) {
	case TRX_QUE_RUNNING:
		newline = FALSE; break;
	case TRX_QUE_LOCK_WAIT:
		fputs("LOCK WAIT ", f); break;
	case TRX_QUE_ROLLING_BACK:
		fputs("ROLLING BACK ", f); break;
	case TRX_QUE_COMMITTING:
		fputs("COMMITTING ", f); break;
	default:
		fprintf(f, "que state %lu ", (ulong) trx->lock.que_state);
	}

	if (n_lock_struct > 0 || heap_size > 400) {
		newline = TRUE;

		fprintf(f, "%lu lock struct(s), heap size %lu,"
			" %lu row lock(s)",
			(ulong) n_lock_struct,
			(ulong) heap_size,
			(ulong) n_lock_rec);
	}

	if (trx->has_search_latch) {
		newline = TRUE;
		fputs(", holds adaptive hash latch", f);
	}

	if (trx->undo_no != 0) {
		newline = TRUE;
		fprintf(f, ", undo log entries %llu",
			(ullint) trx->undo_no);
	}

	if (newline) {
		putc('\n', f);
	}

	if (trx->mysql_thd != NULL) {
		innobase_mysql_print_thd(f, trx->mysql_thd, max_query_len);
	}
}

/**********************************************************************//**
Prints info about a transaction.
The caller must hold lock_sys->mutex and trx_sys->lock.
When possible, use trx_print() instead. */
UNIV_INTERN
void
trx_print_latched(
/*==============*/
	FILE*		f,		/*!< in: output stream */
	const trx_t*	trx,		/*!< in: transaction */
	ulint		max_query_len)	/*!< in: max query length to print,
					or 0 to use the default max length */
{
	ut_ad(lock_mutex_own());
#ifdef UNIV_SYNC_DEBUG
	ut_ad(rw_lock_own(&trx_sys->lock, RW_LOCK_SHARED));
#endif /* UNIV_SYNC_DEBUG */

	trx_print_low(f, trx, max_query_len,
		      lock_number_of_rows_locked(&trx->lock),
		      UT_LIST_GET_LEN(trx->lock.trx_locks),
		      mem_heap_get_size(trx->lock.lock_heap));
}

/**********************************************************************//**
Prints info about a transaction.
Acquires and releases lock_sys->mutex and trx_sys->lock. */
UNIV_INTERN
void
trx_print(
/*======*/
	FILE*		f,		/*!< in: output stream */
	const trx_t*	trx,		/*!< in: transaction */
	ulint		max_query_len)	/*!< in: max query length to print,
					or 0 to use the default max length */
{
	ulint	n_lock_rec;
	ulint	n_lock_struct;
	ulint	heap_size;

	lock_mutex_enter();
	n_lock_rec = lock_number_of_rows_locked(&trx->lock);
	n_lock_struct = UT_LIST_GET_LEN(trx->lock.trx_locks);
	heap_size = mem_heap_get_size(trx->lock.lock_heap);
	lock_mutex_exit();

	rw_lock_s_lock(&trx_sys->lock);
	trx_print_low(f, trx, max_query_len,
		      n_lock_rec, n_lock_struct, heap_size);
	rw_lock_s_unlock(&trx_sys->lock);
}

#ifdef UNIV_DEBUG
/**********************************************************************//**
Asserts that a transaction has been started.
The caller must hold trx_sys->lock.
@return TRUE if started */
UNIV_INTERN
ibool
trx_assert_started(
/*===============*/
	const trx_t*	trx)	/*!< in: transaction */
{
# ifdef UNIV_SYNC_DEBUG
	ut_ad(rw_lock_own(&trx_sys->lock, RW_LOCK_SHARED));
# endif /* UNIV_SYNC_DEBUG */

	/* trx->state cannot change from or to NOT_STARTED while we
	are holding trx_sys->lock. It may change from ACTIVE to
	PREPARED. Unless we are holding lock_sys->mutex, it may also
	change to COMMITTED. */

	switch (trx->state) {
	case TRX_STATE_ACTIVE:
	case TRX_STATE_PREPARED:
	case TRX_STATE_COMMITTED_IN_MEMORY:
		ut_ad(trx->in_trx_list);
		return(TRUE);
	case TRX_STATE_NOT_STARTED:
		break;
	}

	ut_error;
	return(FALSE);
}
#endif /* UNIV_DEBUG */

/*******************************************************************//**
Compares the "weight" (or size) of two transactions. Transactions that
have edited non-transactional tables are considered heavier than ones
that have not.
@return	TRUE if weight(a) >= weight(b) */
UNIV_INTERN
ibool
trx_weight_ge(
/*==========*/
	const trx_t*	a,	/*!< in: the first transaction to be compared */
	const trx_t*	b)	/*!< in: the second transaction to be compared */
{
	ibool	a_notrans_edit;
	ibool	b_notrans_edit;

	/* If mysql_thd is NULL for a transaction we assume that it has
	not edited non-transactional tables. */

	a_notrans_edit = a->mysql_thd != NULL
		&& thd_has_edited_nontrans_tables(a->mysql_thd);

	b_notrans_edit = b->mysql_thd != NULL
		&& thd_has_edited_nontrans_tables(b->mysql_thd);

	if (a_notrans_edit != b_notrans_edit) {

		return(a_notrans_edit);
	}

	/* Either both had edited non-transactional tables or both had
	not, we fall back to comparing the number of altered/locked
	rows. */

#if 0
	fprintf(stderr,
		"%s TRX_WEIGHT(a): %lld+%lu, TRX_WEIGHT(b): %lld+%lu\n",
		__func__,
		a->undo_no, UT_LIST_GET_LEN(a->lock.trx_locks),
		b->undo_no, UT_LIST_GET_LEN(b->lock.trx_locks));
#endif

	return(TRX_WEIGHT(a) >= TRX_WEIGHT(b));
}

/****************************************************************//**
Prepares a transaction. */
static
void
trx_prepare(
/*========*/
	trx_t*	trx)	/*!< in: transaction */
{
	trx_rseg_t*	rseg;
	ib_uint64_t	lsn		= 0;
	mtr_t		mtr;

	rseg = trx->rseg;

	if (trx->insert_undo != NULL || trx->update_undo != NULL) {

		trx_mutex_exit(trx);

		mtr_start(&mtr);

		/* Change the undo log segment states from TRX_UNDO_ACTIVE
		to TRX_UNDO_PREPARED: these modifications to the file data
		structure define the transaction as prepared in the
		file-based world, at the serialization point of lsn. */

		mutex_enter(&rseg->mutex);

		if (trx->insert_undo != NULL) {

			/* It is not necessary to obtain trx->undo_mutex here
			because only a single OS thread is allowed to do the
			transaction prepare for this transaction. */

			trx_undo_set_state_at_prepare(trx, trx->insert_undo,
						      &mtr);
		}

		if (trx->update_undo) {
			trx_undo_set_state_at_prepare(
				trx, trx->update_undo, &mtr);
		}

		mutex_exit(&rseg->mutex);

		/*--------------*/
		mtr_commit(&mtr);	/* This mtr commit makes the
					transaction prepared in the file-based
					world */
		/*--------------*/
		lsn = mtr.end_lsn;

		trx_mutex_enter(trx);
	}

	ut_ad(trx_mutex_own(trx));

	/* Note: This state change is only covered by the trx_t::mutex and
	not the trx_sys_t::lock. */

	/*--------------------------------------*/
	trx->state = TRX_STATE_PREPARED;
	/*--------------------------------------*/

	if (lsn) {
		/* Depending on the my.cnf options, we may now write the log
		buffer to the log files, making the prepared state of the
		transaction durable if the OS does not crash. We may also
		flush the log files to disk, making the prepared state of the
		transaction durable also at an OS crash or a power outage.

		The idea in InnoDB's group prepare is that a group of
		transactions gather behind a trx doing a physical disk write
		to log files, and when that physical write has been completed,
		one of those transactions does a write which prepares the whole
		group. Note that this group prepare will only bring benefit if
		there are > 2 users in the database. Then at least 2 users can
		gather behind one doing the physical log write to disk.

		TODO: find out if MySQL holds some mutex when calling this.
		That would spoil our group prepare algorithm. */

		trx_mutex_exit(trx);

		if (srv_flush_log_at_trx_commit == 0) {
			/* Do nothing */
		} else if (srv_flush_log_at_trx_commit == 1) {
			if (srv_unix_file_flush_method == SRV_UNIX_NOSYNC) {
				/* Write the log but do not flush it to disk */

				log_write_up_to(lsn, LOG_WAIT_ONE_GROUP,
						FALSE);
			} else {
				/* Write the log to the log files AND flush
				them to disk */

				log_write_up_to(lsn, LOG_WAIT_ONE_GROUP, TRUE);
			}
		} else if (srv_flush_log_at_trx_commit == 2) {

			/* Write the log but do not flush it to disk */

			log_write_up_to(lsn, LOG_WAIT_ONE_GROUP, FALSE);
		} else {
			ut_error;
		}

		trx_mutex_enter(trx);
	}
}

/**********************************************************************//**
Does the transaction prepare for MySQL. */
UNIV_INTERN
void
trx_prepare_for_mysql(
/*==================*/
	trx_t*	trx)	/*!< in/out: trx handle */
{
	trx_start_if_not_started_xa(trx);

	trx_mutex_enter(trx);

	trx->op_info = "preparing";

	trx_prepare(trx);

	trx->op_info = "";

	trx_mutex_exit(trx);
}

/**********************************************************************//**
This function is used to find number of prepared transactions and
their transaction objects for a recovery.
@return	number of prepared transactions stored in xid_list */
UNIV_INTERN
int
trx_recover_for_mysql(
/*==================*/
	XID*	xid_list,	/*!< in/out: prepared transactions */
	ulint	len)		/*!< in: number of slots in xid_list */
{
	const trx_t*	trx;
	ulint		count = 0;

	ut_ad(xid_list);
	ut_ad(len);

	/* We should set those transactions which are in the prepared state
	to the xid_list */

	rw_lock_s_lock(&trx_sys->lock);

	for (trx = UT_LIST_GET_FIRST(trx_sys->trx_list);
	     trx != NULL;
	     trx = UT_LIST_GET_NEXT(trx_list, trx)) {

		/* trx->state cannot change from or to NOT_STARTED
		while we are holding the trx_sys->lock. It may change
		to PREPARED, but not if trx->is_recovered. It may also
		change to COMMITTED. */
		if (trx_state_eq(trx, TRX_STATE_PREPARED)) {
			xid_list[count] = trx->xid;

			if (count == 0) {
				ut_print_timestamp(stderr);
				fprintf(stderr,
					"  InnoDB: Starting recovery for"
					" XA transactions...\n");
			}

			ut_print_timestamp(stderr);
			fprintf(stderr,
				"  InnoDB: Transaction " TRX_ID_FMT " in"
				" prepared state after recovery\n",
				(ullint) trx->id);

			ut_print_timestamp(stderr);
			fprintf(stderr,
				"  InnoDB: Transaction contains changes"
				" to %llu rows\n",
				(ullint) trx->undo_no);

			count++;

			if (count == len) {
				break;
			}
		}
	}

	rw_lock_s_unlock(&trx_sys->lock);

	if (count > 0){
		ut_print_timestamp(stderr);
		fprintf(stderr,
			"  InnoDB: %lu transactions in prepared state"
			" after recovery\n",
			(ulong) count);
	}

	return ((int) count);
}

/*******************************************************************//**
This function is used to find one X/Open XA distributed transaction
which is in the prepared state
@return	trx or NULL; note that the trx may have been committed,
unless the caller is holding lock_sys->mutex */
UNIV_INTERN
trx_t*
trx_get_trx_by_xid(
/*===============*/
	const XID*	xid)	/*!< in: X/Open XA transaction identification */
{
	trx_t*	trx;

	if (xid == NULL) {

		return (NULL);
	}

	rw_lock_s_lock(&trx_sys->lock);

	for (trx = UT_LIST_GET_FIRST(trx_sys->trx_list);
	     trx != NULL;
	     trx = UT_LIST_GET_NEXT(trx_list, trx)) {

		/* Compare two X/Open XA transaction id's: their
		length should be the same and binary comparison
		of gtrid_length+bqual_length bytes should be
		the same */

		ut_ad(trx->in_trx_list);

		if (xid->gtrid_length == trx->xid.gtrid_length
		    && xid->bqual_length == trx->xid.bqual_length
		    && memcmp(xid->data, trx->xid.data,
			      xid->gtrid_length + xid->bqual_length) == 0) {
			break;
		}
	}

	if (trx != NULL && !trx_state_eq(trx, TRX_STATE_PREPARED)) {

		trx = NULL;
	}

	rw_lock_s_unlock(&trx_sys->lock);

	return(trx);
}

/*************************************************************//**
Starts the transaction if it is not yet started. */
UNIV_INTERN
void
trx_start_if_not_started_xa(
/*========================*/
	trx_t*	trx)	/*!< in: transaction */
{
	switch (trx->state) {
	case TRX_STATE_NOT_STARTED:

		/* Update the info whether we should skip XA steps
		that eat CPU time.

		For the duration of the transaction trx->support_xa is
		not reread from thd so any changes in the value take
		effect in the next transaction. This is to avoid a
		scenario where some undo generated by a transaction,
		has XA stuff, and other undo, generated by the same
		transaction, doesn't. */
		trx->support_xa = thd_supports_xa(trx->mysql_thd);

		trx_start_low(trx);
		/* fall through */
	case TRX_STATE_ACTIVE:
		return;
	case TRX_STATE_PREPARED:
	case TRX_STATE_COMMITTED_IN_MEMORY:
		break;
	}

	ut_error;
}

/*************************************************************//**
Starts the transaction if it is not yet started. */
UNIV_INTERN
void
trx_start_if_not_started(
/*=====================*/
	trx_t*	trx)	/*!< in: transaction */
{
	switch (trx->state) {
	case TRX_STATE_NOT_STARTED:
		trx_start_low(trx);
		/* fall through */
	case TRX_STATE_ACTIVE:
		return;
	case TRX_STATE_PREPARED:
	case TRX_STATE_COMMITTED_IN_MEMORY:
		break;
	}

	ut_error;

}
