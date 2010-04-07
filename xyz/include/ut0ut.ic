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

/**************************************************************//**
@file include/ut0ut.ic
Various utilities

Created 5/30/1994 Heikki Tuuri
*******************************************************************/

/******************************************************//**
Calculates the minimum of two ulints.
@return	minimum */
UNIV_INLINE
ulint
ut_min(
/*===*/
	ulint	 n1,	/*!< in: first number */
	ulint	 n2)	/*!< in: second number */
{
	return((n1 <= n2) ? n1 : n2);
}

/******************************************************//**
Calculates the maximum of two ulints.
@return	maximum */
UNIV_INLINE
ulint
ut_max(
/*===*/
	ulint	 n1,	/*!< in: first number */
	ulint	 n2)	/*!< in: second number */
{
	return((n1 <= n2) ? n2 : n1);
}

/****************************************************************//**
Calculates minimum of two ulint-pairs. */
UNIV_INLINE
void
ut_pair_min(
/*========*/
	ulint*	a,	/*!< out: more significant part of minimum */
	ulint*	b,	/*!< out: less significant part of minimum */
	ulint	a1,	/*!< in: more significant part of first pair */
	ulint	b1,	/*!< in: less significant part of first pair */
	ulint	a2,	/*!< in: more significant part of second pair */
	ulint	b2)	/*!< in: less significant part of second pair */
{
	if (a1 == a2) {
		*a = a1;
		*b = ut_min(b1, b2);
	} else if (a1 < a2) {
		*a = a1;
		*b = b1;
	} else {
		*a = a2;
		*b = b2;
	}
}

/******************************************************//**
Compares two ulints.
@return	1 if a > b, 0 if a == b, -1 if a < b */
UNIV_INLINE
int
ut_ulint_cmp(
/*=========*/
	ulint	a,	/*!< in: ulint */
	ulint	b)	/*!< in: ulint */
{
	if (a < b) {
		return(-1);
	} else if (a == b) {
		return(0);
	} else {
		return(1);
	}
}

/*******************************************************//**
Compares two pairs of ulints.
@return	-1 if a < b, 0 if a == b, 1 if a > b */
UNIV_INLINE
int
ut_pair_cmp(
/*========*/
	ulint	a1,	/*!< in: more significant part of first pair */
	ulint	a2,	/*!< in: less significant part of first pair */
	ulint	b1,	/*!< in: more significant part of second pair */
	ulint	b2)	/*!< in: less significant part of second pair */
{
	if (a1 > b1) {
		return(1);
	} else if (a1 < b1) {
		return(-1);
	} else if (a2 > b2) {
		return(1);
	} else if (a2 < b2) {
		return(-1);
	} else {
		return(0);
	}
}

/*************************************************************//**
Calculates fast the 2-logarithm of a number, rounded upward to an
integer.
@return	logarithm in the base 2, rounded upward */
UNIV_INLINE
ulint
ut_2_log(
/*=====*/
	ulint	n)	/*!< in: number != 0 */
{
	ulint	res;

	res = 0;

	ut_ad(n > 0);

	n = n - 1;

	for (;;) {
		n = n / 2;

		if (n == 0) {
			break;
		}

		res++;
	}

	return(res + 1);
}

/*************************************************************//**
Calculates 2 to power n.
@return	2 to power n */
UNIV_INLINE
ulint
ut_2_exp(
/*=====*/
	ulint	n)	/*!< in: number */
{
	return((ulint) 1 << n);
}
