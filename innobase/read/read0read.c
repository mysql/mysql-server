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
view. Opens a new view if no views currently exist. The view must be
closed with ..._close. This is used in purge. */

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
	
	ut_ad(mutex_own(&kernel_mutex));

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

	ut_ad(mutex_own(&kernel_mutex));

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
		if (trx != cr_trx && trx->conc_state == TRX_ACTIVE) {

			read_view_set_nth_trx_id(view, n, trx->id);
		
			n++;

			/* NOTE that a transaction whose trx number is <
			trx_sys->max_trx_id can still be active, if it is
			in the middle of the commit! Note that when a
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
	ut_ad(mutex_own(&kernel_mutex));

	UT_LIST_REMOVE(view_list, trx_sys->view_list, view);
} 
