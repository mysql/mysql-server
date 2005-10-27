/******************************************************
Sessions

(c) 1996 Innobase Oy

Created 6/25/1996 Heikki Tuuri
*******************************************************/

#include "usr0sess.h"

#ifdef UNIV_NONINL
#include "usr0sess.ic"
#endif

#include "trx0trx.h"

/*************************************************************************
Closes a session, freeing the memory occupied by it. */
static
void
sess_close(
/*=======*/
	sess_t*		sess);	/* in, own: session object */

/*************************************************************************
Opens a session. */

sess_t*
sess_open(void)
/*===========*/
					/* out, own: session object */
{	
	sess_t*	sess;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */
	sess = mem_alloc(sizeof(sess_t));

	sess->state = SESS_ACTIVE;

	sess->trx = trx_create(sess);

	UT_LIST_INIT(sess->graphs);

	return(sess);
}

/*************************************************************************
Closes a session, freeing the memory occupied by it. */

static
void
sess_close(
/*=======*/
	sess_t*	sess)	/* in, own: session object */
{	
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */
	ut_ad(sess->trx == NULL);

	mem_free(sess);
}

/*************************************************************************
Closes a session, freeing the memory occupied by it, if it is in a state
where it should be closed. */

ibool
sess_try_close(
/*===========*/
			/* out: TRUE if closed */
	sess_t*	sess)	/* in, own: session object */
{
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */
	if (UT_LIST_GET_LEN(sess->graphs) == 0) {
		sess_close(sess);

		return(TRUE);
	}

	return(FALSE);
}
