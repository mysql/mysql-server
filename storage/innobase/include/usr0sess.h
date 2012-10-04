/*****************************************************************************

Copyright (c) 1996, 2009, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

*****************************************************************************/

/**************************************************//**
@file include/usr0sess.h
Sessions

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

/*********************************************************************//**
Opens a session.
@return	own: session object */
UNIV_INTERN
sess_t*
sess_open(void);
/*============*/
/*********************************************************************//**
Closes a session, freeing the memory occupied by it. */
UNIV_INTERN
void
sess_close(
/*=======*/
	sess_t*		sess);		/* in, own: session object */

/* The session handle. This data structure is only used by purge and is
not really necessary. We should get rid of it. */
struct sess_t{
	ulint		state;		/*!< state of the session */
	trx_t*		trx;		/*!< transaction object permanently
					assigned for the session: the
					transaction instance designated by the
					trx id changes, but the memory
					structure is preserved */
	UT_LIST_BASE_NODE_T(que_t)
			graphs;		/*!< query graphs belonging to this
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
