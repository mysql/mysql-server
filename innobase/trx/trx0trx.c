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
#include "btr0sea.h"
#include "os0proc.h"
#include "trx0xa.h"

/* Copy of the prototype for innobase_mysql_print_thd: this
copy MUST be equal to the one in mysql/sql/ha_innodb.cc ! */

void innobase_mysql_print_thd(
	FILE* f,
	void* thd);

/* Dummy session used currently in MySQL interface */
sess_t*		trx_dummy_sess = NULL;

/* Number of transactions currently allocated for MySQL: protected by
the kernel mutex */
ulint	trx_n_mysql_transactions = 0;

/*****************************************************************
Starts the transaction if it is not yet started. */

void
trx_start_if_not_started_noninline(
/*===============================*/
	trx_t*  trx) /* in: transaction */
{
        trx_start_if_not_started(trx);
}

/********************************************************************
Retrieves the error_info field from a trx. */

void*
trx_get_error_info(
/*===============*/
		     /* out: the error info */
	trx_t*  trx) /* in: trx object */
{
        return(trx->error_info);
}

/********************************************************************
Creates and initializes a transaction object. */

trx_t*
trx_create(
/*=======*/
			/* out, own: the transaction */
	sess_t*	sess)	/* in: session or NULL */
{
	trx_t*	trx;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */

	trx = mem_alloc(sizeof(trx_t));

	trx->magic_n = TRX_MAGIC_N;

	trx->op_info = "";
	
	trx->type = TRX_USER;
	trx->conc_state = TRX_NOT_STARTED;
	trx->start_time = time(NULL);

	trx->isolation_level = TRX_ISO_REPEATABLE_READ;

	trx->id = ut_dulint_zero;
	trx->no = ut_dulint_max;

	trx->check_foreigns = TRUE;
	trx->check_unique_secondary = TRUE;

	trx->flush_log_later = FALSE;

	trx->dict_operation = FALSE;

	trx->mysql_thd = NULL;
	trx->mysql_query_str = NULL;

	trx->n_mysql_tables_in_use = 0;
	trx->mysql_n_tables_locked = 0;

	trx->mysql_log_file_name = NULL;
	trx->mysql_log_offset = 0;
	trx->mysql_master_log_file_name = "";
	trx->mysql_master_log_pos = 0;
	
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

	trx->auto_inc_lock = NULL;
	trx->n_lock_table_exp = 0;
	
	trx->read_view_heap = mem_heap_create(256);
	trx->read_view = NULL;

	/* Set X/Open XA transaction identification to NULL */
	memset(&trx->xid,0,sizeof(trx->xid));
	trx->xid.formatID = -1;

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
		trx_dummy_sess = sess_open();
	}
	
	trx = trx_create(trx_dummy_sess);

	trx_n_mysql_transactions++;
	
	UT_LIST_ADD_FIRST(mysql_trx_list, trx_sys->mysql_trx_list, trx);
	
	mutex_exit(&kernel_mutex);

	trx->mysql_thread_id = os_thread_get_curr_id();

	trx->mysql_process_no = os_proc_get_number();
	
	return(trx);
}

/************************************************************************
Creates a transaction object for background operations by the master thread. */

trx_t*
trx_allocate_for_background(void)
/*=============================*/
				/* out, own: transaction object */
{
	trx_t*	trx;

	mutex_enter(&kernel_mutex);
	
	/* Open a dummy session */

	if (!trx_dummy_sess) {
		trx_dummy_sess = sess_open();
	}
	
	trx = trx_create(trx_dummy_sess);

	mutex_exit(&kernel_mutex);
	
	return(trx);
}

/************************************************************************
Releases the search latch if trx has reserved it. */

void
trx_search_latch_release_if_reserved(
/*=================================*/
        trx_t*     trx) /* in: transaction */
{
  	if (trx->has_search_latch) {
    		rw_lock_s_unlock(&btr_search_latch);

    		trx->has_search_latch = FALSE;
  	}
}

/************************************************************************
Frees a transaction object. */

void
trx_free(
/*=====*/
	trx_t*	trx)	/* in, own: trx object */
{
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */

	if (trx->declared_to_be_inside_innodb) {
	        ut_print_timestamp(stderr);
		fputs(
"  InnoDB: Error: Freeing a trx which is declared to be processing\n"
"InnoDB: inside InnoDB.\n", stderr);
		trx_print(stderr, trx);
		putc('\n', stderr);
	}

	ut_a(trx->magic_n == TRX_MAGIC_N);

	trx->magic_n = 11112222;

	ut_a(trx->conc_state == TRX_NOT_STARTED);
	
	mutex_free(&(trx->undo_mutex));

	ut_a(trx->insert_undo == NULL); 
	ut_a(trx->update_undo == NULL); 

	ut_a(trx->n_mysql_tables_in_use == 0);
	ut_a(trx->mysql_n_tables_locked == 0);
	
	if (trx->undo_no_arr) {
		trx_undo_arr_free(trx->undo_no_arr);
	}

	ut_a(UT_LIST_GET_LEN(trx->signals) == 0);
	ut_a(UT_LIST_GET_LEN(trx->reply_signals) == 0);

	ut_a(trx->wait_lock == NULL);
	ut_a(UT_LIST_GET_LEN(trx->wait_thrs) == 0);

	ut_a(!trx->has_search_latch);
	ut_a(!trx->auto_inc_lock);
	ut_a(!trx->n_lock_table_exp);

	ut_a(trx->dict_operation_lock_mode == 0);

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
	
	UT_LIST_REMOVE(mysql_trx_list, trx_sys->mysql_trx_list, trx);

	trx_free(trx);

	ut_a(trx_n_mysql_transactions > 0);

	trx_n_mysql_transactions--;
	
	mutex_exit(&kernel_mutex);
}

/************************************************************************
Frees a transaction object of a background operation of the master thread. */

void
trx_free_for_background(
/*====================*/
	trx_t*	trx)	/* in, own: trx object */
{
	mutex_enter(&kernel_mutex);
	
	trx_free(trx);
	
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

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */

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

			trx->id = undo->trx_id;
			trx->xid = undo->xid;
			trx->insert_undo = undo;
			trx->rseg = rseg;

			if (undo->state != TRX_UNDO_ACTIVE) {

				/* Prepared transactions are left in
				the prepared state waiting for a
				commit or abort decision from MySQL */

				if (undo->state == TRX_UNDO_PREPARED) {
					trx->conc_state = TRX_PREPARED;
				} else {
					trx->conc_state = 
						TRX_COMMITTED_IN_MEMORY;
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
				field inited to ut_dulint_max */

				trx->no = ut_dulint_max;
			}

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

				trx->id = undo->trx_id;
				trx->xid = undo->xid;

				if (undo->state != TRX_UNDO_ACTIVE) {

					/* Prepared transactions are left in
					the prepared state waiting for a
					commit or abort decision from MySQL */

					if (undo->state == TRX_UNDO_PREPARED) {
						trx->conc_state =
							TRX_PREPARED;
					} else {
						trx->conc_state =
							TRX_COMMITTED_IN_MEMORY;
					}

					/* We give a dummy value for the trx
					number */

					trx->no = trx->id;
				} else {
					trx->conc_state = TRX_ACTIVE;

					/* A running transaction always has
					the number field inited to
					ut_dulint_max */

					trx->no = ut_dulint_max;
				}

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

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */
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

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */
	ut_ad(trx->rseg == NULL);

	if (trx->type == TRX_PURGE) {
		trx->id = ut_dulint_zero;
		trx->conc_state = TRX_ACTIVE;
		trx->start_time = time(NULL);

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
	trx->start_time = time(NULL);

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
	
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */

	rseg = trx->rseg;
	
	if (trx->insert_undo != NULL || trx->update_undo != NULL) {

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

		/* Update the latest MySQL binlog name and offset info
		in trx sys header if MySQL binlogging is on or the database
		server is a MySQL replication slave */

		if (trx->mysql_log_file_name) {
			trx_sys_update_mysql_binlog_offset(
					trx->mysql_log_file_name,
					trx->mysql_log_offset,
					TRX_SYS_MYSQL_LOG_INFO, &mtr);
			trx->mysql_log_file_name = NULL;
		}

		if (trx->mysql_master_log_file_name[0] != '\0') {
			/* This database server is a MySQL replication slave */ 
			trx_sys_update_mysql_binlog_offset(
				trx->mysql_master_log_file_name,
				trx->mysql_master_log_pos,
				TRX_SYS_MYSQL_MASTER_LOG_INFO, &mtr);
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

	ut_ad(trx->conc_state == TRX_ACTIVE || trx->conc_state == TRX_PREPARED);

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */
	
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

/*	fprintf(stderr, "Trx %lu commit finished\n",
		ut_dulint_get_low(trx->id)); */

	if (must_flush_log) {

		mutex_exit(&kernel_mutex);
	
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

                If we are calling trx_commit() under MySQL's binlog mutex, we
                will delay possible log write and flush to a separate function
                trx_commit_complete_for_mysql(), which is only called when the
                thread has released the binlog mutex. This is to make the
                group commit algorithm to work. Otherwise, the MySQL binlog
                mutex would serialize all commits and prevent a group of
                transactions from gathering. */

                if (trx->flush_log_later) {
                        /* Do nothing yet */
                } else if (srv_flush_log_at_trx_commit == 0) {
                        /* Do nothing */
                } else if (srv_flush_log_at_trx_commit == 1) {
                        if (srv_unix_file_flush_method == SRV_UNIX_NOSYNC) {
                               /* Write the log but do not flush it to disk */

                               log_write_up_to(lsn, LOG_WAIT_ONE_GROUP, FALSE);
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
		
		/*-------------------------------------*/
	
		mutex_enter(&kernel_mutex);
	}

	/* Free savepoints */
	trx_roll_savepoints_free(trx, NULL);

	trx->conc_state = TRX_NOT_STARTED;
	trx->rseg = NULL;
	trx->undo_no = ut_dulint_zero;
	trx->last_sql_stat_start.least_undo_no = ut_dulint_zero;

	ut_ad(UT_LIST_GET_LEN(trx->wait_thrs) == 0);
	ut_ad(UT_LIST_GET_LEN(trx->trx_locks) == 0);

	UT_LIST_REMOVE(trx_list, trx_sys->trx_list, trx);
}

/********************************************************************
Cleans up a transaction at database startup. The cleanup is needed if
the transaction already got to the middle of a commit when the database
crashed, andf we cannot roll it back. */

void
trx_cleanup_at_db_startup(
/*======================*/
	trx_t*	trx)	/* in: transaction */
{
	if (trx->insert_undo != NULL) {

		trx_undo_insert_cleanup(trx);
	}

	trx->conc_state = TRX_NOT_STARTED;
	trx->rseg = NULL;
	trx->undo_no = ut_dulint_zero;
	trx->last_sql_stat_start.least_undo_no = ut_dulint_zero;

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
	
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */

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

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */
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

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */
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

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */
	
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

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */

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
	que_thr_t*	receiver_thr,	/* in: query thread which wants the
					reply, or NULL; if type is
					TRX_SIG_END_WAIT, this must be NULL */
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
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */

	if (!trx_sig_is_compatible(trx, type, sender)) {
		/* The signal is not compatible with the other signals in
		the queue: do nothing */

		ut_error;
		
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

		ut_error;
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
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */
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
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */

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

/********************************************************************
Send the reply message when a signal in the queue of the trx has been
handled. */

void
trx_sig_reply(
/*==========*/
	trx_sig_t*	sig,		/* in: signal */
	que_thr_t**	next_thr)	/* in/out: next query thread to run;
					if the value which is passed in is
					a pointer to a NULL pointer, then the
					calling function can start running
					a new query thread */
{
	trx_t*	receiver_trx;

	ut_ad(sig);
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */

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

/********************************************************************
Removes a signal object from the trx signal queue. */

void
trx_sig_remove(
/*===========*/
	trx_t*		trx,	/* in: trx handle */
	trx_sig_t*	sig)	/* in, own: signal */
{
	ut_ad(trx && sig);
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */

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
					TRX_SIG_SELF, thr, NULL, &next_thr);
		
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

	ut_a(trx);

	trx->op_info = "committing";
	
	trx_start_if_not_started(trx);

	mutex_enter(&kernel_mutex);

	trx_commit_off_kernel(trx);

	mutex_exit(&kernel_mutex);

	trx->op_info = "";
	
	return(0);
}

/**************************************************************************
If required, flushes the log to disk if we called trx_commit_for_mysql()
with trx->flush_log_later == TRUE. */

ulint
trx_commit_complete_for_mysql(
/*==========================*/
			/* out: 0 or error number */
	trx_t*	trx)	/* in: trx handle */
{
        dulint  lsn     = trx->commit_lsn;

        ut_a(trx);
	
	trx->op_info = "flushing log";

        if (srv_flush_log_at_trx_commit == 0) {
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

	trx->op_info = "";

        return(0);
}

/**************************************************************************
Marks the latest SQL statement ended. */

void
trx_mark_sql_stat_end(
/*==================*/
	trx_t*	trx)	/* in: trx handle */
{
	ut_a(trx);

	if (trx->conc_state == TRX_NOT_STARTED) {
		trx->undo_no = ut_dulint_zero;
	}

	trx->last_sql_stat_start.least_undo_no = trx->undo_no;
}

/**************************************************************************
Prints info about a transaction to the standard output. The caller must
own the kernel mutex and must have called
innobase_mysql_prepare_print_arbitrary_thd(), unless he knows that MySQL or
InnoDB cannot meanwhile change the info printed here. */

void
trx_print(
/*======*/
	FILE*	f,	/* in: output stream */
	trx_t*	trx)	/* in: transaction */
{
	ibool	newline;

	fprintf(f, "TRANSACTION %lu %lu",
		(ulong) ut_dulint_get_high(trx->id),
		 (ulong) ut_dulint_get_low(trx->id));

  	switch (trx->conc_state) {
		case TRX_NOT_STARTED:
			fputs(", not started", f);
			break;
		case TRX_ACTIVE:
			fprintf(f, ", ACTIVE %lu sec",
				(ulong)difftime(time(NULL), trx->start_time));
									break;
		case TRX_COMMITTED_IN_MEMORY:
			fputs(", COMMITTED IN MEMORY", f);
			break;
		default:
			fprintf(f, " state %lu", (ulong) trx->conc_state);
  	}

#ifdef UNIV_LINUX
	fprintf(f, ", process no %lu", trx->mysql_process_no);
#endif
	fprintf(f, ", OS thread id %lu",
		       (ulong) os_thread_pf(trx->mysql_thread_id));

	if (*trx->op_info) {
		putc(' ', f);
		fputs(trx->op_info, f);
	}
	
  	if (trx->type != TRX_USER) {
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

  	if (0 < UT_LIST_GET_LEN(trx->trx_locks) ||
	    mem_heap_get_size(trx->lock_heap) > 400) {
		newline = TRUE;

		fprintf(f, "%lu lock struct(s), heap size %lu",
			       (ulong) UT_LIST_GET_LEN(trx->trx_locks),
			       (ulong) mem_heap_get_size(trx->lock_heap));
	}

  	if (trx->has_search_latch) {
		newline = TRUE;
		fputs(", holds adaptive hash latch", f);
  	}

	if (ut_dulint_cmp(trx->undo_no, ut_dulint_zero) != 0) {
		newline = TRUE;
		fprintf(f, ", undo log entries %lu",
			(ulong) ut_dulint_get_low(trx->undo_no));
	}
	
	if (newline) {
		putc('\n', f);
	}

  	if (trx->mysql_thd != NULL) {
		innobase_mysql_print_thd(f, trx->mysql_thd);
  	}  
}

/********************************************************************
Prepares a transaction. */

void
trx_prepare_off_kernel(
/*==================*/
	trx_t*	trx)	/* in: transaction */
{
	page_t*		update_hdr_page;
	dulint		lsn;
	trx_rseg_t*	rseg;
	trx_undo_t*	undo;
	ibool		must_flush_log	= FALSE;
	mtr_t		mtr;
	
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */

	rseg = trx->rseg;
	
	if (trx->insert_undo != NULL || trx->update_undo != NULL) {

		mutex_exit(&kernel_mutex);

		mtr_start(&mtr);
		
		must_flush_log = TRUE;

		/* Change the undo log segment states from TRX_UNDO_ACTIVE
		to some other state: these modifications to the file data
		structure define the transaction as prepared in the file
		based world, at the serialization point of the log sequence
		number lsn obtained below. */

		mutex_enter(&(rseg->mutex));
			
		if (trx->insert_undo != NULL) {
			trx_undo_set_state_at_prepare(trx, trx->insert_undo,
							   &mtr);
		}

		undo = trx->update_undo;

		if (undo) {

			/* It is not necessary to obtain trx->undo_mutex here
			because only a single OS thread is allowed to do the
			transaction prepare for this transaction. */
					
			update_hdr_page = trx_undo_set_state_at_prepare(trx,								undo, &mtr);
		}

		mutex_exit(&(rseg->mutex));

		/*--------------*/
		mtr_commit(&mtr);
		/*--------------*/
		lsn = mtr.end_lsn;

		mutex_enter(&kernel_mutex);
	}

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */

	/*--------------------------------------*/
	trx->conc_state = TRX_PREPARED;
	/*--------------------------------------*/

	if (trx->read_view) {
		read_view_close(trx->read_view);

		mem_heap_empty(trx->read_view_heap);
		trx->read_view = NULL;
	}

	if (must_flush_log) {

		mutex_exit(&kernel_mutex);
	
		/* Write the log to the log files AND flush them to disk */

		/*-------------------------------------*/

		log_write_up_to(lsn, LOG_WAIT_ONE_GROUP, TRUE);

		/*-------------------------------------*/
	
		mutex_enter(&kernel_mutex);
	}
}

/**************************************************************************
Does the transaction prepare for MySQL. */

ulint
trx_prepare_for_mysql(
/*=================*/
			/* out: 0 or error number */
	trx_t*	trx)	/* in: trx handle */
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

/**************************************************************************
This function is used to find number of prepared transactions and
their transaction objects for a recovery. */

int
trx_recover_for_mysql(
/*==================*/
				/* out: number of prepared transactions 
				stored in xid_list */
	XID*    xid_list, 	/* in/out: prepared transactions */
	uint	len)		/* in: number of slots in xid_list */
{
	trx_t*	trx;
	int	num_of_transactions = 0;

	ut_ad(xid_list);
	ut_ad(len);

	fprintf(stderr,
		"InnoDB: Starting recovery for XA transactions...\n");


	/* We should set those transactions which are in
	the prepared state to the xid_list */

	mutex_enter(&kernel_mutex);

	trx = UT_LIST_GET_FIRST(trx_sys->trx_list);

	while (trx) {
		if (trx->conc_state == TRX_PREPARED) {
			xid_list[num_of_transactions] = trx->xid;

			fprintf(stderr,
"InnoDB: Transaction %lu %lu in prepared state after recovery\n",
				(ulong) ut_dulint_get_high(trx->id),
				(ulong) ut_dulint_get_low(trx->id));

			fprintf(stderr,
"InnoDB: Transaction contains changes to %lu rows\n",
			(ulong)ut_conv_dulint_to_longlong(trx->undo_no));

			num_of_transactions++;
		
			if ((uint)num_of_transactions == len ) {
				break;
			}
		}

		trx = UT_LIST_GET_NEXT(trx_list, trx);
	}

	mutex_exit(&kernel_mutex);

	fprintf(stderr,
		"InnoDB: %d transactions in prepare state after recovery\n",
		num_of_transactions);

	return (num_of_transactions);			
}

/***********************************************************************
This function is used to find one X/Open XA distributed transaction
which is in the prepared state */

trx_t * 
trx_get_trx_by_xid(
/*===============*/
			/* out: trx or NULL */
	XID*	xid)	/*  in: X/Open XA Transaction Idenfication */
{
	trx_t*	trx;

	if (xid == NULL) {
		return (NULL);
	}
 
	mutex_enter(&kernel_mutex);

	trx = UT_LIST_GET_FIRST(trx_sys->trx_list);

	while (trx) {
		/* Compare two X/Open XA transaction id's: their
		length should be the same and binary comparison
		of gtrid_lenght+bqual_length bytes should be
		the same */

		if (xid->gtrid_length == trx->xid.gtrid_length &&
		    xid->bqual_length == trx->xid.bqual_length &&
		    memcmp(xid, &trx->xid, 
				xid->gtrid_length + 
				xid->bqual_length) == 0) {
			break;
		}

		trx = UT_LIST_GET_NEXT(trx_list, trx);
	}

	mutex_exit(&kernel_mutex);

	if (trx) {
		if (trx->conc_state != TRX_PREPARED) {
			return(NULL);
		}

		return(trx);
	} else {
		return(NULL);
	}
}

