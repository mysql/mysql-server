/*****************************************************************************

Copyright (c) 1994, 2009, Innobase Oy. All Rights Reserved.

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

/***************************************************************//**
@file ut/ut0rnd.c
Random numbers and hashing

Created 5/11/1994 Heikki Tuuri
********************************************************************/

#include "ut0rnd.h"

#ifdef UNIV_NONINL
#include "ut0rnd.ic"
#endif

/** These random numbers are used in ut_find_prime */
/*@{*/
#define	UT_RANDOM_1	1.0412321
#define	UT_RANDOM_2	1.1131347
#define UT_RANDOM_3	1.0132677
/*@}*/

/** Seed value of ut_rnd_gen_ulint(). */
UNIV_INTERN ulint	ut_rnd_ulint_counter = 65654363;

/***********************************************************//**
Looks for a prime number slightly greater than the given argument.
The prime is chosen so that it is not near any power of 2.
@return	prime */
UNIV_INTERN
ulint
ut_find_prime(
/*==========*/
	ulint	n)	/*!< in: positive number > 100 */
{
	ulint	pow2;
	ulint	i;

	n += 100;

	pow2 = 1;
	while (pow2 * 2 < n) {
		pow2 = 2 * pow2;
	}

	if ((double)n < 1.05 * (double)pow2) {
		n = (ulint) ((double)n * UT_RANDOM_1);
	}

	pow2 = 2 * pow2;

	if ((double)n > 0.95 * (double)pow2) {
		n = (ulint) ((double)n * UT_RANDOM_2);
	}

	if (n > pow2 - 20) {
		n += 30;
	}

	/* Now we have n far enough from powers of 2. To make
	n more random (especially, if it was not near
	a power of 2), we then multiply it by a random number. */

	n = (ulint) ((double)n * UT_RANDOM_3);

	for (;; n++) {
		i = 2;
		while (i * i <= n) {
			if (n % i == 0) {
				goto next_n;
			}
			i++;
		}

		/* Found a prime */
		break;
next_n:		;
	}

	return(n);
}
