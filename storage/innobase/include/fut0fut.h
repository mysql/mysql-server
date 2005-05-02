/**********************************************************************
File-based utilities

(c) 1995 Innobase Oy

Created 12/13/1995 Heikki Tuuri
***********************************************************************/


#ifndef fut0fut_h
#define fut0fut_h

#include "univ.i"

#include "fil0fil.h"
#include "mtr0mtr.h"

/************************************************************************
Gets a pointer to a file address and latches the page. */
UNIV_INLINE
byte*
fut_get_ptr(
/*========*/
				/* out: pointer to a byte in a frame; the file
				page in the frame is bufferfixed and latched */
	ulint		space,	/* in: space id */
	fil_addr_t	addr,	/* in: file address */
	ulint		rw_latch, /* in: RW_S_LATCH, RW_X_LATCH */
	mtr_t*		mtr);	/* in: mtr handle */

#ifndef UNIV_NONINL
#include "fut0fut.ic"
#endif

#endif

