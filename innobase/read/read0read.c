/******************************************************
Cursor read

(c) 1997 Innobase Oy

Created 2/16/1997 Heikki Tuuri
*******************************************************/

#include "read0read.h"

#ifdef UNIV_NONINL
#include "read0read.ic"
#endif

#include "srv0srv.h"
#include "trx0sys.h"

/*************************************************************************
Creates a read view object. */
UNIV_INLINE
read_view_t*
read_view_create_low(
/*=================*/
				/* out, own: read view struct */
	ulint		n,	/* in: number of cells in the trx_ids array */
	mem_heap_t*	heap)	/* in: memory heap from which allocated */
{
	read_view_t*	view;

	view = mem_heap_alloc(heap, sizeof(read_view_t));

	view->n_trx_ids = n;
	view->trx_ids = mem_heap_alloc(heap, n * sizeof(dulint));

	return(view);
}

/*************************************************************************
Makes a copy of the oldest existing read view, with the exception that also
the creating trx of the oldest view is set as not visible in the 'copied'
view. Opens a new view if no views currently exist. The view must be closed
with ..._close. This is used in purge. */

read_view_t*
read_view_oldest_copy_or_open_new(
/*==============================*/
				/* out, own: read view struct */
	trx_t*		cr_trx,	/* in: creating transaction, or NULL */
	mem_heap_t*	heap)	/* in: memory heap from which allocated */
{
	read_view_t*	old_view;
	read_view_t*	view_copy;
	ibool		needs_insert	= TRUE;
	ulint		insert_done	= 0;
	ulint		n;
	ulint		i;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */
	old_view = UT_LIST_GET_LAST(trx_sys->view_list);

	if (old_view == NULL) {

		return(read_view_open_now(cr_trx, heap));
	}

	n = old_view->n_trx_ids;

	if (old_view->creator) {
		n++;
	} else {
		needs_insert = FALSE;
	}

	view_copy = read_view_create_low(n, heap);
	
	/* Insert the id of the creator in the right place of the descending
	array of ids, if needs_insert is TRUE: */

	i = 0;
	while (i < n) {
		if (needs_insert
		    && (i >= old_view->n_trx_ids
		     || ut_dulint_cmp(old_view->creator->id,
					read_view_get_nth_trx_id(old_view, i))
				> 0)) {

			read_view_set_nth_trx_id(view_copy, i,
						old_view->creator->id);
			needs_insert = FALSE;
			insert_done = 1;
		} else {
			read_view_set_nth_trx_id(view_copy, i,
				read_view_get_nth_trx_id(old_view,
							i - insert_done));
		}

		i++;
	}

	view_copy->creator = cr_trx;
	
  	view_copy->low_limit_no = old_view->low_limit_no;
	view_copy->low_limit_id = old_view->low_limit_id;

	view_copy->can_be_too_old = FALSE;

	if (n > 0) {
		/* The last active transaction has the smallest id: */
		view_copy->up_limit_id = read_view_get_nth_trx_id(
							view_copy, n - 1);
	} else {
		view_copy->up_limit_id = old_view->up_limit_id;
	}

	UT_LIST_ADD_LAST(view_list, trx_sys->view_list, view_copy);

	return(view_copy);
}

/*************************************************************************
Opens a read view where exactly the transactions serialized before this
point in time are seen in the view. */

read_view_t*
read_view_open_now(
/*===============*/
				/* out, own: read view struct */
	trx_t*		cr_trx,	/* in: creating transaction, or NULL */
	mem_heap_t*	heap)	/* in: memory heap from which allocated */
{
	read_view_t*	view;
	trx_t*		trx;
	ulint		n;
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */
	view = read_view_create_low(UT_LIST_GET_LEN(trx_sys->trx_list), heap);

	view->creator = cr_trx;

	/* No future transactions should be visible in the view */

  	view->low_limit_no = trx_sys->max_trx_id;
	view->low_limit_id = view->low_limit_no;

	view->can_be_too_old = FALSE;

	n = 0;
	trx = UT_LIST_GET_FIRST(trx_sys->trx_list);

	/* No active transaction should be visible, except cr_trx */

	while (trx) {
                if (trx != cr_trx && (trx->conc_state == TRX_ACTIVE ||
                                        trx->conc_state == TRX_PREPARED)) {

			read_view_set_nth_trx_id(view, n, trx->id);

			n++;

			/* NOTE that a transaction whose trx number is <
			trx_sys->max_trx_id can still be active, if it is
			in the middle of its commit! Note that when a
			transaction starts, we initialize trx->no to
			ut_dulint_max. */

			if (ut_dulint_cmp(view->low_limit_no, trx->no) > 0) {

				view->low_limit_no = trx->no;
			}	
		}

		trx = UT_LIST_GET_NEXT(trx_list, trx);
	}

	view->n_trx_ids = n;		

	if (n > 0) {
		/* The last active transaction has the smallest id: */
		view->up_limit_id = read_view_get_nth_trx_id(view, n - 1);
	} else {
		view->up_limit_id = view->low_limit_id;
	}

	UT_LIST_ADD_FIRST(view_list, trx_sys->view_list, view);
	
	return(view);
}

/*************************************************************************
Closes a read view. */

void
read_view_close(
/*============*/
	read_view_t*	view)	/* in: read view */
{
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */
	UT_LIST_REMOVE(view_list, trx_sys->view_list, view);
} 

/*************************************************************************
Closes a consistent read view for MySQL. This function is called at an SQL
statement end if the trx isolation level is <= TRX_ISO_READ_COMMITTED. */

void
read_view_close_for_mysql(
/*======================*/
	trx_t*	trx)	/* in: trx which has a read view */
{
	ut_a(trx->global_read_view);

	mutex_enter(&kernel_mutex);

	read_view_close(trx->global_read_view);

	mem_heap_empty(trx->global_read_view_heap);

	trx->read_view = NULL;
	trx->global_read_view = NULL;

	mutex_exit(&kernel_mutex);
}
	
/*************************************************************************
Prints a read view to stderr. */

void
read_view_print(
/*============*/
	read_view_t*	view)	/* in: read view */
{
	ulint	n_ids;
	ulint	i;
	
	fprintf(stderr, "Read view low limit trx n:o %lu %lu\n",
			(ulong) ut_dulint_get_high(view->low_limit_no),
			(ulong) ut_dulint_get_low(view->low_limit_no));

	fprintf(stderr, "Read view up limit trx id %lu %lu\n",
			(ulong) ut_dulint_get_high(view->up_limit_id),
			(ulong) ut_dulint_get_low(view->up_limit_id));		

	fprintf(stderr, "Read view low limit trx id %lu %lu\n",
			(ulong) ut_dulint_get_high(view->low_limit_id),
			(ulong) ut_dulint_get_low(view->low_limit_id));

	fprintf(stderr, "Read view individually stored trx ids:\n");

	n_ids = view->n_trx_ids;

	for (i = 0; i < n_ids; i++) {
		fprintf(stderr, "Read view trx id %lu %lu\n",
			(ulong) ut_dulint_get_high(read_view_get_nth_trx_id(view, i)),
			(ulong) ut_dulint_get_low(read_view_get_nth_trx_id(view, i)));
	}
}

/*************************************************************************
Create a consistent cursor view for mysql to be used in cursors. In this 
consistent read view modifications done by the creating transaction or future
transactions are not visible. */

cursor_view_t*
read_cursor_view_create_for_mysql(
/*==============================*/
	trx_t*	cr_trx)	/* in: trx where cursor view is created */
{
	cursor_view_t*	curview;
	read_view_t*	view;
	mem_heap_t*	heap;
	trx_t*		trx;
	ulint		n;

	ut_a(cr_trx);

	/* Use larger heap than in trx_create when creating a read_view 
	because cursors are quite long. */

	heap = mem_heap_create(512);

	curview = (cursor_view_t*) mem_heap_alloc(heap, sizeof(cursor_view_t));
	curview->heap = heap;

        /* Drop cursor tables from consideration when evaluating the need of
          auto-commit */
	curview->n_mysql_tables_in_use = cr_trx->n_mysql_tables_in_use;
	cr_trx->n_mysql_tables_in_use = 0;

	mutex_enter(&kernel_mutex);

	curview->read_view = read_view_create_low(
				UT_LIST_GET_LEN(trx_sys->trx_list), 
					curview->heap);

	view = curview->read_view;
	view->creator = cr_trx;

	/* No future transactions should be visible in the view */

  	view->low_limit_no = trx_sys->max_trx_id;
	view->low_limit_id = view->low_limit_no;

	view->can_be_too_old = FALSE;

	n = 0;
	trx = UT_LIST_GET_FIRST(trx_sys->trx_list);

	/* No active transaction should be visible, except cr_trx.
	This is quick fix for a bug 12456 and needs to be fixed when
	semi-consistent high-granularity read view is implemented. */

	while (trx) {
                if (trx != cr_trx && (trx->conc_state == TRX_ACTIVE ||
					trx->conc_state == TRX_PREPARED)) {

			read_view_set_nth_trx_id(view, n, trx->id);

			n++;

			/* NOTE that a transaction whose trx number is <
			trx_sys->max_trx_id can still be active, if it is
			in the middle of its commit! Note that when a
			transaction starts, we initialize trx->no to
			ut_dulint_max. */

			if (ut_dulint_cmp(view->low_limit_no, trx->no) > 0) {

				view->low_limit_no = trx->no;
			}	
		}

		trx = UT_LIST_GET_NEXT(trx_list, trx);
	}

	view->n_trx_ids = n;		

	if (n > 0) {
		/* The last active transaction has the smallest id: */
		view->up_limit_id = read_view_get_nth_trx_id(view, n - 1);
	} else {
		view->up_limit_id = view->low_limit_id;
	}

	UT_LIST_ADD_FIRST(view_list, trx_sys->view_list, view);
	
	mutex_exit(&kernel_mutex);

	return(curview);
}

/*************************************************************************
Close a given consistent cursor view for mysql and restore global read view
back to a transaction read view. */

void
read_cursor_view_close_for_mysql(
/*=============================*/
	trx_t*		trx,	/* in: trx */
	cursor_view_t*	curview)/* in: cursor view to be closed */
{
	ut_a(curview);
	ut_a(curview->read_view); 
	ut_a(curview->heap);

        /* Add cursor's tables to the global count of active tables that
          belong to this transaction */
	trx->n_mysql_tables_in_use += curview->n_mysql_tables_in_use;

	mutex_enter(&kernel_mutex);

	read_view_close(curview->read_view);
	trx->read_view = trx->global_read_view;

	mutex_exit(&kernel_mutex);

	mem_heap_free(curview->heap);
}
	
/*************************************************************************
This function sets a given consistent cursor view to a transaction
read view if given consistent cursor view is not NULL. Otherwise, function
restores a global read view to a transaction read view. */

void 
read_cursor_set_for_mysql(
/*======================*/
	trx_t*		trx,	/* in: transaction where cursor is set */
	cursor_view_t*	curview)/* in: consistent cursor view to be set */
{
	ut_a(trx);

	mutex_enter(&kernel_mutex);

	if (UNIV_LIKELY(curview != NULL)) {
		trx->read_view = curview->read_view;
	} else {
		trx->read_view = trx->global_read_view;
	}

	mutex_exit(&kernel_mutex);
}
