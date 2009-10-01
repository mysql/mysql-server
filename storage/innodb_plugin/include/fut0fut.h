/*****************************************************************************

Copyright (c) 1995, 2009, Innobase Oy. All Rights Reserved.

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

/******************************************************************//**
@file include/fut0fut.h
File-based utilities

Created 12/13/1995 Heikki Tuuri
***********************************************************************/


#ifndef fut0fut_h
#define fut0fut_h

#include "univ.i"

#include "fil0fil.h"
#include "mtr0mtr.h"

/********************************************************************//**
Gets a pointer to a file address and latches the page.
@return pointer to a byte in a frame; the file page in the frame is
bufferfixed and latched */
UNIV_INLINE
byte*
fut_get_ptr(
/*========*/
	ulint		space,	/*!< in: space id */
	ulint		zip_size,/*!< in: compressed page size in bytes
				or 0 for uncompressed pages */
	fil_addr_t	addr,	/*!< in: file address */
	ulint		rw_latch, /*!< in: RW_S_LATCH, RW_X_LATCH */
	mtr_t*		mtr);	/*!< in: mtr handle */

#ifndef UNIV_NONINL
#include "fut0fut.ic"
#endif

#endif

