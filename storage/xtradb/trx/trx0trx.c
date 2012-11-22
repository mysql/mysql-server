/*****************************************************************************

Copyright (c) 1996, 2011, Innobase Oy. All Rights Reserved.

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
#include "btr0sea.h"
#include "os0proc.h"
#include "trx0xa.h"
#include "trx0purge.h"
#include "ha_prototypes.h"

/** Dummy session used currently in MySQL interface */
UNIV_INTERN sess_t*		trx_dummy_sess = NULL;

/** Number of transactions currently allocated for MySQL: protected by
the kernel mutex */
UNIV_INTERN ulint	trx_n_mysql_transactions = 0;
/** Number of transactions currently in the XA PREPARED state: protected by
the kernel mutex */
UNIV_INTERN ulint	trx_n_prepared = 0;

#ifdef UNIV_PFS_MUTEX
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
Creates and initializes a transaction object.
@return	own: the transaction */
UNIV_INTERN
trx_t*
trx_create(
/*=======*/
	sess_t*	sess)	/*!< in: session */
{
	trx_t*	trx;

	ut_ad(mutex_own(&kernel_mutex));
	ut_ad(sess);

	trx = mem_alloc(sizeof(trx_t));

	trx->magic_n = TRX_MAGIC_N;

	trx->op_info = "";

	trx->is_purge = 0;
	trx->is_recovered = 0;
	trx->conc_state = TRX_NOT_STARTED;

	trx->is_registered = 0;
	trx->active_commit_ordered = 0;

	trx->start_time = ut_time();

	trx->isolation_level = TRX_ISO_REPEATABLE_READ;

	trx->id = 0;
	trx->no = IB_ULONGLONG_MAX;

	trx->support_xa = TRUE;

	trx->fake_changes = FALSE;

	trx->check_foreigns = TRUE;
	trx->check_unique_secondary = TRUE;

	trx->flush_log_later = FALSE;
	trx->must_flush_log_later = FALSE;

	trx->dict_operation = TRX_DICT_OP_NONE;
	trx->table_id = 0;

	trx->mysql_thd = NULL;
	trx->duplicates = 0;

	trx->n_mysql_tables_in_use = 0;
	trx->mysql_n_tables_locked = 0;

	trx->mysql_log_file_name = NULL;
	trx->mysql_log_offset = 0;
	trx->mysql_master_log_file_name = "";
	trx->mysql_master_log_pos = 0;
	trx->mysql_relay_log_file_name = "";
	trx->mysql_relay_log_pos = 0;

	trx->idle_start = 0;
	trx->last_stmt_start = 0;

	mutex_create(trx_undo_mutex_key, &trx->undo_mutex, SYNC_TRX_UNDO);

	trx->rseg = NULL;

	trx->undo_no = 0;
	trx->last_sql_stat_start.least_undo_no = 0;
	trx->insert_undo = NULL;
	trx->update_undo = NULL;
	trx->undo_no_arr = NULL;

	trx->error_state = DB_SUCCESS;
	trx->error_key_num = 0;
	trx->detailed_error[0] = '\0';

	trx->sess = sess;
	trx->que_state = TRX_QUE_RUNNING;
	trx->n_active_thrs = 0;

	trx->handling_signals = FALSE;

	UT_LIST_INIT(trx->signals);
	UT_LIST_INIT(trx->reply_signals);

	trx->graph = NULL;

	trx->wait_lock = NULL;
	trx->was_chosen_as_deadlock_victim = FALSE;
	UT_LIST_INIT(trx->wait_thrs);

	trx->lock_heap = mem_heap_create_in_buffer(256);
	UT_LIST_INIT(trx->trx_locks);

	UT_LIST_INIT(trx->trx_savepoints);

	trx->dict_operation_lock_mode = 0;
	trx->has_search_latch = FALSE;
	trx->search_latch_timeout = BTR_SEA_TIMEOUT;

	trx->declared_to_be_inside_innodb = FALSE;
	trx->n_tickets_to_enter_innodb = 0;

	trx->global_read_view_heap = mem_heap_create(256);
	trx->global_read_view = NULL;
	trx->read_view = NULL;

	trx->io_reads = 0;
	trx->io_read = 0;
	trx->io_reads_wait_timer = 0;
	trx->lock_que_wait_timer = 0;
	trx->innodb_que_wait_timer = 0;
	trx->distinct_page_access = 0;
	trx->distinct_page_access_hash = NULL;
	trx->take_stats = FALSE;

	/* Set X/Open XA transaction identification to NULL */
	memset(&trx->xid, 0, sizeof(trx->xid));
	trx->xid.formatID = -1;

	trx->n_autoinc_rows = 0;

	/* Remember to free the vector explicitly. */
	trx->autoinc_locks = ib_vector_create(
		mem_heap_create(sizeof(ib_vector_t) + sizeof(void*) * 4), 4);

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

	mutex_enter(&kernel_mutex);

	trx = trx_create(trx_dummy_sess);

	trx_n_mysql_transactions++;

	UT_LIST_ADD_FIRST(mysql_trx_list, trx_sys->mysql_trx_list, trx);

	mutex_exit(&kernel_mutex);

	if (innobase_get_slow_log() && trx->take_stats) {
		trx->distinct_page_access_hash = mem_alloc(DPAH_SIZE);
		memset(trx->distinct_page_access_hash, 0, DPAH_SIZE);
	}

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

	mutex_enter(&kernel_mutex);

	trx = trx_create(trx_dummy_sess);

	mutex_exit(&kernel_mutex);

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
	ulint	i;

	if (trx->has_search_latch) {
		for (i = 0; i < btr_search_index_num; i++) {
			if (trx->has_search_latch & ((ulint)1 << i)) {
				rw_lock_s_unlock(btr_search_latch_part[i]);
			}
		}

		trx->has_search_latch = FALSE;
	}
}

/********************************************************************//**
Frees a transaction object. */
UNIV_INTERN
void
trx_free(
/*=====*/
	trx_t*	trx)	/*!< in, own: trx object */
{
	ut_ad(mutex_own(&kernel_mutex));

	if (trx->declared_to_be_inside_innodb) {
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

	if (trx->n_mysql_tables_in_use != 0
	    || trx->mysql_n_tables_locked != 0) {

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

	ut_a(trx->magic_n == TRX_MAGIC_N);

	trx->magic_n = 11112222;

	ut_a(trx->conc_state == TRX_NOT_STARTED);

	mutex_free(&(trx->undo_mutex));

	ut_a(trx->insert_undo == NULL);
	ut_a(trx->update_undo == NULL);

	if (trx->undo_no_arr) {
		trx_undo_arr_free(trx->undo_no_arr);
	}

	ut_a(UT_LIST_GET_LEN(trx->signals) == 0);
	ut_a(UT_LIST_GET_LEN(trx->reply_signals) == 0);

	ut_a(trx->wait_lock == NULL);
	ut_a(UT_LIST_GET_LEN(trx->wait_thrs) == 0);

	ut_a(!trx->has_search_latch);

	ut_a(trx->dict_operation_lock_mode == 0);

	if (trx->lock_heap) {
		mem_heap_free(trx->lock_heap);
	}

	ut_a(UT_LIST_GET_LEN(trx->trx_locks) == 0);

	if (trx->global_read_view_heap) {
		mem_heap_free(trx->global_read_view_heap);
	}

	trx->global_read_view = NULL;

	ut_a(trx->read_view == NULL);

	ut_a(ib_vector_is_empty(trx->autoinc_locks));
	/* We allocated a dedicated heap for the vector. */
	ib_vector_free(trx->autoinc_locks);

	mem_free(trx);
}

/********************************************************************//**
At shutdown, frees a transaction object that is in the PREPARED state. */
UNIV_INTERN
void
trx_free_prepared(
/*==============*/
	trx_t*	trx)	/*!< in, own: trx object */
{
	ut_ad(mutex_own(&kernel_mutex));
	ut_a(trx->conc_state == TRX_PREPARED);
	ut_a(trx->magic_n == TRX_MAGIC_N);

	/* Prepared transactions are sort of active; they allow
	ROLLBACK and COMMIT operations. Because the system does not
	contain any other transactions than prepared transactions at
	the shutdown stage and because a transaction cannot become
	PREPARED while holding locks, it is safe to release the locks
	held by PREPARED transactions here at shutdown.*/
	lock_release_off_kernel(trx);

	trx_undo_free_prepared(trx);

	mutex_free(&trx->undo_mutex);

	if (trx->undo_no_arr) {
		trx_undo_arr_free(trx->undo_no_arr);
	}

	ut_a(UT_LIST_GET_LEN(trx->signals) == 0);
	ut_a(UT_LIST_GET_LEN(trx->reply_signals) == 0);

	ut_a(trx->wait_lock == NULL);
	ut_a(UT_LIST_GET_LEN(trx->wait_thrs) == 0);

	ut_a(!trx->has_search_latch);

	ut_a(trx->dict_operation_lock_mode == 0);

	if (trx->lock_heap) {
		mem_heap_free(trx->lock_heap);
	}

	if (trx->global_read_view_heap) {
		mem_heap_free(trx->global_read_view_heap);
	}

	ut_a(ib_vector_is_empty(trx->autoinc_locks));
	ib_vector_free(trx->autoinc_locks);

	UT_LIST_REMOVE(trx_list, trx_sys->trx_list, trx);

	mem_free(trx);
}

/********************************************************************//**
Frees a transaction object for MySQL. */
UNIV_INTERN
void
trx_free_for_mysql(
/*===============*/
	trx_t*	trx)	/*!< in, own: trx object */
{
	if (trx->distinct_page_access_hash)
	{
		mem_free(trx->distinct_page_access_hash);
		trx->distinct_page_access_hash= NULL;
	}

	mutex_enter(&kernel_mutex);

	UT_LIST_REMOVE(mysql_trx_list, trx_sys->mysql_trx_list, trx);

	trx_free(trx);

	ut_a(trx_n_mysql_transactions > 0);

	trx_n_mysql_transactions--;

	mutex_exit(&kernel_mutex);
}

/********************************************************************//**
Frees a transaction object of a background operation of the master thread. */
UNIV_INTERN
void
trx_free_for_background(
/*====================*/
	trx_t*	trx)	/*!< in, own: trx object */
{
	if (trx->distinct_page_access_hash)
	{
		mem_free(trx->distinct_page_access_hash);
		trx->distinct_page_access_hash= NULL;
	}

	mutex_enter(&kernel_mutex);

	trx_free(trx);

	mutex_exit(&kernel_mutex);
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

	ut_ad(mutex_own(&kernel_mutex));

	trx2 = UT_LIST_GET_FIRST(trx_sys->trx_list);

	while (trx2 != NULL) {
		if (trx->id >= trx2->id) {

			ut_ad(trx->id > trx2->id);
			break;
		}
		trx2 = UT_LIST_GET_NEXT(trx_list, trx2);
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
	trx_rseg_t*	rseg;
	trx_undo_t*	undo;
	trx_t*		trx;

	ut_ad(mutex_own(&kernel_mutex));
	UT_LIST_INIT(trx_sys->trx_list);

	/* Look from the rollback segments if there exist undo logs for
	transactions */

	rseg = UT_LIST_GET_FIRST(trx_sys->rseg_list);

	while (rseg != NULL) {
		undo = UT_LIST_GET_FIRST(rseg->insert_undo_list);

		while (undo != NULL) {

			trx = trx_create(trx_dummy_sess);

			trx->is_recovered = TRUE;
			trx->id = undo->trx_id;
			trx->xid = undo->xid;
			trx->insert_undo = undo;
			trx->rseg = rseg;

			if (undo->state != TRX_UNDO_ACTIVE) {

				/* Prepared transactions are left in
				the prepared state waiting for a
				commit or abort decision from MySQL */

				if (undo->state == TRX_UNDO_PREPARED) {

					fprintf(stderr,
						"InnoDB: Transaction "
						TRX_ID_FMT
						" was in the"
						" XA prepared state.\n",
						(ullint) trx->id);

					if (srv_force_recovery == 0) {

						trx->conc_state = TRX_PREPARED;
						trx_n_prepared++;
					} else {
						fprintf(stderr,
							"InnoDB: Since"
							" innodb_force_recovery"
							" > 0, we will"
							" rollback it"
							" anyway.\n");

						trx->conc_state = TRX_ACTIVE;
					}
				} else {
					trx->conc_state
						= TRX_COMMITTED_IN_MEMORY;
				}

				/* We give a dummy value for the trx no;
				this should have no relevance since purge
				is not interested in committed transaction
				numbers, unless they are in the history
				list, in which case it looks the number
				from the disk based undo log structure */

				trx->no = trx->id;
			} else {
				trx->conc_state = TRX_ACTIVE;

				/* A running transaction always has the number
				field inited to IB_ULONGLONG_MAX */

				trx->no = IB_ULONGLONG_MAX;
			}

			if (undo->dict_operation) {
				trx_set_dict_operation(
					trx, TRX_DICT_OP_TABLE);
				trx->table_id = undo->table_id;
			}

			if (!undo->empty) {
				trx->undo_no = undo->top_undo_no + 1;
			}

			trx_list_insert_ordered(trx);

			undo = UT_LIST_GET_NEXT(undo_list, undo);
		}

		undo = UT_LIST_GET_FIRST(rseg->update_undo_list);

		while (undo != NULL) {
			trx = trx_get_on_id(undo->trx_id);

			if (NULL == trx) {
				trx = trx_create(trx_dummy_sess);

				trx->is_recovered = TRUE;
				trx->id = undo->trx_id;
				trx->xid = undo->xid;

				if (undo->state != TRX_UNDO_ACTIVE) {

					/* Prepared transactions are left in
					the prepared state waiting for a
					commit or abort decision from MySQL */

					if (undo->state == TRX_UNDO_PREPARED) {
						fprintf(stderr,
							"InnoDB: Transaction "
							TRX_ID_FMT " was in the"
							" XA prepared state.\n",
							(ullint) trx->id);

						if (srv_force_recovery == 0) {

							trx->conc_state
								= TRX_PREPARED;
							trx_n_prepared++;
						} else {
							fprintf(stderr,
								"InnoDB: Since"
								" innodb_force_recovery"
								" > 0, we will"
								" rollback it"
								" anyway.\n");

							trx->conc_state
								= TRX_ACTIVE;
						}
					} else {
						trx->conc_state
							= TRX_COMMITTED_IN_MEMORY;
					}

					/* We give a dummy value for the trx
					number */

					trx->no = trx->id;
				} else {
					trx->conc_state = TRX_ACTIVE;

					/* A running transaction always has
					the number field inited to
					IB_ULONGLONG_MAX */

					trx->no = IB_ULONGLONG_MAX;
				}

				trx->rseg = rseg;
				trx_list_insert_ordered(trx);

				if (undo->dict_operation) {
					trx_set_dict_operation(
						trx, TRX_DICT_OP_TABLE);
					trx->table_id = undo->table_id;
				}
			}

			trx->update_undo = undo;

			if ((!undo->empty)
			    && undo->top_undo_no >= trx->undo_no) {

				trx->undo_no = undo->top_undo_no + 1;
			}

			undo = UT_LIST_GET_NEXT(undo_list, undo);
		}

		rseg = UT_LIST_GET_NEXT(rseg_list, rseg);
	}
}

/******************************************************************//**
Assigns a rollback segment to a transaction in a round-robin fashion.
@return	assigned rollback segment instance */
UNIV_INLINE
trx_rseg_t*
trx_assign_rseg(
/*============*/
	ulint	max_undo_logs)	/*!< in: maximum number of UNDO logs to use */
{
	trx_rseg_t*	rseg = trx_sys->latest_rseg;

	ut_ad(mutex_own(&kernel_mutex));

	rseg = UT_LIST_GET_NEXT(rseg_list, rseg);

	if (rseg == NULL || rseg->id == max_undo_logs - 1) {
		rseg = UT_LIST_GET_FIRST(trx_sys->rseg_list);
	}

	trx_sys->latest_rseg = rseg;

	return(rseg);
}

/****************************************************************//**
Starts a new transaction.
@return	TRUE */
UNIV_INTERN
ibool
trx_start_low(
/*==========*/
	trx_t*	trx,	/*!< in: transaction */
	ulint	rseg_id)/*!< in: rollback segment id; if ULINT_UNDEFINED
			is passed, the system chooses the rollback segment
			automatically in a round-robin fashion */
{
	trx_rseg_t*	rseg;

	ut_ad(mutex_own(&kernel_mutex));
	ut_ad(trx->rseg == NULL);

	if (trx->is_purge) {
		trx->id = 0;
		trx->conc_state = TRX_ACTIVE;
		trx->start_time = time(NULL);

		return(TRUE);
	}

	ut_ad(trx->conc_state != TRX_ACTIVE);

	ut_a(rseg_id == ULINT_UNDEFINED);

	rseg = trx_assign_rseg(srv_rollback_segments);

	trx->id = trx_sys_get_new_trx_id();

	/* The initial value for trx->no: IB_ULONGLONG_MAX is used in
	read_view_open_now: */

	trx->no = IB_ULONGLONG_MAX;

	trx->rseg = rseg;

	trx->conc_state = TRX_ACTIVE;
	trx->start_time = time(NULL);

	UT_LIST_ADD_FIRST(trx_list, trx_sys->trx_list, trx);

	return(TRUE);
}

/****************************************************************//**
Starts a new transaction.
@return	TRUE */
UNIV_INTERN
ibool
trx_start(
/*======*/
	trx_t*	trx,	/*!< in: transaction */
	ulint	rseg_id)/*!< in: rollback segment id; if ULINT_UNDEFINED
			is passed, the system chooses the rollback segment
			automatically in a round-robin fashion */
{
	ibool	ret;

	/* Update the info whether we should skip XA steps that eat CPU time
	For the duration of the transaction trx->support_xa is not reread
	from thd so any changes in the value take effect in the next
	transaction. This is to avoid a scenario where some undo
	generated by a transaction, has XA stuff, and other undo,
	generated by the same transaction, doesn't. */
	trx->support_xa = thd_supports_xa(trx->mysql_thd);

	mutex_enter(&kernel_mutex);

	ret = trx_start_low(trx, rseg_id);

	mutex_exit(&kernel_mutex);

	return(ret);
}

/****************************************************************//**
Set the transaction serialisation number. */
static
void
trx_serialisation_number_get(
/*=========================*/
	trx_t*		trx)	/*!< in: transaction */
{
	trx_rseg_t*	rseg;

	rseg = trx->rseg;

	ut_ad(mutex_own(&rseg->mutex));

	mutex_enter(&kernel_mutex);

	trx->no = trx_sys_get_new_trx_id();

	/* If the rollack segment is not empty then the
	new trx_t::no can't be less than any trx_t::no
	already in the rollback segment. User threads only
	produce events when a rollback segment is empty. */

	if (rseg->last_page_no == FIL_NULL) {
		void*		ptr;
		rseg_queue_t	rseg_queue;

		rseg_queue.rseg = rseg;
		rseg_queue.trx_no = trx->no;

		mutex_enter(&purge_sys->bh_mutex);

		/* This is to reduce the pressure on the kernel mutex,
		though in reality it should make very little (read no)
		difference because this code path is only taken when the
		rbs is empty. */

		mutex_exit(&kernel_mutex);

		ptr = ib_bh_push(purge_sys->ib_bh, &rseg_queue);
		ut_a(ptr);

		mutex_exit(&purge_sys->bh_mutex);
	} else {
		mutex_exit(&kernel_mutex);
	}
}

/****************************************************************//**
Assign the transaction its history serialisation number and write the
update UNDO log record to the assigned rollback segment.
@return the LSN of the UNDO log write. */
static
ib_uint64_t
trx_write_serialisation_history(
/*============================*/
	trx_t*		trx)	/*!< in: transaction */
{
	mtr_t		mtr;
	trx_rseg_t*	rseg;
	trx_sysf_t*	sys_header = NULL;

	ut_ad(!mutex_own(&kernel_mutex));

	rseg = trx->rseg;

	mtr_start(&mtr);

	/* Change the undo log segment states from TRX_UNDO_ACTIVE
	to some other state: these modifications to the file data
	structure define the transaction as committed in the file
	based domain, at the serialization point of the log sequence
	number lsn obtained below. */

	if (trx->update_undo != NULL) {
		page_t*		undo_hdr_page;
		trx_undo_t*	undo = trx->update_undo;

		/* We have to hold the rseg mutex because update
		log headers have to be put to the history list in the
		(serialisation) order of the UNDO trx number. This is
		required for the purge in-memory data structures too. */

		mutex_enter(&rseg->mutex);

		/* Assign the transaction serialisation number and also
		update the purge min binary heap if this is the first
		UNDO log being written to the assigned rollback segment. */

		trx_serialisation_number_get(trx);

		/* It is not necessary to obtain trx->undo_mutex here
		because only a single OS thread is allowed to do the
		transaction commit for this transaction. */

		undo_hdr_page = trx_undo_set_state_at_finish(undo, &mtr);

		trx_undo_update_cleanup(trx, undo_hdr_page, &mtr);
	} else {
		mutex_enter(&rseg->mutex);
	}

	if (trx->insert_undo != NULL) {
		trx_undo_set_state_at_finish(trx->insert_undo, &mtr);
	}

	mutex_exit(&rseg->mutex);

	/* Update the latest MySQL binlog name and offset info
	in trx sys header if MySQL binlogging is on or the database
	server is a MySQL replication slave */

	if (trx->mysql_log_file_name
	    && trx->mysql_log_file_name[0] != '\0') {
		if (!sys_header) {
			sys_header = trx_sysf_get(&mtr);
		}

		trx_sys_update_mysql_binlog_offset(
			sys_header,
			trx->mysql_log_file_name,
			trx->mysql_log_offset,
			TRX_SYS_MYSQL_LOG_INFO, &mtr);

		trx->mysql_log_file_name = NULL;
	}

	if (trx->mysql_master_log_file_name[0] != '\0') {
		/* This database server is a MySQL replication slave */
		if (!sys_header) {
			sys_header = trx_sysf_get(&mtr);
		}

		trx_sys_update_mysql_binlog_offset(
			sys_header,
			trx->mysql_relay_log_file_name,
			trx->mysql_relay_log_pos,
			TRX_SYS_COMMIT_RELAY_LOG_INFO, &mtr);

		trx_sys_update_mysql_binlog_offset(
			sys_header,
			trx->mysql_master_log_file_name,
			trx->mysql_master_log_pos,
			TRX_SYS_COMMIT_MASTER_LOG_INFO, &mtr);

		trx->mysql_master_log_file_name = "";
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
	order we should hold the kernel mutex up to this point,
	adding to the contention of the kernel mutex. However, if
	a transaction T2 is able to see modifications made by
	a transaction T1, T2 will always get a bigger transaction
	number and a bigger commit lsn than T1. */

	/*--------------*/
	mtr_commit(&mtr);
	/*--------------*/

	return(mtr.end_lsn);
}

/****************************************************************//**
Commits a transaction. */
UNIV_INTERN
void
trx_commit_off_kernel(
/*==================*/
	trx_t*	trx)	/*!< in: transaction */
{
	ib_uint64_t	lsn;

	ut_ad(mutex_own(&kernel_mutex));

	trx->must_flush_log_later = FALSE;

	/* If the transaction made any updates then we need to write the
	UNDO logs for the updates to the assigned rollback segment. */

	if (trx->insert_undo != NULL || trx->update_undo != NULL) {
		mutex_exit(&kernel_mutex);

		lsn = trx_write_serialisation_history(trx);

		mutex_enter(&kernel_mutex);
	} else {
		lsn = 0;
	}

	ut_ad(trx->conc_state == TRX_ACTIVE || trx->conc_state == TRX_PREPARED);
	ut_ad(mutex_own(&kernel_mutex));

	if (UNIV_UNLIKELY(trx->conc_state == TRX_PREPARED)) {
		ut_a(trx_n_prepared > 0);
		trx_n_prepared--;
	}

	/* The following assignment makes the transaction committed in memory
	and makes its changes to data visible to other transactions.
	NOTE that there is a small discrepancy from the strict formal
	visibility rules here: a human user of the database can see
	modifications made by another transaction T even before the necessary
	log segment has been flushed to the disk. If the database happens to
	crash before the flush, the user has seen modifications from T which
	will never be a committed transaction. However, any transaction T2
	which sees the modifications of the committing transaction T, and
	which also itself makes modifications to the database, will get an lsn
	larger than the committing transaction T. In the case where the log
	flush fails, and T never gets committed, also T2 will never get
	committed. */

	/*--------------------------------------*/
	trx->conc_state = TRX_COMMITTED_IN_MEMORY;
	/*--------------------------------------*/

	/* If we release kernel_mutex below and we are still doing
	recovery i.e.: back ground rollback thread is still active
	then there is a chance that the rollback thread may see
	this trx as COMMITTED_IN_MEMORY and goes adhead to clean it
	up calling trx_cleanup_at_db_startup(). This can happen
	in the case we are committing a trx here that is left in
	PREPARED state during the crash. Note that commit of the
	rollback of a PREPARED trx happens in the recovery thread
	while the rollback of other transactions happen in the
	background thread. To avoid this race we unconditionally
	unset the is_recovered flag from the trx. */

	trx->is_recovered = FALSE;

	lock_release_off_kernel(trx);

	if (trx->global_read_view) {
		read_view_close(trx->global_read_view);
		mem_heap_empty(trx->global_read_view_heap);
		trx->global_read_view = NULL;
	}

	trx->read_view = NULL;

	if (lsn) {
		ulint	flush_log_at_trx_commit;

		mutex_exit(&kernel_mutex);

		if (trx->insert_undo != NULL) {

			trx_undo_insert_cleanup(trx);
		}

		if (srv_use_global_flush_log_at_trx_commit) {
			flush_log_at_trx_commit = thd_flush_log_at_trx_commit(NULL);
		} else {
			flush_log_at_trx_commit = thd_flush_log_at_trx_commit(trx->mysql_thd);
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
		} else if (flush_log_at_trx_commit == 0) {
			/* Do nothing */
		} else if (flush_log_at_trx_commit == 1) {
			if (srv_unix_file_flush_method == SRV_UNIX_NOSYNC) {
				/* Write the log but do not flush it to disk */

				log_write_up_to(lsn, LOG_WAIT_ONE_GROUP,
						FALSE);
			} else {
				/* Write the log to the log files AND flush
				them to disk */

				log_write_up_to(lsn, LOG_WAIT_ONE_GROUP, TRUE);
			}
		} else if (flush_log_at_trx_commit == 2) {

			/* Write the log but do not flush it to disk */

			log_write_up_to(lsn, LOG_WAIT_ONE_GROUP, FALSE);
		} else {
			ut_error;
		}

		trx->commit_lsn = lsn;

		/*-------------------------------------*/

		mutex_enter(&kernel_mutex);
	}

	/* Free all savepoints */
	trx_roll_free_all_savepoints(trx);

	trx->conc_state = TRX_NOT_STARTED;
	trx->rseg = NULL;
	trx->undo_no = 0;
	trx->last_sql_stat_start.least_undo_no = 0;

	ut_ad(UT_LIST_GET_LEN(trx->wait_thrs) == 0);
	ut_ad(UT_LIST_GET_LEN(trx->trx_locks) == 0);

	UT_LIST_REMOVE(trx_list, trx_sys->trx_list, trx);

	trx->error_state = DB_SUCCESS;
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
	if (trx->insert_undo != NULL) {

		trx_undo_insert_cleanup(trx);
	}

	trx->conc_state = TRX_NOT_STARTED;
	trx->rseg = NULL;
	trx->undo_no = 0;
	trx->last_sql_stat_start.least_undo_no = 0;

	UT_LIST_REMOVE(trx_list, trx_sys->trx_list, trx);
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
	ut_ad(trx->conc_state == TRX_ACTIVE);

	if (trx->read_view) {
		return(trx->read_view);
	}

	mutex_enter(&kernel_mutex);

	if (!trx->read_view) {
		trx->read_view = read_view_open_now(
			trx->id, trx->global_read_view_heap);
		trx->global_read_view = trx->read_view;
	}

	mutex_exit(&kernel_mutex);

	return(trx->read_view);
}

/****************************************************************//**
Commits a transaction. NOTE that the kernel mutex is temporarily released. */
static
void
trx_handle_commit_sig_off_kernel(
/*=============================*/
	trx_t*		trx,		/*!< in: transaction */
	que_thr_t**	next_thr)	/*!< in/out: next query thread to run;
					if the value which is passed in is
					a pointer to a NULL pointer, then the
					calling function can start running
					a new query thread */
{
	trx_sig_t*	sig;
	trx_sig_t*	next_sig;

	ut_ad(mutex_own(&kernel_mutex));

	trx->que_state = TRX_QUE_COMMITTING;

	trx_commit_off_kernel(trx);

	ut_ad(UT_LIST_GET_LEN(trx->wait_thrs) == 0);

	/* Remove all TRX_SIG_COMMIT signals from the signal queue and send
	reply messages to them */

	sig = UT_LIST_GET_FIRST(trx->signals);

	while (sig != NULL) {
		next_sig = UT_LIST_GET_NEXT(signals, sig);

		if (sig->type == TRX_SIG_COMMIT) {

			trx_sig_reply(sig, next_thr);
			trx_sig_remove(trx, sig);
		}

		sig = next_sig;
	}

	trx->que_state = TRX_QUE_RUNNING;
}

/***********************************************************//**
The transaction must be in the TRX_QUE_LOCK_WAIT state. Puts it to
the TRX_QUE_RUNNING state and releases query threads which were
waiting for a lock in the wait_thrs list. */
UNIV_INTERN
void
trx_end_lock_wait(
/*==============*/
	trx_t*	trx)	/*!< in: transaction */
{
	que_thr_t*	thr;
	ulint           sec;
	ulint           ms;
	ib_uint64_t     now;

	ut_ad(mutex_own(&kernel_mutex));
	ut_ad(trx->que_state == TRX_QUE_LOCK_WAIT);

	thr = UT_LIST_GET_FIRST(trx->wait_thrs);

	while (thr != NULL) {
		que_thr_end_wait_no_next_thr(thr);

		UT_LIST_REMOVE(trx_thrs, trx->wait_thrs, thr);

		thr = UT_LIST_GET_FIRST(trx->wait_thrs);
	}

	if (innobase_get_slow_log() && trx->take_stats) {
		ut_usectime(&sec, &ms);
		now = (ib_uint64_t)sec * 1000000 + ms;
		trx->lock_que_wait_timer += (ulint)(now - trx->lock_que_wait_ustarted);
	}
	trx->que_state = TRX_QUE_RUNNING;
}

/***********************************************************//**
Moves the query threads in the lock wait list to the SUSPENDED state and puts
the transaction to the TRX_QUE_RUNNING state. */
static
void
trx_lock_wait_to_suspended(
/*=======================*/
	trx_t*	trx)	/*!< in: transaction in the TRX_QUE_LOCK_WAIT state */
{
	que_thr_t*	thr;
	ulint           sec;
	ulint           ms;
	ib_uint64_t     now;

	ut_ad(mutex_own(&kernel_mutex));
	ut_ad(trx->que_state == TRX_QUE_LOCK_WAIT);

	thr = UT_LIST_GET_FIRST(trx->wait_thrs);

	while (thr != NULL) {
		thr->state = QUE_THR_SUSPENDED;

		UT_LIST_REMOVE(trx_thrs, trx->wait_thrs, thr);

		thr = UT_LIST_GET_FIRST(trx->wait_thrs);
	}

	if (innobase_get_slow_log() && trx->take_stats) {
		ut_usectime(&sec, &ms);
		now = (ib_uint64_t)sec * 1000000 + ms;
		trx->lock_que_wait_timer += (ulint)(now - trx->lock_que_wait_ustarted);
	}
	trx->que_state = TRX_QUE_RUNNING;
}

/***********************************************************//**
Moves the query threads in the sig reply wait list of trx to the SUSPENDED
state. */
static
void
trx_sig_reply_wait_to_suspended(
/*============================*/
	trx_t*	trx)	/*!< in: transaction */
{
	trx_sig_t*	sig;
	que_thr_t*	thr;

	ut_ad(mutex_own(&kernel_mutex));

	sig = UT_LIST_GET_FIRST(trx->reply_signals);

	while (sig != NULL) {
		thr = sig->receiver;

		ut_ad(thr->state == QUE_THR_SIG_REPLY_WAIT);

		thr->state = QUE_THR_SUSPENDED;

		sig->receiver = NULL;

		UT_LIST_REMOVE(reply_signals, trx->reply_signals, sig);

		sig = UT_LIST_GET_FIRST(trx->reply_signals);
	}
}

/*****************************************************************//**
Checks the compatibility of a new signal with the other signals in the
queue.
@return	TRUE if the signal can be queued */
static
ibool
trx_sig_is_compatible(
/*==================*/
	trx_t*	trx,	/*!< in: trx handle */
	ulint	type,	/*!< in: signal type */
	ulint	sender)	/*!< in: TRX_SIG_SELF or TRX_SIG_OTHER_SESS */
{
	trx_sig_t*	sig;

	ut_ad(mutex_own(&kernel_mutex));

	if (UT_LIST_GET_LEN(trx->signals) == 0) {

		return(TRUE);
	}

	if (sender == TRX_SIG_SELF) {
		if (type == TRX_SIG_ERROR_OCCURRED) {

			return(TRUE);

		} else if (type == TRX_SIG_BREAK_EXECUTION) {

			return(TRUE);
		} else {
			return(FALSE);
		}
	}

	ut_ad(sender == TRX_SIG_OTHER_SESS);

	sig = UT_LIST_GET_FIRST(trx->signals);

	if (type == TRX_SIG_COMMIT) {
		while (sig != NULL) {

			if (sig->type == TRX_SIG_TOTAL_ROLLBACK) {

				return(FALSE);
			}

			sig = UT_LIST_GET_NEXT(signals, sig);
		}

		return(TRUE);

	} else if (type == TRX_SIG_TOTAL_ROLLBACK) {
		while (sig != NULL) {

			if (sig->type == TRX_SIG_COMMIT) {

				return(FALSE);
			}

			sig = UT_LIST_GET_NEXT(signals, sig);
		}

		return(TRUE);

	} else if (type == TRX_SIG_BREAK_EXECUTION) {

		return(TRUE);
	} else {
		ut_error;

		return(FALSE);
	}
}

/****************************************************************//**
Sends a signal to a trx object. */
UNIV_INTERN
void
trx_sig_send(
/*=========*/
	trx_t*		trx,		/*!< in: trx handle */
	ulint		type,		/*!< in: signal type */
	ulint		sender,		/*!< in: TRX_SIG_SELF or
					TRX_SIG_OTHER_SESS */
	que_thr_t*	receiver_thr,	/*!< in: query thread which wants the
					reply, or NULL; if type is
					TRX_SIG_END_WAIT, this must be NULL */
	trx_savept_t*	savept,		/*!< in: possible rollback savepoint, or
					NULL */
	que_thr_t**	next_thr)	/*!< in/out: next query thread to run;
					if the value which is passed in is
					a pointer to a NULL pointer, then the
					calling function can start running
					a new query thread; if the parameter
					is NULL, it is ignored */
{
	trx_sig_t*	sig;
	trx_t*		receiver_trx;

	ut_ad(trx);
	ut_ad(mutex_own(&kernel_mutex));

	if (!trx_sig_is_compatible(trx, type, sender)) {
		/* The signal is not compatible with the other signals in
		the queue: die */

		ut_error;
	}

	/* Queue the signal object */

	if (UT_LIST_GET_LEN(trx->signals) == 0) {

		/* The signal list is empty: the 'sig' slot must be unused
		(we improve performance a bit by avoiding mem_alloc) */
		sig = &(trx->sig);
	} else {
		/* It might be that the 'sig' slot is unused also in this
		case, but we choose the easy way of using mem_alloc */

		sig = mem_alloc(sizeof(trx_sig_t));
	}

	UT_LIST_ADD_LAST(signals, trx->signals, sig);

	sig->type = type;
	sig->sender = sender;
	sig->receiver = receiver_thr;

	if (savept) {
		sig->savept = *savept;
	}

	if (receiver_thr) {
		receiver_trx = thr_get_trx(receiver_thr);

		UT_LIST_ADD_LAST(reply_signals, receiver_trx->reply_signals,
				 sig);
	}

	if (trx->sess->state == SESS_ERROR) {

		trx_sig_reply_wait_to_suspended(trx);
	}

	if ((sender != TRX_SIG_SELF) || (type == TRX_SIG_BREAK_EXECUTION)) {
		ut_error;
	}

	/* If there were no other signals ahead in the queue, try to start
	handling of the signal */

	if (UT_LIST_GET_FIRST(trx->signals) == sig) {

		trx_sig_start_handle(trx, next_thr);
	}
}

/****************************************************************//**
Ends signal handling. If the session is in the error state, and
trx->graph_before_signal_handling != NULL, then returns control to the error
handling routine of the graph (currently just returns the control to the
graph root which then will send an error message to the client). */
UNIV_INTERN
void
trx_end_signal_handling(
/*====================*/
	trx_t*	trx)	/*!< in: trx */
{
	ut_ad(mutex_own(&kernel_mutex));
	ut_ad(trx->handling_signals == TRUE);

	trx->handling_signals = FALSE;

	trx->graph = trx->graph_before_signal_handling;

	if (trx->graph && (trx->sess->state == SESS_ERROR)) {

		que_fork_error_handle(trx, trx->graph);
	}
}

/****************************************************************//**
Starts handling of a trx signal. */
UNIV_INTERN
void
trx_sig_start_handle(
/*=================*/
	trx_t*		trx,		/*!< in: trx handle */
	que_thr_t**	next_thr)	/*!< in/out: next query thread to run;
					if the value which is passed in is
					a pointer to a NULL pointer, then the
					calling function can start running
					a new query thread; if the parameter
					is NULL, it is ignored */
{
	trx_sig_t*	sig;
	ulint		type;
loop:
	/* We loop in this function body as long as there are queued signals
	we can process immediately */

	ut_ad(trx);
	ut_ad(mutex_own(&kernel_mutex));

	if (trx->handling_signals && (UT_LIST_GET_LEN(trx->signals) == 0)) {

		trx_end_signal_handling(trx);

		return;
	}

	if (trx->conc_state == TRX_NOT_STARTED) {

		trx_start_low(trx, ULINT_UNDEFINED);
	}

	/* If the trx is in a lock wait state, moves the waiting query threads
	to the suspended state */

	if (trx->que_state == TRX_QUE_LOCK_WAIT) {

		trx_lock_wait_to_suspended(trx);
	}

	/* If the session is in the error state and this trx has threads
	waiting for reply from signals, moves these threads to the suspended
	state, canceling wait reservations; note that if the transaction has
	sent a commit or rollback signal to itself, and its session is not in
	the error state, then nothing is done here. */

	if (trx->sess->state == SESS_ERROR) {
		trx_sig_reply_wait_to_suspended(trx);
	}

	/* If there are no running query threads, we can start processing of a
	signal, otherwise we have to wait until all query threads of this
	transaction are aware of the arrival of the signal. */

	if (trx->n_active_thrs > 0) {

		return;
	}

	if (trx->handling_signals == FALSE) {
		trx->graph_before_signal_handling = trx->graph;

		trx->handling_signals = TRUE;
	}

	sig = UT_LIST_GET_FIRST(trx->signals);
	type = sig->type;

	if (type == TRX_SIG_COMMIT) {

		trx_handle_commit_sig_off_kernel(trx, next_thr);

	} else if ((type == TRX_SIG_TOTAL_ROLLBACK)
		   || (type == TRX_SIG_ROLLBACK_TO_SAVEPT)) {

		trx_rollback(trx, sig, next_thr);

		/* No further signals can be handled until the rollback
		completes, therefore we return */

		return;

	} else if (type == TRX_SIG_ERROR_OCCURRED) {

		trx_rollback(trx, sig, next_thr);

		/* No further signals can be handled until the rollback
		completes, therefore we return */

		return;

	} else if (type == TRX_SIG_BREAK_EXECUTION) {

		trx_sig_reply(sig, next_thr);
		trx_sig_remove(trx, sig);
	} else {
		ut_error;
	}

	goto loop;
}

/****************************************************************//**
Send the reply message when a signal in the queue of the trx has been
handled. */
UNIV_INTERN
void
trx_sig_reply(
/*==========*/
	trx_sig_t*	sig,		/*!< in: signal */
	que_thr_t**	next_thr)	/*!< in/out: next query thread to run;
					if the value which is passed in is
					a pointer to a NULL pointer, then the
					calling function can start running
					a new query thread */
{
	trx_t*	receiver_trx;

	ut_ad(sig);
	ut_ad(mutex_own(&kernel_mutex));

	if (sig->receiver != NULL) {
		ut_ad((sig->receiver)->state == QUE_THR_SIG_REPLY_WAIT);

		receiver_trx = thr_get_trx(sig->receiver);

		UT_LIST_REMOVE(reply_signals, receiver_trx->reply_signals,
			       sig);
		ut_ad(receiver_trx->sess->state != SESS_ERROR);

		que_thr_end_wait(sig->receiver, next_thr);

		sig->receiver = NULL;

	}
}

/****************************************************************//**
Removes a signal object from the trx signal queue. */
UNIV_INTERN
void
trx_sig_remove(
/*===========*/
	trx_t*		trx,	/*!< in: trx handle */
	trx_sig_t*	sig)	/*!< in, own: signal */
{
	ut_ad(trx && sig);
	ut_ad(mutex_own(&kernel_mutex));

	ut_ad(sig->receiver == NULL);

	UT_LIST_REMOVE(signals, trx->signals, sig);
	sig->type = 0;	/* reset the field to catch possible bugs */

	if (sig != &(trx->sig)) {
		mem_free(sig);
	}
}

/*********************************************************************//**
Creates a commit command node struct.
@return	own: commit node struct */
UNIV_INTERN
commit_node_t*
commit_node_create(
/*===============*/
	mem_heap_t*	heap)	/*!< in: mem heap where created */
{
	commit_node_t*	node;

	node = mem_heap_alloc(heap, sizeof(commit_node_t));
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
	que_thr_t*	next_thr;

	node = thr->run_node;

	ut_ad(que_node_get_type(node) == QUE_NODE_COMMIT);

	if (thr->prev_node == que_node_get_parent(node)) {
		node->state = COMMIT_NODE_SEND;
	}

	if (node->state == COMMIT_NODE_SEND) {
		mutex_enter(&kernel_mutex);

		node->state = COMMIT_NODE_WAIT;

		next_thr = NULL;

		thr->state = QUE_THR_SIG_REPLY_WAIT;

		/* Send the commit signal to the transaction */

		trx_sig_send(thr_get_trx(thr), TRX_SIG_COMMIT, TRX_SIG_SELF,
			     thr, NULL, &next_thr);

		mutex_exit(&kernel_mutex);

		return(next_thr);
	}

	ut_ad(node->state == COMMIT_NODE_WAIT);

	node->state = COMMIT_NODE_SEND;

	thr->run_node = que_node_get_parent(node);

	return(thr);
}

/**********************************************************************//**
Does the transaction commit for MySQL.
@return	DB_SUCCESS or error number */
UNIV_INTERN
ulint
trx_commit_for_mysql(
/*=================*/
	trx_t*	trx)	/*!< in: trx handle */
{
	/* Because we do not do the commit by sending an Innobase
	sig to the transaction, we must here make sure that trx has been
	started. */

	ut_a(trx);

	trx_start_if_not_started(trx);

	trx->op_info = "committing";

	mutex_enter(&kernel_mutex);

	trx_commit_off_kernel(trx);

	mutex_exit(&kernel_mutex);

	trx->op_info = "";

	return(DB_SUCCESS);
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
	ulint		flush_log_at_trx_commit;

	ut_a(trx);

	trx->op_info = "flushing log";

	if (srv_use_global_flush_log_at_trx_commit) {
		flush_log_at_trx_commit = thd_flush_log_at_trx_commit(NULL);
	} else {
		flush_log_at_trx_commit = thd_flush_log_at_trx_commit(trx->mysql_thd);
	}

	if (!trx->must_flush_log_later) {
		/* Do nothing */
	} else if (flush_log_at_trx_commit == 0) {
		/* Do nothing */
	} else if (flush_log_at_trx_commit == 1) {
		if (srv_unix_file_flush_method == SRV_UNIX_NOSYNC) {
			/* Write the log but do not flush it to disk */

			log_write_up_to(lsn, LOG_WAIT_ONE_GROUP, FALSE);
		} else {
			/* Write the log to the log files AND flush them to
			disk */

			log_write_up_to(lsn, LOG_WAIT_ONE_GROUP, TRUE);
		}
	} else if (flush_log_at_trx_commit == 2) {

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

	if (trx->conc_state == TRX_NOT_STARTED) {
		trx->undo_no = 0;
	}

	trx->last_sql_stat_start.least_undo_no = trx->undo_no;
}

/**********************************************************************//**
Prints info about a transaction to the given file. The caller must own the
kernel mutex. */
UNIV_INTERN
void
trx_print(
/*======*/
	FILE*	f,		/*!< in: output stream */
	trx_t*	trx,		/*!< in: transaction */
	ulint	max_query_len)	/*!< in: max query length to print, or 0 to
				   use the default max length */
{
	ibool	newline;

	fprintf(f, "TRANSACTION " TRX_ID_FMT, (ullint) trx->id);

	switch (trx->conc_state) {
	case TRX_NOT_STARTED:
		fputs(", not started", f);
		break;
	case TRX_ACTIVE:
		fprintf(f, ", ACTIVE %lu sec",
			(ulong)difftime(time(NULL), trx->start_time));
		break;
	case TRX_PREPARED:
		fprintf(f, ", ACTIVE (PREPARED) %lu sec",
			(ulong)difftime(time(NULL), trx->start_time));
		break;
	case TRX_COMMITTED_IN_MEMORY:
		fputs(", COMMITTED IN MEMORY", f);
		break;
	default:
		fprintf(f, " state %lu", (ulong) trx->conc_state);
	}

	if (*trx->op_info) {
		putc(' ', f);
		fputs(trx->op_info, f);
	}

	if (trx->is_recovered) {
		fputs(" recovered trx", f);
	}

	if (trx->is_purge) {
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

	switch (trx->que_state) {
	case TRX_QUE_RUNNING:
		newline = FALSE; break;
	case TRX_QUE_LOCK_WAIT:
		fputs("LOCK WAIT ", f); break;
	case TRX_QUE_ROLLING_BACK:
		fputs("ROLLING BACK ", f); break;
	case TRX_QUE_COMMITTING:
		fputs("COMMITTING ", f); break;
	default:
		fprintf(f, "que state %lu ", (ulong) trx->que_state);
	}

	if (0 < UT_LIST_GET_LEN(trx->trx_locks)
	    || mem_heap_get_size(trx->lock_heap) > 400) {
		newline = TRUE;

		fprintf(f, "%lu lock struct(s), heap size %lu,"
			" %lu row lock(s)",
			(ulong) UT_LIST_GET_LEN(trx->trx_locks),
			(ulong) mem_heap_get_size(trx->lock_heap),
			(ulong) lock_number_of_rows_locked(trx));
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
		a->undo_no, UT_LIST_GET_LEN(a->trx_locks),
		b->undo_no, UT_LIST_GET_LEN(b->trx_locks));
#endif

	return(TRX_WEIGHT(a) >= TRX_WEIGHT(b));
}

/****************************************************************//**
Prepares a transaction. */
UNIV_INTERN
void
trx_prepare_off_kernel(
/*===================*/
	trx_t*	trx)	/*!< in: transaction */
{
	trx_rseg_t*	rseg;
	ib_uint64_t	lsn		= 0;
	mtr_t		mtr;

	ut_ad(mutex_own(&kernel_mutex));

	rseg = trx->rseg;

	if (trx->insert_undo != NULL || trx->update_undo != NULL) {

		mutex_exit(&kernel_mutex);

		mtr_start(&mtr);

		/* Change the undo log segment states from TRX_UNDO_ACTIVE
		to TRX_UNDO_PREPARED: these modifications to the file data
		structure define the transaction as prepared in the
		file-based world, at the serialization point of lsn. */

		mutex_enter(&(rseg->mutex));

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

		mutex_exit(&(rseg->mutex));

		if (trx->mysql_master_log_file_name[0] != '\0') {
			/* This database server is a MySQL replication slave */
			trx_sysf_t*	sys_header	= trx_sysf_get(&mtr);

			trx_sys_update_mysql_binlog_offset(
				sys_header,
				trx->mysql_relay_log_file_name,
				trx->mysql_relay_log_pos,
				TRX_SYS_MYSQL_RELAY_LOG_INFO, &mtr);
			trx_sys_update_mysql_binlog_offset(
				sys_header,
				trx->mysql_master_log_file_name,
				trx->mysql_master_log_pos,
				TRX_SYS_MYSQL_MASTER_LOG_INFO, &mtr);
			trx->mysql_master_log_file_name = "";
		}

		/*--------------*/
		mtr_commit(&mtr);	/* This mtr commit makes the
					transaction prepared in the file-based
					world */
		/*--------------*/
		lsn = mtr.end_lsn;

		mutex_enter(&kernel_mutex);
	}

	ut_ad(mutex_own(&kernel_mutex));

	/*--------------------------------------*/
	trx->conc_state = TRX_PREPARED;
	trx_n_prepared++;
	/*--------------------------------------*/

	if (lsn) {
		ulint	flush_log_at_trx_commit;

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

		mutex_exit(&kernel_mutex);

		if (srv_use_global_flush_log_at_trx_commit) {
			flush_log_at_trx_commit = thd_flush_log_at_trx_commit(NULL);
		} else {
			flush_log_at_trx_commit = thd_flush_log_at_trx_commit(trx->mysql_thd);
		}

		if (flush_log_at_trx_commit == 0) {
			/* Do nothing */
		} else if (flush_log_at_trx_commit == 1) {
			if (srv_unix_file_flush_method == SRV_UNIX_NOSYNC) {
				/* Write the log but do not flush it to disk */

				log_write_up_to(lsn, LOG_WAIT_ONE_GROUP,
						FALSE);
			} else {
				/* Write the log to the log files AND flush
				them to disk */

				log_write_up_to(lsn, LOG_WAIT_ONE_GROUP, TRUE);
			}
		} else if (flush_log_at_trx_commit == 2) {

			/* Write the log but do not flush it to disk */

			log_write_up_to(lsn, LOG_WAIT_ONE_GROUP, FALSE);
		} else {
			ut_error;
		}

		mutex_enter(&kernel_mutex);
	}
}

/**********************************************************************//**
Does the transaction prepare for MySQL.
@return	0 or error number */
UNIV_INTERN
ulint
trx_prepare_for_mysql(
/*==================*/
	trx_t*	trx)	/*!< in: trx handle */
{
	/* Because we do not do the prepare by sending an Innobase
	sig to the transaction, we must here make sure that trx has been
	started. */

	ut_a(trx);

	trx->op_info = "preparing";

	trx_start_if_not_started(trx);

	mutex_enter(&kernel_mutex);

	trx_prepare_off_kernel(trx);

	mutex_exit(&kernel_mutex);

	trx->op_info = "";

	return(0);
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
	trx_t*	trx;
	ulint	count = 0;

	ut_ad(xid_list);
	ut_ad(len);

	/* We should set those transactions which are in the prepared state
	to the xid_list */

	mutex_enter(&kernel_mutex);

	trx = UT_LIST_GET_FIRST(trx_sys->trx_list);

	while (trx) {
		if (trx->conc_state == TRX_PREPARED) {
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

		trx = UT_LIST_GET_NEXT(trx_list, trx);
	}

	mutex_exit(&kernel_mutex);

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
@return	trx or NULL; on match, the trx->xid will be invalidated */
UNIV_INTERN
trx_t*
trx_get_trx_by_xid(
/*===============*/
	const XID*	xid)	/*!< in: X/Open XA transaction identifier */
{
	trx_t*	trx;

	if (xid == NULL) {

		return(NULL);
	}

	mutex_enter(&kernel_mutex);

	trx = UT_LIST_GET_FIRST(trx_sys->trx_list);

	while (trx) {
		/* Compare two X/Open XA transaction id's: their
		length should be the same and binary comparison
		of gtrid_length+bqual_length bytes should be
		the same */

		if (trx->is_recovered
		    && trx->conc_state == TRX_PREPARED
		    && xid->gtrid_length == trx->xid.gtrid_length
		    && xid->bqual_length == trx->xid.bqual_length
		    && memcmp(xid->data, trx->xid.data,
			      xid->gtrid_length + xid->bqual_length) == 0) {

			/* Invalidate the XID, so that subsequent calls
			will not find it. */
			memset(&trx->xid, 0, sizeof(trx->xid));
			trx->xid.formatID = -1;
			break;
		}

		trx = UT_LIST_GET_NEXT(trx_list, trx);
	}

	mutex_exit(&kernel_mutex);

	return(trx);
}
