/******************************************************
Transaction rollback

(c) 1996 Innobase Oy

Created 3/26/1996 Heikki Tuuri
*******************************************************/

#include "trx0roll.h"

#ifdef UNIV_NONINL
#include "trx0roll.ic"
#endif

#include "fsp0fsp.h"
#include "mach0data.h"
#include "trx0rseg.h"
#include "trx0trx.h"
#include "trx0undo.h"
#include "trx0rec.h"
#include "que0que.h"
#include "usr0sess.h"
#include "srv0que.h"
#include "srv0start.h"
#include "row0undo.h"
#include "row0mysql.h"
#include "lock0lock.h"
#include "pars0pars.h"

/* This many pages must be undone before a truncate is tried within rollback */
#define TRX_ROLL_TRUNC_THRESHOLD	1

/* In crash recovery, the current trx to be rolled back */
trx_t*		trx_roll_crash_recv_trx	= NULL;

/* In crash recovery we set this to the undo n:o of the current trx to be
rolled back. Then we can print how many % the rollback has progressed. */
ib_longlong	trx_roll_max_undo_no;

/* Auxiliary variable which tells the previous progress % we printed */
ulint		trx_roll_progress_printed_pct;

/***********************************************************************
Rollback a transaction used in MySQL. */

int
trx_general_rollback_for_mysql(
/*===========================*/
				/* out: error code or DB_SUCCESS */
	trx_t*		trx,	/* in: transaction handle */
	ibool		partial,/* in: TRUE if partial rollback requested */
	trx_savept_t*	savept)	/* in: pointer to savepoint undo number, if
				partial rollback requested */
{
#ifndef UNIV_HOTBACKUP
	mem_heap_t*	heap;
	que_thr_t*	thr;
	roll_node_t*	roll_node;

	/* Tell Innobase server that there might be work for
	utility threads: */

	srv_active_wake_master_thread();

	trx_start_if_not_started(trx);

	heap = mem_heap_create(512);

	roll_node = roll_node_create(heap);

	roll_node->partial = partial;

	if (partial) {
		roll_node->savept = *savept;
	}

	trx->error_state = DB_SUCCESS;

	thr = pars_complete_graph_for_exec(roll_node, trx, heap);
	
	ut_a(thr == que_fork_start_command(que_node_get_parent(thr)));
	que_run_threads(thr);

	mutex_enter(&kernel_mutex);

	while (trx->que_state != TRX_QUE_RUNNING) {

		mutex_exit(&kernel_mutex);

		os_thread_sleep(100000);

		mutex_enter(&kernel_mutex);
	}

	mutex_exit(&kernel_mutex);

 	mem_heap_free(heap);

 	ut_a(trx->error_state == DB_SUCCESS);

	/* Tell Innobase server that there might be work for
	utility threads: */

	srv_active_wake_master_thread();

	return((int) trx->error_state);
#else /* UNIV_HOTBACKUP */
	/* This function depends on MySQL code that is not included in
	InnoDB Hot Backup builds.  Besides, this function should never
	be called in InnoDB Hot Backup. */
	ut_error;
#endif /* UNIV_HOTBACKUP */
}

/***********************************************************************
Rollback a transaction used in MySQL. */

int
trx_rollback_for_mysql(
/*===================*/
			/* out: error code or DB_SUCCESS */
	trx_t*	trx)	/* in: transaction handle */
{
	int	err;

	if (trx->conc_state == TRX_NOT_STARTED) {

		return(DB_SUCCESS);
	}

	trx->op_info = "rollback";
	
	err = trx_general_rollback_for_mysql(trx, FALSE, NULL);

	trx->op_info = "";

	return(err);
}	

/***********************************************************************
Rollback the latest SQL statement for MySQL. */

int
trx_rollback_last_sql_stat_for_mysql(
/*=================================*/
			/* out: error code or DB_SUCCESS */
	trx_t*	trx)	/* in: transaction handle */
{
	int	err;

	if (trx->conc_state == TRX_NOT_STARTED) {

		return(DB_SUCCESS);
	}

	trx->op_info = "rollback of SQL statement";
	
	err = trx_general_rollback_for_mysql(trx, TRUE,
						&(trx->last_sql_stat_start));
	/* The following call should not be needed, but we play safe: */
	trx_mark_sql_stat_end(trx);

	trx->op_info = "";
	
	return(err);
}

/***********************************************************************
Frees savepoint structs. */

void
trx_roll_savepoints_free(
/*=====================*/
	trx_t*			trx,	/* in: transaction handle */
	trx_named_savept_t*	savep)	/* in: free all savepoints > this one;
					if this is NULL, free all savepoints
					of trx */
{
	trx_named_savept_t*	next_savep;

	if (savep == NULL) {
	        savep = UT_LIST_GET_FIRST(trx->trx_savepoints);
	} else {
	        savep = UT_LIST_GET_NEXT(trx_savepoints, savep);
	}
	
	while (savep != NULL) {
	        next_savep = UT_LIST_GET_NEXT(trx_savepoints, savep);

		UT_LIST_REMOVE(trx_savepoints, trx->trx_savepoints, savep);
		mem_free(savep->name);
		mem_free(savep);

		savep = next_savep;
	}
}

/***********************************************************************
Rolls back a transaction back to a named savepoint. Modifications after the
savepoint are undone but InnoDB does NOT release the corresponding locks
which are stored in memory. If a lock is 'implicit', that is, a new inserted
row holds a lock where the lock information is carried by the trx id stored in 
the row, these locks are naturally released in the rollback. Savepoints which
were set after this savepoint are deleted. */

ulint
trx_rollback_to_savepoint_for_mysql(
/*================================*/
						/* out: if no savepoint
						of the name found then
						DB_NO_SAVEPOINT,
						otherwise DB_SUCCESS */
	trx_t*		trx,			/* in: transaction handle */
	const char*	savepoint_name,		/* in: savepoint name */
	ib_longlong*	mysql_binlog_cache_pos)	/* out: the MySQL binlog cache
						position corresponding to this
						savepoint; MySQL needs this
						information to remove the
						binlog entries of the queries
						executed after the savepoint */
{
	trx_named_savept_t*	savep;
	ulint			err;

	savep = UT_LIST_GET_FIRST(trx->trx_savepoints);

	while (savep != NULL) {
	        if (0 == ut_strcmp(savep->name, savepoint_name)) {
		        /* Found */
			break;
		}
	        savep = UT_LIST_GET_NEXT(trx_savepoints, savep);
	}

	if (savep == NULL) {	

	        return(DB_NO_SAVEPOINT);
	}

	if (trx->conc_state == TRX_NOT_STARTED) {
		ut_print_timestamp(stderr);
		fputs("  InnoDB: Error: transaction has a savepoint ", stderr);
		ut_print_name(stderr, trx, savep->name);
		fputs(" though it is not started\n", stderr);
	        return(DB_ERROR);
	}

	/* We can now free all savepoints strictly later than this one */

	trx_roll_savepoints_free(trx, savep);

	*mysql_binlog_cache_pos = savep->mysql_binlog_cache_pos;

	trx->op_info = "rollback to a savepoint";
	
	err = trx_general_rollback_for_mysql(trx, TRUE, &(savep->savept));

	/* Store the current undo_no of the transaction so that we know where
	to roll back if we have to roll back the next SQL statement: */

	trx_mark_sql_stat_end(trx);

	trx->op_info = "";

	return(err);
}

/***********************************************************************
Creates a named savepoint. If the transaction is not yet started, starts it.
If there is already a savepoint of the same name, this call erases that old
savepoint and replaces it with a new. Savepoints are deleted in a transaction
commit or rollback. */

ulint
trx_savepoint_for_mysql(
/*====================*/
						/* out: always DB_SUCCESS */
	trx_t*		trx,			/* in: transaction handle */
	const char*	savepoint_name,		/* in: savepoint name */
	ib_longlong	binlog_cache_pos)	/* in: MySQL binlog cache
						position corresponding to this
						connection at the time of the
						savepoint */
{
	trx_named_savept_t*	savep;

	ut_a(trx);
	ut_a(savepoint_name);

	trx_start_if_not_started(trx);

	savep = UT_LIST_GET_FIRST(trx->trx_savepoints);

	while (savep != NULL) {
	        if (0 == ut_strcmp(savep->name, savepoint_name)) {
		        /* Found */
			break;
		}
	        savep = UT_LIST_GET_NEXT(trx_savepoints, savep);
	}

	if (savep) {
	        /* There is a savepoint with the same name: free that */

		UT_LIST_REMOVE(trx_savepoints, trx->trx_savepoints, savep);
		
		mem_free(savep->name);
		mem_free(savep);
	}

	/* Create a new savepoint and add it as the last in the list */

	savep = mem_alloc(sizeof(trx_named_savept_t));

	savep->name = mem_strdup(savepoint_name);

	savep->savept = trx_savept_take(trx);

	savep->mysql_binlog_cache_pos = binlog_cache_pos;

	UT_LIST_ADD_LAST(trx_savepoints, trx->trx_savepoints, savep);

	return(DB_SUCCESS);
}

/***********************************************************************
Releases a named savepoint. Savepoints which
were set after this savepoint are deleted. */

ulint
trx_release_savepoint_for_mysql(
/*============================*/
						/* out: if no savepoint
						of the name found then
						DB_NO_SAVEPOINT,
						otherwise DB_SUCCESS */
	trx_t*		trx,			/* in: transaction handle */
	const char*	savepoint_name)		/* in: savepoint name */
{
	trx_named_savept_t*	savep;

	savep = UT_LIST_GET_FIRST(trx->trx_savepoints);

	while (savep != NULL) {
	        if (0 == ut_strcmp(savep->name, savepoint_name)) {
		        /* Found */
			break;
		}
	        savep = UT_LIST_GET_NEXT(trx_savepoints, savep);
	}

	if (savep == NULL) {	

	        return(DB_NO_SAVEPOINT);
	}

	/* We can now free all savepoints strictly later than this one */

	trx_roll_savepoints_free(trx, savep);
	
	/* Now we can free this savepoint too */

	UT_LIST_REMOVE(trx_savepoints, trx->trx_savepoints, savep);

	mem_free(savep->name);
	mem_free(savep);

	return(DB_SUCCESS);
}

/***********************************************************************
Returns a transaction savepoint taken at this point in time. */

trx_savept_t
trx_savept_take(
/*============*/
			/* out: savepoint */
	trx_t*	trx)	/* in: transaction */
{
	trx_savept_t	savept;

	savept.least_undo_no = trx->undo_no;

	return(savept);
}

/***********************************************************************
Rollback or clean up transactions which have no user session. If the
transaction already was committed, then we clean up a possible insert
undo log. If the transaction was not yet committed, then we roll it back. 
Note: this is done in a background thread. */

#ifndef __WIN__
void*
#else
ulint
#endif
trx_rollback_or_clean_all_without_sess(
/*===================================*/
                        /* out: a dummy parameter */
        void*   arg __attribute__((unused)))
                        /* in: a dummy parameter required by
                        os_thread_create */
{
	mem_heap_t*	heap;
	que_fork_t*	fork;
	que_thr_t*	thr;
	roll_node_t*	roll_node;
	trx_t*		trx;
	dict_table_t*	table;
	ib_longlong	rows_to_undo;
	const char*	unit		= "";
	int		err;

	mutex_enter(&kernel_mutex);
	
	/* Open a dummy session */

	if (!trx_dummy_sess) {
		trx_dummy_sess = sess_open();
	}
	
	mutex_exit(&kernel_mutex);

	if (UT_LIST_GET_FIRST(trx_sys->trx_list)) {

		fprintf(stderr,
"InnoDB: Starting in background the rollback of uncommitted transactions\n");
	} else {		
		goto leave_function;
	}
loop:
	heap = mem_heap_create(512);

	mutex_enter(&kernel_mutex);

	trx = UT_LIST_GET_FIRST(trx_sys->trx_list);

	while (trx) {
		if ((trx->sess || (trx->conc_state == TRX_NOT_STARTED))) {
			trx = UT_LIST_GET_NEXT(trx_list, trx);
		} else if (trx->conc_state == TRX_PREPARED) {

			trx->sess = trx_dummy_sess;
			trx = UT_LIST_GET_NEXT(trx_list, trx);
		} else {
			break;
		}
	}
	
	mutex_exit(&kernel_mutex);

	if (trx == NULL) {
		ut_print_timestamp(stderr);
		fprintf(stderr,
		"  InnoDB: Rollback of non-prepared transactions completed\n");

 		mem_heap_free(heap);

		goto leave_function;
	}

	trx->sess = trx_dummy_sess;

	if (trx->conc_state == TRX_COMMITTED_IN_MEMORY) {	
		fprintf(stderr, "InnoDB: Cleaning up trx with id %lu %lu\n",
					(ulong) ut_dulint_get_high(trx->id),
					(ulong) ut_dulint_get_low(trx->id));

		trx_cleanup_at_db_startup(trx);
					
		mem_heap_free(heap);

		goto loop;
	}

	fork = que_fork_create(NULL, NULL, QUE_FORK_RECOVERY, heap);
	fork->trx = trx;

	thr = que_thr_create(fork, heap);

	roll_node = roll_node_create(heap);

	thr->child = roll_node;
	roll_node->common.parent = thr;

	mutex_enter(&kernel_mutex);	

	trx->graph = fork;

	ut_a(thr == que_fork_start_command(fork));
	
	trx_roll_crash_recv_trx	= trx;
	trx_roll_max_undo_no = ut_conv_dulint_to_longlong(trx->undo_no);
	trx_roll_progress_printed_pct = 0;
	rows_to_undo = trx_roll_max_undo_no;

	if (rows_to_undo > 1000000000) {
		rows_to_undo = rows_to_undo / 1000000;
		unit = "M";
	}

	ut_print_timestamp(stderr);
	fprintf(stderr,
"  InnoDB: Rolling back trx with id %lu %lu, %lu%s rows to undo\n",
					(ulong) ut_dulint_get_high(trx->id),
					(ulong) ut_dulint_get_low(trx->id),
					(ulong) rows_to_undo, unit);
	mutex_exit(&kernel_mutex);

	trx->mysql_thread_id = os_thread_get_curr_id();

	trx->mysql_process_no = os_proc_get_number();

	if (trx->dict_operation) {
		row_mysql_lock_data_dictionary(trx);
	}

	que_run_threads(thr);

	mutex_enter(&kernel_mutex);

	while (trx->que_state != TRX_QUE_RUNNING) {

		mutex_exit(&kernel_mutex);

		fprintf(stderr,
		"InnoDB: Waiting for rollback of trx id %lu to end\n",
					(ulong) ut_dulint_get_low(trx->id));
		os_thread_sleep(100000);

		mutex_enter(&kernel_mutex);
	}

	mutex_exit(&kernel_mutex);

	if (trx->dict_operation) {
		/* If the transaction was for a dictionary operation, we
		drop the relevant table, if it still exists */

		fprintf(stderr,
"InnoDB: Dropping table with id %lu %lu in recovery if it exists\n",
			(ulong) ut_dulint_get_high(trx->table_id),
			(ulong) ut_dulint_get_low(trx->table_id));

		table = dict_table_get_on_id_low(trx->table_id, trx);

		if (table) {		
			fputs("InnoDB: Table found: dropping table ", stderr);
			ut_print_name(stderr, trx, table->name);
			fputs(" in recovery\n", stderr);

			err = row_drop_table_for_mysql(table->name, trx, TRUE);

			ut_a(err == (int) DB_SUCCESS);
		}
	}

	if (trx->dict_operation) {
		row_mysql_unlock_data_dictionary(trx);
	}

	fprintf(stderr, "\nInnoDB: Rolling back of trx id %lu %lu completed\n",
					(ulong) ut_dulint_get_high(trx->id),
					(ulong) ut_dulint_get_low(trx->id));
	mem_heap_free(heap);

	trx_roll_crash_recv_trx	= NULL;

	goto loop;

leave_function:
	/* We count the number of threads in os_thread_exit(). A created
	thread should always use that to exit and not use return() to exit. */

	os_thread_exit(NULL);

	/* The following is dummy code to keep the compiler happy: */

#ifndef __WIN__
        return(NULL);
#else
        return(0);
#endif
}
	
/***********************************************************************
Creates an undo number array. */

trx_undo_arr_t*
trx_undo_arr_create(void)
/*=====================*/
{
	trx_undo_arr_t*	arr;
	mem_heap_t*	heap;
	ulint		i;
	
	heap = mem_heap_create(1024);

	arr = mem_heap_alloc(heap, sizeof(trx_undo_arr_t));

	arr->infos = mem_heap_alloc(heap, sizeof(trx_undo_inf_t)
						* UNIV_MAX_PARALLELISM);
	arr->n_cells = UNIV_MAX_PARALLELISM;
	arr->n_used = 0;

	arr->heap = heap;

	for (i = 0; i < UNIV_MAX_PARALLELISM; i++) {

		(trx_undo_arr_get_nth_info(arr, i))->in_use = FALSE;
	}

	return(arr);
}

/***********************************************************************
Frees an undo number array. */

void
trx_undo_arr_free(
/*==============*/
	trx_undo_arr_t*	arr)	/* in: undo number array */
{
	ut_ad(arr->n_used == 0);

	mem_heap_free(arr->heap);
}

/***********************************************************************
Stores info of an undo log record to the array if it is not stored yet. */
static
ibool
trx_undo_arr_store_info(
/*====================*/
			/* out: FALSE if the record already existed in the
			array */
	trx_t*	trx,	/* in: transaction */
	dulint	undo_no)/* in: undo number */
{
	trx_undo_inf_t*	cell;
	trx_undo_inf_t*	stored_here;
	trx_undo_arr_t*	arr;
	ulint		n_used;
	ulint		n;
	ulint		i;

	n = 0;
	arr = trx->undo_no_arr;
	n_used = arr->n_used;
	stored_here = NULL;
	
	for (i = 0;; i++) {
		cell = trx_undo_arr_get_nth_info(arr, i);

		if (!cell->in_use) {
			if (!stored_here) {
				/* Not in use, we may store here */
				cell->undo_no = undo_no;
				cell->in_use = TRUE;

				arr->n_used++;

				stored_here = cell;
			}
		} else {
			n++;

			if (0 == ut_dulint_cmp(cell->undo_no, undo_no)) {

				if (stored_here) {
					stored_here->in_use = FALSE;
					ut_ad(arr->n_used > 0);
					arr->n_used--;
				}

				ut_ad(arr->n_used == n_used);

				return(FALSE);
			}
		}
		
		if (n == n_used && stored_here) {

			ut_ad(arr->n_used == 1 + n_used);

			return(TRUE);
		}
	}
}

/***********************************************************************
Removes an undo number from the array. */
static
void
trx_undo_arr_remove_info(
/*=====================*/
	trx_undo_arr_t*	arr,	/* in: undo number array */
	dulint		undo_no)/* in: undo number */
{
	trx_undo_inf_t*	cell;
	ulint		n_used;
	ulint		n;
	ulint		i;

	n_used = arr->n_used;
	n = 0;

	for (i = 0;; i++) {
		cell = trx_undo_arr_get_nth_info(arr, i);

		if (cell->in_use
			     && 0 == ut_dulint_cmp(cell->undo_no, undo_no)) {

			cell->in_use = FALSE;
				
			ut_ad(arr->n_used > 0);

			arr->n_used--;

			return;
		}
	}
}

/***********************************************************************
Gets the biggest undo number in an array. */
static
dulint
trx_undo_arr_get_biggest(
/*=====================*/
				/* out: biggest value, ut_dulint_zero if
				the array is empty */
	trx_undo_arr_t*	arr)	/* in: undo number array */
{
	trx_undo_inf_t*	cell;
	ulint		n_used;
	dulint		biggest;
	ulint		n;
	ulint		i;
	
	n = 0;
	n_used = arr->n_used;
	biggest = ut_dulint_zero;
	
	for (i = 0;; i++) {
		cell = trx_undo_arr_get_nth_info(arr, i);

		if (cell->in_use) {
			n++;
			if (ut_dulint_cmp(cell->undo_no, biggest) > 0) {

				biggest = cell->undo_no;
			}
		}
		
		if (n == n_used) {
			return(biggest);
		}
	}
}

/***************************************************************************
Tries truncate the undo logs. */

void
trx_roll_try_truncate(
/*==================*/
	trx_t*	trx)	/* in: transaction */
{
	trx_undo_arr_t*	arr;
	dulint		limit;
	dulint		biggest;
	
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(trx->undo_mutex)));
	ut_ad(mutex_own(&((trx->rseg)->mutex)));
#endif /* UNIV_SYNC_DEBUG */

	trx->pages_undone = 0;
	
	arr = trx->undo_no_arr;

	limit = trx->undo_no;

	if (arr->n_used > 0) {
		biggest = trx_undo_arr_get_biggest(arr);

	    	if (ut_dulint_cmp(biggest, limit) >= 0) {

	    		limit = ut_dulint_add(biggest, 1);
	    	}
	}

	if (trx->insert_undo) {
		trx_undo_truncate_end(trx, trx->insert_undo, limit);
	}

	if (trx->update_undo) {
		trx_undo_truncate_end(trx, trx->update_undo, limit);
	}
}

/***************************************************************************
Pops the topmost undo log record in a single undo log and updates the info
about the topmost record in the undo log memory struct. */
static
trx_undo_rec_t*
trx_roll_pop_top_rec(
/*=================*/
				/* out: undo log record, the page s-latched */
	trx_t*		trx,	/* in: transaction */
	trx_undo_t*	undo,	/* in: undo log */
	mtr_t*		mtr)	/* in: mtr */
{
	page_t* 	undo_page;
	ulint		offset;
	trx_undo_rec_t*	prev_rec;
	page_t*		prev_rec_page;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(trx->undo_mutex)));
#endif /* UNIV_SYNC_DEBUG */

	undo_page = trx_undo_page_get_s_latched(undo->space,
						undo->top_page_no, mtr);
	offset = undo->top_offset;

/*	fprintf(stderr, "Thread %lu undoing trx %lu undo record %lu\n",
		os_thread_get_curr_id(), ut_dulint_get_low(trx->id),
		ut_dulint_get_low(undo->top_undo_no)); */

	prev_rec = trx_undo_get_prev_rec(undo_page + offset,
					undo->hdr_page_no, undo->hdr_offset,
									mtr);
	if (prev_rec == NULL) {

		undo->empty = TRUE;
	} else {
		prev_rec_page = buf_frame_align(prev_rec);
	
		if (prev_rec_page != undo_page) {

			trx->pages_undone++;
		}
	
		undo->top_page_no = buf_frame_get_page_no(prev_rec_page);
		undo->top_offset  = prev_rec - prev_rec_page;
		undo->top_undo_no = trx_undo_rec_get_undo_no(prev_rec);
	}

	return(undo_page + offset);
}

/************************************************************************
Pops the topmost record when the two undo logs of a transaction are seen
as a single stack of records ordered by their undo numbers. Inserts the
undo number of the popped undo record to the array of currently processed
undo numbers in the transaction. When the query thread finishes processing
of this undo record, it must be released with trx_undo_rec_release. */

trx_undo_rec_t*
trx_roll_pop_top_rec_of_trx(
/*========================*/
				/* out: undo log record copied to heap, NULL
				if none left, or if the undo number of the
				top record would be less than the limit */
	trx_t*		trx,	/* in: transaction */
	dulint		limit,	/* in: least undo number we need */
	dulint*		roll_ptr,/* out: roll pointer to undo record */
	mem_heap_t*	heap)	/* in: memory heap where copied */
{
	trx_undo_t*	undo;
	trx_undo_t*	ins_undo;
	trx_undo_t*	upd_undo;
	trx_undo_rec_t*	undo_rec;
	trx_undo_rec_t*	undo_rec_copy;
	dulint		undo_no;
	ibool		is_insert;
	trx_rseg_t*	rseg;
	ulint		progress_pct;
	mtr_t		mtr;
	
	rseg = trx->rseg;
try_again:
	mutex_enter(&(trx->undo_mutex));

	if (trx->pages_undone >= TRX_ROLL_TRUNC_THRESHOLD) {
		mutex_enter(&(rseg->mutex));

		trx_roll_try_truncate(trx);

		mutex_exit(&(rseg->mutex));
	}

	ins_undo = trx->insert_undo;
	upd_undo = trx->update_undo;

	if (!ins_undo || ins_undo->empty) {
		undo = upd_undo;
	} else if (!upd_undo || upd_undo->empty) {
		undo = ins_undo;
	} else if (ut_dulint_cmp(upd_undo->top_undo_no,
				 ins_undo->top_undo_no) > 0) {
		undo = upd_undo;
	} else {
		undo = ins_undo;
	}

	if (!undo || undo->empty
			|| (ut_dulint_cmp(limit, undo->top_undo_no) > 0)) {

	    	if ((trx->undo_no_arr)->n_used == 0) {
			/* Rollback is ending */
			
			mutex_enter(&(rseg->mutex));

			trx_roll_try_truncate(trx);

			mutex_exit(&(rseg->mutex));
		}

		mutex_exit(&(trx->undo_mutex));

		return(NULL);
	}

	if (undo == ins_undo) {
		is_insert = TRUE;
	} else {
		is_insert = FALSE;
	}
	
	*roll_ptr = trx_undo_build_roll_ptr(is_insert, (undo->rseg)->id,
					undo->top_page_no, undo->top_offset);
	mtr_start(&mtr);

	undo_rec = trx_roll_pop_top_rec(trx, undo, &mtr);

	undo_no = trx_undo_rec_get_undo_no(undo_rec);

	ut_ad(ut_dulint_cmp(ut_dulint_add(undo_no, 1), trx->undo_no) == 0);

	/* We print rollback progress info if we are in a crash recovery
	and the transaction has at least 1000 row operations to undo. */

	if (trx == trx_roll_crash_recv_trx && trx_roll_max_undo_no > 1000) {

	  	progress_pct = 100 - (ulint)
				((ut_conv_dulint_to_longlong(undo_no) * 100)
				/ trx_roll_max_undo_no);
		if (progress_pct != trx_roll_progress_printed_pct) {
			if (trx_roll_progress_printed_pct == 0) {
				fprintf(stderr,
"\nInnoDB: Progress in percents: %lu", (ulong) progress_pct);
			} else {
				fprintf(stderr,
				" %lu", (ulong) progress_pct);
			}
			fflush(stderr);
			trx_roll_progress_printed_pct = progress_pct;
		}
	}

	trx->undo_no = undo_no;

	if (!trx_undo_arr_store_info(trx, undo_no)) {
		/* A query thread is already processing this undo log record */

		mutex_exit(&(trx->undo_mutex));

		mtr_commit(&mtr);
		
		goto try_again;
	}

	undo_rec_copy = trx_undo_rec_copy(undo_rec, heap);
	
	mutex_exit(&(trx->undo_mutex));

	mtr_commit(&mtr);
	
	return(undo_rec_copy);
}

/************************************************************************
Reserves an undo log record for a query thread to undo. This should be
called if the query thread gets the undo log record not using the pop
function above. */

ibool
trx_undo_rec_reserve(
/*=================*/
			/* out: TRUE if succeeded */
	trx_t*	trx,	/* in: transaction */
	dulint	undo_no)/* in: undo number of the record */
{
	ibool	ret;
	
	mutex_enter(&(trx->undo_mutex));

	ret = trx_undo_arr_store_info(trx, undo_no);
	
	mutex_exit(&(trx->undo_mutex));

	return(ret);
}

/***********************************************************************
Releases a reserved undo record. */

void
trx_undo_rec_release(
/*=================*/
	trx_t*	trx,	/* in: transaction */
	dulint	undo_no)/* in: undo number */
{
	trx_undo_arr_t*	arr;
	
	mutex_enter(&(trx->undo_mutex));

	arr = trx->undo_no_arr;

	trx_undo_arr_remove_info(arr, undo_no);

	mutex_exit(&(trx->undo_mutex));
}

/*************************************************************************
Starts a rollback operation. */	

void
trx_rollback(
/*=========*/
	trx_t*		trx,	/* in: transaction */
	trx_sig_t*	sig,	/* in: signal starting the rollback */
	que_thr_t**	next_thr)/* in/out: next query thread to run;
				if the value which is passed in is
				a pointer to a NULL pointer, then the
				calling function can start running
				a new query thread; if the passed value is
				NULL, the parameter is ignored */
{
	que_t*		roll_graph;
	que_thr_t*	thr;
/*	que_thr_t*	thr2; */

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */
	ut_ad((trx->undo_no_arr == NULL) || ((trx->undo_no_arr)->n_used == 0));
	
	/* Initialize the rollback field in the transaction */

	if (sig->type == TRX_SIG_TOTAL_ROLLBACK) {

		trx->roll_limit = ut_dulint_zero;

	} else if (sig->type == TRX_SIG_ROLLBACK_TO_SAVEPT) {
		
		trx->roll_limit = (sig->savept).least_undo_no;

	} else if (sig->type == TRX_SIG_ERROR_OCCURRED) {

		trx->roll_limit = trx->last_sql_stat_start.least_undo_no;
	} else {
		ut_error;
	}

	ut_a(ut_dulint_cmp(trx->roll_limit, trx->undo_no) <= 0);

	trx->pages_undone = 0;

	if (trx->undo_no_arr == NULL) {
		trx->undo_no_arr = trx_undo_arr_create();
	}
	
	/* Build a 'query' graph which will perform the undo operations */

	roll_graph = trx_roll_graph_build(trx);

	trx->graph = roll_graph;
	trx->que_state = TRX_QUE_ROLLING_BACK;

	thr = que_fork_start_command(roll_graph);

	ut_ad(thr);

/*	thr2 = que_fork_start_command(roll_graph);

	ut_ad(thr2); */
	
	if (next_thr && (*next_thr == NULL)) {
		*next_thr = thr;
/*		srv_que_task_enqueue_low(thr2); */
	} else {
		srv_que_task_enqueue_low(thr);
/*		srv_que_task_enqueue_low(thr2); */
	}
}

/********************************************************************
Builds an undo 'query' graph for a transaction. The actual rollback is
performed by executing this query graph like a query subprocedure call.
The reply about the completion of the rollback will be sent by this
graph. */

que_t*
trx_roll_graph_build(
/*=================*/
			/* out, own: the query graph */
	trx_t*	trx)	/* in: trx handle */
{
	mem_heap_t*	heap;
	que_fork_t*	fork;
	que_thr_t*	thr;
/*	que_thr_t*	thr2; */

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */

	heap = mem_heap_create(512);
	fork = que_fork_create(NULL, NULL, QUE_FORK_ROLLBACK, heap);
	fork->trx = trx;

	thr = que_thr_create(fork, heap);
/*	thr2 = que_thr_create(fork, heap); */

	thr->child = row_undo_node_create(trx, thr, heap);  
/*	thr2->child = row_undo_node_create(trx, thr2, heap); */

	return(fork);
}

/*************************************************************************
Finishes error processing after the necessary partial rollback has been
done. */
static
void
trx_finish_error_processing(
/*========================*/
	trx_t*	trx)	/* in: transaction */
{
	trx_sig_t*	sig;
	trx_sig_t*	next_sig;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */

	sig = UT_LIST_GET_FIRST(trx->signals);

	while (sig != NULL) {
		next_sig = UT_LIST_GET_NEXT(signals, sig);

		if (sig->type == TRX_SIG_ERROR_OCCURRED) {

			trx_sig_remove(trx, sig);
		}

		sig = next_sig;
	}

	trx->que_state = TRX_QUE_RUNNING;
}

/*************************************************************************
Finishes a partial rollback operation. */
static
void
trx_finish_partial_rollback_off_kernel(
/*===================================*/
	trx_t*		trx,	/* in: transaction */
	que_thr_t**	next_thr)/* in/out: next query thread to run;
				if the value which is passed in is a pointer
				to a NULL pointer, then the calling function
				can start running a new query thread; if this
				parameter is NULL, it is ignored */
{
	trx_sig_t*	sig;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */

	sig = UT_LIST_GET_FIRST(trx->signals);

	/* Remove the signal from the signal queue and send reply message
	to it */

	trx_sig_reply(sig, next_thr);
	trx_sig_remove(trx, sig);

	trx->que_state = TRX_QUE_RUNNING;
}

/********************************************************************
Finishes a transaction rollback. */

void
trx_finish_rollback_off_kernel(
/*===========================*/
	que_t*		graph,	/* in: undo graph which can now be freed */
	trx_t*		trx,	/* in: transaction */
	que_thr_t**	next_thr)/* in/out: next query thread to run;
				if the value which is passed in is
				a pointer to a NULL pointer, then the
   				calling function can start running
				a new query thread; if this parameter is
				NULL, it is ignored */
{
	trx_sig_t*	sig;
	trx_sig_t*	next_sig;
	
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */

	ut_a(trx->undo_no_arr == NULL || trx->undo_no_arr->n_used == 0);

	/* Free the memory reserved by the undo graph */
	que_graph_free(graph);

	sig = UT_LIST_GET_FIRST(trx->signals);

	if (sig->type == TRX_SIG_ROLLBACK_TO_SAVEPT) {

		trx_finish_partial_rollback_off_kernel(trx, next_thr);

		return;

	} else if (sig->type == TRX_SIG_ERROR_OCCURRED) {

		trx_finish_error_processing(trx);

		return;
	}

#ifdef UNIV_DEBUG
	if (lock_print_waits) {			
		fprintf(stderr, "Trx %lu rollback finished\n",
						(ulong) ut_dulint_get_low(trx->id));
	}
#endif /* UNIV_DEBUG */

	trx_commit_off_kernel(trx);

	/* Remove all TRX_SIG_TOTAL_ROLLBACK signals from the signal queue and
	send reply messages to them */

	trx->que_state = TRX_QUE_RUNNING;
	
	while (sig != NULL) {
		next_sig = UT_LIST_GET_NEXT(signals, sig);

		if (sig->type == TRX_SIG_TOTAL_ROLLBACK) {

			trx_sig_reply(sig, next_thr);

			trx_sig_remove(trx, sig);
		}

		sig = next_sig;
	}
}

/*************************************************************************
Creates a rollback command node struct. */

roll_node_t*
roll_node_create(
/*=============*/
				/* out, own: rollback node struct */
	mem_heap_t*	heap)	/* in: mem heap where created */
{
	roll_node_t*	node;

	node = mem_heap_alloc(heap, sizeof(roll_node_t));
	node->common.type = QUE_NODE_ROLLBACK;
	node->state = ROLL_NODE_SEND;

	node->partial = FALSE;
	
	return(node);
}

/***************************************************************
Performs an execution step for a rollback command node in a query graph. */

que_thr_t*
trx_rollback_step(
/*==============*/
				/* out: query thread to run next, or NULL */
	que_thr_t*	thr)	/* in: query thread */
{
	roll_node_t*	node;
	ibool		success;
	ulint		sig_no;
	trx_savept_t*	savept;
	
	node = thr->run_node;

	ut_ad(que_node_get_type(node) == QUE_NODE_ROLLBACK);

	if (thr->prev_node == que_node_get_parent(node)) {
		node->state = ROLL_NODE_SEND;
	}

	if (node->state == ROLL_NODE_SEND) {
		mutex_enter(&kernel_mutex);

		node->state = ROLL_NODE_WAIT;

		if (node->partial) {
			sig_no = TRX_SIG_ROLLBACK_TO_SAVEPT;
			savept = &(node->savept);
		} else {
			sig_no = TRX_SIG_TOTAL_ROLLBACK;
			savept = NULL;
		}

		/* Send a rollback signal to the transaction */

		success = trx_sig_send(thr_get_trx(thr),
					sig_no, TRX_SIG_SELF,
					thr, savept, NULL);

		thr->state = QUE_THR_SIG_REPLY_WAIT;
		
		mutex_exit(&kernel_mutex);

		if (!success) {
			/* Error in delivering the rollback signal */
			que_thr_handle_error(thr, DB_ERROR, NULL, 0);
		}

		return(NULL);
	}

	ut_ad(node->state == ROLL_NODE_WAIT);
		
	thr->run_node = que_node_get_parent(node);
	
	return(thr);
}
