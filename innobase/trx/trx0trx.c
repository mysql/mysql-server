/******************************************************
The transaction

(c) 1996 Innobase Oy

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
#include "thr0loc.h"

/* Dummy session used currently in MySQL interface */
sess_t*		trx_dummy_sess = NULL;

/* Number of transactions currently allocated for MySQL: protected by
the kernel mutex */
ulint	trx_n_mysql_transactions = 0;


/********************************************************************
Creates and initializes a transaction object. */

trx_t*
trx_create(
/*=======*/
			/* out, own: the transaction */
	sess_t*	sess)	/* in: session or NULL */
{
	trx_t*	trx;

	ut_ad(mutex_own(&kernel_mutex));

	trx = mem_alloc(sizeof(trx_t));

	trx->type = TRX_USER;
	trx->conc_state = TRX_NOT_STARTED;

	trx->dict_operation = FALSE;

	trx->n_mysql_tables_in_use = 0;

	mutex_create(&(trx->undo_mutex));
	mutex_set_level(&(trx->undo_mutex), SYNC_TRX_UNDO);

	trx->rseg = NULL;

	trx->undo_no = ut_dulint_zero;
	trx->last_sql_stat_start.least_undo_no = ut_dulint_zero;
	trx->insert_undo = NULL;
	trx->update_undo = NULL;
	trx->undo_no_arr = NULL;
	
	trx->error_state = DB_SUCCESS;

	trx->sess = sess;
	trx->que_state = TRX_QUE_RUNNING;
	trx->n_active_thrs = 0;

	trx->handling_signals = FALSE;

	UT_LIST_INIT(trx->signals);
	UT_LIST_INIT(trx->reply_signals);

	trx->graph = NULL;

	trx->wait_lock = NULL;
	UT_LIST_INIT(trx->wait_thrs);

	trx->lock_heap = mem_heap_create_in_buffer(256);
	UT_LIST_INIT(trx->trx_locks);

	trx->read_view_heap = mem_heap_create(256);
	trx->read_view = NULL;

	return(trx);
}

/************************************************************************
Creates a transaction object for MySQL. */

trx_t*
trx_allocate_for_mysql(void)
/*========================*/
				/* out, own: transaction object */
{
	trx_t*	trx;

	mutex_enter(&kernel_mutex);
	
	/* Open a dummy session */

	if (!trx_dummy_sess) {
		trx_dummy_sess = sess_open(NULL, (byte*)"Dummy sess",
						ut_strlen("Dummy sess"));
	}
	
	trx = trx_create(trx_dummy_sess);

	trx_n_mysql_transactions++;
	
	mutex_exit(&kernel_mutex);

	trx->mysql_thread_id = os_thread_get_curr_id();
	
	return(trx);
}

/************************************************************************
Frees a transaction object. */

void
trx_free(
/*=====*/
	trx_t*	trx)	/* in, own: trx object */
{
	ut_ad(mutex_own(&kernel_mutex));
	ut_a(trx->conc_state == TRX_NOT_STARTED);
	
	mutex_free(&(trx->undo_mutex));

	ut_a(trx->insert_undo == NULL); 
	ut_a(trx->update_undo == NULL); 

	ut_a(trx->n_mysql_tables_in_use == 0);
	
	if (trx->undo_no_arr) {
		trx_undo_arr_free(trx->undo_no_arr);
	}

	ut_a(UT_LIST_GET_LEN(trx->signals) == 0);
	ut_a(UT_LIST_GET_LEN(trx->reply_signals) == 0);

	ut_a(trx->wait_lock == NULL);
	ut_a(UT_LIST_GET_LEN(trx->wait_thrs) == 0);

	if (trx->lock_heap) {
		mem_heap_free(trx->lock_heap);
	}

	ut_a(UT_LIST_GET_LEN(trx->trx_locks) == 0);

	if (trx->read_view_heap) {
		mem_heap_free(trx->read_view_heap);
	}

	ut_a(trx->read_view == NULL);
	
	mem_free(trx);
}

/************************************************************************
Frees a transaction object for MySQL. */

void
trx_free_for_mysql(
/*===============*/
	trx_t*	trx)	/* in, own: trx object */
{
	thr_local_free(trx->mysql_thread_id);

	mutex_enter(&kernel_mutex);
	
	trx_free(trx);

	ut_a(trx_n_mysql_transactions > 0);

	trx_n_mysql_transactions--;
	
	mutex_exit(&kernel_mutex);
}

/********************************************************************
Inserts the trx handle in the trx system trx list in the right position.
The list is sorted on the trx id so that the biggest id is at the list
start. This function is used at the database startup to insert incomplete
transactions to the list. */
static
void
trx_list_insert_ordered(
/*====================*/
	trx_t*	trx)	/* in: trx handle */
{
	trx_t*	trx2;

	ut_ad(mutex_own(&kernel_mutex));

	trx2 = UT_LIST_GET_FIRST(trx_sys->trx_list);

	while (trx2 != NULL) {
		if (ut_dulint_cmp(trx->id, trx2->id) >= 0) {

			ut_ad(ut_dulint_cmp(trx->id, trx2->id) == 1);
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

/********************************************************************
Creates trx objects for transactions and initializes the trx list of
trx_sys at database start. Rollback segment and undo log lists must
already exist when this function is called, because the lists of
transactions to be rolled back or cleaned up are built based on the
undo log lists. */

void
trx_lists_init_at_db_start(void)
/*============================*/
{
	trx_rseg_t*	rseg;
	trx_undo_t*	undo;
	trx_t*		trx;

	UT_LIST_INIT(trx_sys->trx_list);

	/* Look from the rollback segments if there exist undo logs for
	transactions */
	
	rseg = UT_LIST_GET_FIRST(trx_sys->rseg_list);

	while (rseg != NULL) {
		undo = UT_LIST_GET_FIRST(rseg->insert_undo_list);

		while (undo != NULL) {

			trx = trx_create(NULL); 

			if (undo->state != TRX_UNDO_ACTIVE) {

				trx->conc_state = TRX_COMMITTED_IN_MEMORY;
			} else {
				trx->conc_state = TRX_ACTIVE;
			}

			trx->id = undo->trx_id;
			trx->insert_undo = undo;
			trx->rseg = rseg;

			if (undo->dict_operation) {
				trx->dict_operation = undo->dict_operation;
				trx->table_id = undo->table_id;
			}

			if (!undo->empty) {
				trx->undo_no = ut_dulint_add(undo->top_undo_no,
									1);
			}

			trx_list_insert_ordered(trx);

			undo = UT_LIST_GET_NEXT(undo_list, undo);
		}

		undo = UT_LIST_GET_FIRST(rseg->update_undo_list);

		while (undo != NULL) {
			trx = trx_get_on_id(undo->trx_id);

			if (NULL == trx) {
				trx = trx_create(NULL); 

				if (undo->state != TRX_UNDO_ACTIVE) {
					trx->conc_state =
						TRX_COMMITTED_IN_MEMORY;
				} else {
					trx->conc_state = TRX_ACTIVE;
				}

				trx->id = undo->trx_id;
				trx->rseg = rseg;
				trx_list_insert_ordered(trx);

				if (undo->dict_operation) {
					trx->dict_operation =
							undo->dict_operation;
					trx->table_id = undo->table_id;
				}
			}

			trx->update_undo = undo;

			if ((!undo->empty)
			    && (ut_dulint_cmp(undo->top_undo_no, trx->undo_no)
			        >= 0)) {

				trx->undo_no = ut_dulint_add(undo->top_undo_no,
									1);
			}
			
			undo = UT_LIST_GET_NEXT(undo_list, undo);
		}

		rseg = UT_LIST_GET_NEXT(rseg_list, rseg);
	}
}

/**********************************************************************
Assigns a rollback segment to a transaction in a round-robin fashion.
Skips the SYSTEM rollback segment if another is available. */
UNIV_INLINE
ulint
trx_assign_rseg(void)
/*=================*/
			/* out: assigned rollback segment id */
{
	trx_rseg_t*	rseg	= trx_sys->latest_rseg;

	ut_ad(mutex_own(&kernel_mutex));
loop:
	/* Get next rseg in a round-robin fashion */

	rseg = UT_LIST_GET_NEXT(rseg_list, rseg);

	if (rseg == NULL) {
		rseg = UT_LIST_GET_FIRST(trx_sys->rseg_list);
	}

	/* If it is the SYSTEM rollback segment, and there exist others, skip
	it */

	if ((rseg->id == TRX_SYS_SYSTEM_RSEG_ID) 
			&& (UT_LIST_GET_LEN(trx_sys->rseg_list) > 1)) {
		goto loop;
	}			

	trx_sys->latest_rseg = rseg;

	return(rseg->id);
}

/********************************************************************
Starts a new transaction. */

ibool
trx_start_low(
/*==========*/
			/* out: TRUE */
	trx_t* 	trx,	/* in: transaction */
	ulint	rseg_id)/* in: rollback segment id; if ULINT_UNDEFINED
			is passed, the system chooses the rollback segment
			automatically in a round-robin fashion */
{
	trx_rseg_t*	rseg;

	ut_ad(mutex_own(&kernel_mutex));
	ut_ad(trx->rseg == NULL);

	if (trx->type == TRX_PURGE) {
		trx->id = ut_dulint_zero;
		trx->conc_state = TRX_ACTIVE;

		return(TRUE);
	}

	ut_ad(trx->conc_state != TRX_ACTIVE);
	
	if (rseg_id == ULINT_UNDEFINED) {

		rseg_id = trx_assign_rseg();
	}

	rseg = trx_sys_get_nth_rseg(trx_sys, rseg_id);

	trx->id = trx_sys_get_new_trx_id();

	/* The initial value for trx->no: ut_dulint_max is used in
	read_view_open_now: */

	trx->no = ut_dulint_max;

	trx->rseg = rseg;

	trx->conc_state = TRX_ACTIVE;

	UT_LIST_ADD_FIRST(trx_list, trx_sys->trx_list, trx);

	return(TRUE);
}

/********************************************************************
Starts a new transaction. */

ibool
trx_start(
/*======*/
			/* out: TRUE */
	trx_t* 	trx,	/* in: transaction */
	ulint	rseg_id)/* in: rollback segment id; if ULINT_UNDEFINED
			is passed, the system chooses the rollback segment
			automatically in a round-robin fashion */
{
	ibool	ret;
	
	mutex_enter(&kernel_mutex);

	ret = trx_start_low(trx, rseg_id);

	mutex_exit(&kernel_mutex);

	return(ret);
}

/********************************************************************
Commits a transaction. */

void
trx_commit_off_kernel(
/*==================*/
	trx_t*	trx)	/* in: transaction */
{
	page_t*		update_hdr_page;
	dulint		lsn;
	trx_rseg_t*	rseg;
	trx_undo_t*	undo;
	ibool		must_flush_log	= FALSE;
	mtr_t		mtr;
	
	ut_ad(mutex_own(&kernel_mutex));

	rseg = trx->rseg;
	
	if ((trx->insert_undo != NULL) || (trx->update_undo != NULL)) {

		mutex_exit(&kernel_mutex);

		mtr_start(&mtr);
		
		must_flush_log = TRUE;

		/* Change the undo log segment states from TRX_UNDO_ACTIVE
		to some other state: these modifications to the file data
		structure define the transaction as committed in the file
		based world, at the serialization point of the log sequence
		number lsn obtained below. */

		mutex_enter(&(rseg->mutex));
			
		if (trx->insert_undo != NULL) {
			trx_undo_set_state_at_finish(trx, trx->insert_undo,
									&mtr);
		}

		undo = trx->update_undo;

		if (undo) {
			mutex_enter(&kernel_mutex);
#ifdef TRX_UPDATE_UNDO_OPT
			if (!undo->del_marks && (undo->size == 1)
			    && (UT_LIST_GET_LEN(trx_sys->view_list) == 1)) {

			    	/* There is no need to save the update undo
			    	log: discard it; note that &mtr gets committed
			    	while we must hold the kernel mutex and
				therefore this optimization may add to the
				contention of the kernel mutex. */

			    	lsn = trx_undo_update_cleanup_by_discard(trx,
									&mtr);
				mutex_exit(&(rseg->mutex));

			    	goto shortcut;
			}
#endif
			trx->no = trx_sys_get_new_trx_no();
			
			mutex_exit(&kernel_mutex);

			/* It is not necessary to obtain trx->undo_mutex here
			because only a single OS thread is allowed to do the
			transaction commit for this transaction. */
					
			update_hdr_page = trx_undo_set_state_at_finish(trx,
								undo, &mtr);

			/* We have to do the cleanup for the update log while
			holding the rseg mutex because update log headers
			have to be put to the history list in the order of
			the trx number. */

			trx_undo_update_cleanup(trx, update_hdr_page, &mtr);
		}

		mutex_exit(&(rseg->mutex));

		/* If we did not take the shortcut, the following call
		commits the mini-transaction, making the whole transaction
		committed in the file-based world at this log sequence number;
		otherwise, we get the commit lsn from the call of
		trx_undo_update_cleanup_by_discard above.
		NOTE that transaction numbers, which are assigned only to
		transactions with an update undo log, do not necessarily come
		in exactly the same order as commit lsn's, if the transactions
		have different rollback segments. To get exactly the same
		order we should hold the kernel mutex up to this point,
		adding to to the contention of the kernel mutex. However, if
		a transaction T2 is able to see modifications made by
		a transaction T1, T2 will always get a bigger transaction
		number and a bigger commit lsn than T1. */

		/*--------------*/
 		mtr_commit(&mtr);
 		/*--------------*/
 		lsn = mtr.end_lsn;

		mutex_enter(&kernel_mutex);
	}
#ifdef TRX_UPDATE_UNDO_OPT
shortcut:
#endif
	ut_ad(trx->conc_state == TRX_ACTIVE);
	ut_ad(mutex_own(&kernel_mutex));
	
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

	lock_release_off_kernel(trx);

	if (trx->read_view) {
		read_view_close(trx->read_view);

		mem_heap_empty(trx->read_view_heap);
		trx->read_view = NULL;
	}

/*	printf("Trx %lu commit finished\n", ut_dulint_get_low(trx->id)); */

	if (must_flush_log) {

		mutex_exit(&kernel_mutex);
	
		if (trx->insert_undo != NULL) {

			trx_undo_insert_cleanup(trx);
		}

		/* NOTE that we could possibly make a group commit more
		efficient here: call os_thread_yield here to allow also other
		trxs to come to commit! */

		/* We now flush the log, as the transaction made changes to
		the database, making the transaction committed on disk. It is
		enough that any one of the log groups gets written to disk. */

		/*-------------------------------------*/

		/* Only in some performance tests the variable srv_flush..
		will be set to FALSE: */

		if (srv_flush_log_at_trx_commit) {
		
 			log_flush_up_to(lsn, LOG_WAIT_ONE_GROUP);
 		}

		/*-------------------------------------*/
	
		mutex_enter(&kernel_mutex);
	}

	trx->conc_state = TRX_NOT_STARTED;
	trx->rseg = NULL;
	trx->undo_no = ut_dulint_zero;
	trx->last_sql_stat_start.least_undo_no = ut_dulint_zero;

	ut_ad(UT_LIST_GET_LEN(trx->wait_thrs) == 0);
	ut_ad(UT_LIST_GET_LEN(trx->trx_locks) == 0);

	UT_LIST_REMOVE(trx_list, trx_sys->trx_list, trx);	
}

/************************************************************************
Assigns a read view for a consistent read query. All the consistent reads
within the same transaction will get the same read view, which is created
when this function is first called for a new started transaction. */

read_view_t*
trx_assign_read_view(
/*=================*/
			/* out: consistent read view */
	trx_t*	trx)	/* in: active transaction */
{
	ut_ad(trx->conc_state == TRX_ACTIVE);

	if (trx->read_view) {
		return(trx->read_view);
	}
	
	mutex_enter(&kernel_mutex);

	if (!trx->read_view) {
		trx->read_view = read_view_open_now(trx, trx->read_view_heap);
	}

	mutex_exit(&kernel_mutex);
	
	return(trx->read_view);
}

/********************************************************************
Commits a transaction. NOTE that the kernel mutex is temporarily released. */
static
void
trx_handle_commit_sig_off_kernel(
/*=============================*/
	trx_t*		trx,		/* in: transaction */
	que_thr_t**	next_thr)	/* in/out: next query thread to run;
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

			trx_sig_reply(trx, sig, next_thr);
			trx_sig_remove(trx, sig);
		}

		sig = next_sig;
	}

	trx->que_state = TRX_QUE_RUNNING;
}

/***************************************************************
The transaction must be in the TRX_QUE_LOCK_WAIT state. Puts it to
the TRX_QUE_RUNNING state and releases query threads which were
waiting for a lock in the wait_thrs list. */

void
trx_end_lock_wait(
/*==============*/
	trx_t*	trx)	/* in: transaction */
{
	que_thr_t*	thr;

	ut_ad(mutex_own(&kernel_mutex));
	ut_ad(trx->que_state == TRX_QUE_LOCK_WAIT);
	
	thr = UT_LIST_GET_FIRST(trx->wait_thrs);

	while (thr != NULL) {
		que_thr_end_wait_no_next_thr(thr);

		UT_LIST_REMOVE(trx_thrs, trx->wait_thrs, thr);
			
		thr = UT_LIST_GET_FIRST(trx->wait_thrs);
	}

	trx->que_state = TRX_QUE_RUNNING;
}

/***************************************************************
Moves the query threads in the lock wait list to the SUSPENDED state and puts
the transaction to the TRX_QUE_RUNNING state. */
static
void
trx_lock_wait_to_suspended(
/*=======================*/
	trx_t*	trx)	/* in: transaction in the TRX_QUE_LOCK_WAIT state */
{
	que_thr_t*	thr;

	ut_ad(mutex_own(&kernel_mutex));
	ut_ad(trx->que_state == TRX_QUE_LOCK_WAIT);
	
	thr = UT_LIST_GET_FIRST(trx->wait_thrs);

	while (thr != NULL) {
		thr->state = QUE_THR_SUSPENDED;
	
		UT_LIST_REMOVE(trx_thrs, trx->wait_thrs, thr);
			
		thr = UT_LIST_GET_FIRST(trx->wait_thrs);
	}

	trx->que_state = TRX_QUE_RUNNING;
}

/***************************************************************
Moves the query threads in the sig reply wait list of trx to the SUSPENDED
state. */
static
void
trx_sig_reply_wait_to_suspended(
/*============================*/
	trx_t*	trx)	/* in: transaction */
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
		sig->reply = FALSE;
	
		UT_LIST_REMOVE(reply_signals, trx->reply_signals, sig);
			
		sig = UT_LIST_GET_FIRST(trx->reply_signals);
	}
}

/*********************************************************************
Checks the compatibility of a new signal with the other signals in the
queue. */
static
ibool
trx_sig_is_compatible(
/*==================*/
			/* out: TRUE if the signal can be queued */
	trx_t*	trx,	/* in: trx handle */
	ulint	type,	/* in: signal type */
	ulint	sender)	/* in: TRX_SIG_SELF or TRX_SIG_OTHER_SESS */
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

/********************************************************************
Sends a signal to a trx object. */

ibool
trx_sig_send(
/*=========*/
					/* out: TRUE if the signal was
					successfully delivered */
	trx_t*		trx,		/* in: trx handle */
	ulint		type,		/* in: signal type */
	ulint		sender,		/* in: TRX_SIG_SELF or
					TRX_SIG_OTHER_SESS */
	ibool		reply,		/* in: TRUE if the sender of the signal
					wants reply after the operation induced
					by the signal is completed; if type
					is TRX_SIG_END_WAIT, this must be
					FALSE */
	que_thr_t*	receiver_thr,	/* in: query thread which wants the
					reply, or NULL */
	trx_savept_t* 	savept,		/* in: possible rollback savepoint, or
					NULL */
	que_thr_t**	next_thr)	/* in/out: next query thread to run;
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
		the queue: do nothing */

		ut_a(0);
		
		/* sess_raise_error_low(trx, 0, 0, NULL, NULL, NULL, NULL,
						"Incompatible signal"); */
		return(FALSE);
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
	sig->state = TRX_SIG_WAITING;
	sig->sender = sender;
	sig->reply = reply;
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

		/* The following call will add a TRX_SIG_ERROR_OCCURRED
		signal to the end of the queue, if the session is not yet
		in the error state: */

		ut_a(0);

		sess_raise_error_low(trx, 0, 0, NULL, NULL, NULL, NULL,
		  "Signal from another session, or a break execution signal");
	}

	/* If there were no other signals ahead in the queue, try to start
	handling of the signal */

	if (UT_LIST_GET_FIRST(trx->signals) == sig) {
	
		trx_sig_start_handle(trx, next_thr);
	}

	return(TRUE);
}

/********************************************************************
Ends signal handling. If the session is in the error state, and
trx->graph_before_signal_handling != NULL, then returns control to the error
handling routine of the graph (currently just returns the control to the
graph root which then will send an error message to the client). */

void
trx_end_signal_handling(
/*====================*/
	trx_t*	trx)	/* in: trx */
{
	ut_ad(mutex_own(&kernel_mutex));
	ut_ad(trx->handling_signals == TRUE);

	trx->handling_signals = FALSE;

	trx->graph = trx->graph_before_signal_handling;

	if (trx->graph && (trx->sess->state == SESS_ERROR)) {
			
		que_fork_error_handle(trx, trx->graph);
	}
}

/********************************************************************
Starts handling of a trx signal. */

void
trx_sig_start_handle(
/*=================*/
	trx_t*		trx,		/* in: trx handle */
	que_thr_t**	next_thr)	/* in/out: next query thread to run;
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

		trx_sig_reply(trx, sig, next_thr);
		trx_sig_remove(trx, sig);
	} else {
		ut_error;
	}

	goto loop;
}			

/********************************************************************
Send the reply message when a signal in the queue of the trx has been
handled. */

void
trx_sig_reply(
/*==========*/
	trx_t*		trx,		/* in: trx handle */
	trx_sig_t*	sig,		/* in: signal */
	que_thr_t**	next_thr)	/* in/out: next query thread to run;
					if the value which is passed in is
					a pointer to a NULL pointer, then the
					calling function can start running
					a new query thread */
{
	trx_t*	receiver_trx;

	ut_ad(trx && sig);
	ut_ad(mutex_own(&kernel_mutex));

	if (sig->reply && (sig->receiver != NULL)) {

		ut_ad((sig->receiver)->state == QUE_THR_SIG_REPLY_WAIT);

		receiver_trx = thr_get_trx(sig->receiver);

		UT_LIST_REMOVE(reply_signals, receiver_trx->reply_signals,
									sig);
		ut_ad(receiver_trx->sess->state != SESS_ERROR);
									
		que_thr_end_wait(sig->receiver, next_thr);

		sig->reply = FALSE;
		sig->receiver = NULL;

	} else if (sig->reply) {
		/* In this case the reply should be sent to the client of
		the session of the transaction */

		sig->reply = FALSE;
		sig->receiver = NULL;

		sess_srv_msg_send_simple(trx->sess, SESS_SRV_SUCCESS,
						SESS_NOT_RELEASE_KERNEL);
	}
}

/********************************************************************
Removes a signal object from the trx signal queue. */

void
trx_sig_remove(
/*===========*/
	trx_t*		trx,	/* in: trx handle */
	trx_sig_t*	sig)	/* in, own: signal */
{
	ut_ad(trx && sig);
	ut_ad(mutex_own(&kernel_mutex));

	ut_ad(sig->reply == FALSE);
	ut_ad(sig->receiver == NULL);

	UT_LIST_REMOVE(signals, trx->signals, sig);
	sig->type = 0;	/* reset the field to catch possible bugs */

	if (sig != &(trx->sig)) {
		mem_free(sig);
	}
}

/*************************************************************************
Creates a commit command node struct. */

commit_node_t*
commit_node_create(
/*===============*/
				/* out, own: commit node struct */
	mem_heap_t*	heap)	/* in: mem heap where created */
{
	commit_node_t*	node;

	node = mem_heap_alloc(heap, sizeof(commit_node_t));
	node->common.type  = QUE_NODE_COMMIT;
	node->state = COMMIT_NODE_SEND;
	
	return(node);
}

/***************************************************************
Performs an execution step for a commit type node in a query graph. */

que_thr_t*
trx_commit_step(
/*============*/
				/* out: query thread to run next, or NULL */
	que_thr_t*	thr)	/* in: query thread */
{
	commit_node_t*	node;
	que_thr_t*	next_thr;
	ibool		success;
	
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
		
		success = trx_sig_send(thr_get_trx(thr), TRX_SIG_COMMIT,
					TRX_SIG_SELF, TRUE, thr, NULL,
					&next_thr);
		
		mutex_exit(&kernel_mutex);

		if (!success) {
			/* Error in delivering the commit signal */
			que_thr_handle_error(thr, DB_ERROR, NULL, 0);
		}

		return(next_thr);
	}

	ut_ad(node->state == COMMIT_NODE_WAIT);
		
	node->state = COMMIT_NODE_SEND;
	
	thr->run_node = que_node_get_parent(node);

	return(thr);
}

/**************************************************************************
Does the transaction commit for MySQL. */

ulint
trx_commit_for_mysql(
/*=================*/
			/* out: 0 or error number */
	trx_t*	trx)	/* in: trx handle */
{
	/* Because we do not do the commit by sending an Innobase
	sig to the transaction, we must here make sure that trx has been
	started. */

	trx_start_if_not_started(trx);

	mutex_enter(&kernel_mutex);

	trx_commit_off_kernel(trx);

	mutex_exit(&kernel_mutex);

	return(0);
}

/**************************************************************************
Marks the latest SQL statement ended. */

void
trx_mark_sql_stat_end(
/*==================*/
	trx_t*	trx)	/* in: trx handle */
{
	trx_start_if_not_started(trx);

	mutex_enter(&kernel_mutex);

	trx->last_sql_stat_start.least_undo_no = trx->undo_no;

	mutex_exit(&kernel_mutex);
}
