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
@file mach/mach0data.c
Utilities for converting data from the database file
to the machine format.

Created 11/28/1995 Heikki Tuuri
***********************************************************************/

#include "mach0data.h"

#ifdef UNIV_NONINL
#include "mach0data.ic"
#endif

/*********************************************************//**
Reads a ulint in a compressed form if the log record fully contains it.
@return	pointer to end of the stored field, NULL if not complete */
UNIV_INTERN
byte*
mach_parse_compressed(
/*==================*/
	byte*	ptr,	/*!< in: pointer to buffer from where to read */
	byte*	end_ptr,/*!< in: pointer to end of the buffer */
	ulint*	val)	/*!< out: read value (< 2^32) */
{
	ulint	flag;

	ut_ad(ptr && end_ptr && val);

	if (ptr >= end_ptr) {

		return(NULL);
	}

	flag = mach_read_from_1(ptr);

	if (flag < 0x80UL) {
		*val = flag;
		return(ptr + 1);

	} else if (flag < 0xC0UL) {
		if (end_ptr < ptr + 2) {
			return(NULL);
		}

		*val = mach_read_from_2(ptr) & 0x7FFFUL;

		return(ptr + 2);

	} else if (flag < 0xE0UL) {
		if (end_ptr < ptr + 3) {
			return(NULL);
		}

		*val = mach_read_from_3(ptr) & 0x3FFFFFUL;

		return(ptr + 3);
	} else if (flag < 0xF0UL) {
		if (end_ptr < ptr + 4) {
			return(NULL);
		}

		*val = mach_read_from_4(ptr) & 0x1FFFFFFFUL;

		return(ptr + 4);
	} else {
		ut_ad(flag == 0xF0UL);

		if (end_ptr < ptr + 5) {
			return(NULL);
		}

		*val = mach_read_from_4(ptr + 1);
		return(ptr + 5);
	}
}
