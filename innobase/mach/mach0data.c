/**********************************************************************
Utilities for converting data from the database file
to the machine format. 

(c) 1995 Innobase Oy

Created 11/28/1995 Heikki Tuuri
***********************************************************************/

#include "mach0data.h"

#ifdef UNIV_NONINL
#include "mach0data.ic"
#endif

/*************************************************************
Reads a ulint in a compressed form if the log record fully contains it. */

byte*
mach_parse_compressed(
/*==================*/
			/* out: pointer to end of the stored field, NULL if
			not complete */
	byte*   ptr,   	/* in: pointer to buffer from where to read */
	byte*	end_ptr,/* in: pointer to end of the buffer */
	ulint*	val)	/* out: read value (< 2^32) */ 
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

/*************************************************************
Reads a dulint in a compressed form if the log record fully contains it. */

byte*
mach_dulint_parse_compressed(
/*=========================*/
			/* out: pointer to end of the stored field, NULL if
			not complete */
	byte*   ptr,   	/* in: pointer to buffer from where to read */
	byte*	end_ptr,/* in: pointer to end of the buffer */
	dulint*	val)	/* out: read value */ 
{
	ulint	high;
	ulint	low;
	ulint	size;

	ut_ad(ptr && end_ptr && val);

	if (end_ptr < ptr + 5) {

		return(NULL);
	}

	high = mach_read_compressed(ptr);

	size = mach_get_compressed_size(high);

	ptr += size;

	if (end_ptr < ptr + 4) {

		return(NULL);
	}

	low = mach_read_from_4(ptr);

	*val = ut_dulint_create(high, low);

	return(ptr + 4); 
}
