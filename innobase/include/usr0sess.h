/******************************************************
Sessions

(c) 1996 Innobase Oy

Created 6/25/1996 Heikki Tuuri
*******************************************************/

#ifndef usr0sess_h
#define usr0sess_h

#include "univ.i"
#include "ut0byte.h"
#include "trx0types.h"
#include "srv0srv.h"
#include "trx0types.h"
#include "usr0types.h"
#include "que0types.h"
#include "data0data.h"
#include "rem0rec.h"

/*************************************************************************
Opens a session. */

sess_t*
sess_open(void);
/*============*/
					/* out, own: session object */
/*************************************************************************
Closes a session, freeing the memory occupied by it, if it is in a state
where it should be closed. */

ibool
sess_try_close(
/*===========*/
				/* out: TRUE if closed */
	sess_t*		sess);	/* in, own: session object */

/* The session handle. All fields are protected by the kernel mutex */
struct sess_struct{
	ulint		state;		/* state of the session */
	trx_t*		trx;		/* transaction object permanently
					assigned for the session: the
					transaction instance designated by the
					trx id changes, but the memory
					structure is preserved */
	UT_LIST_BASE_NODE_T(que_t)
			graphs;		/* query graphs belonging to this
					session */
};

/* Session states */
#define SESS_ACTIVE		1
#define SESS_ERROR		2	/* session contains an error message
					which has not yet been communicated
					to the client */
#ifndef UNIV_NONINL
#include "usr0sess.ic"
#endif

#endif 
