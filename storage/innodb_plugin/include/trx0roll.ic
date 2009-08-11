/*****************************************************************************

Copyright (c) 1996, 2009, Innobase Oy. All Rights Reserved.

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

/**************************************************//**
@file include/trx0roll.ic
Transaction rollback

Created 3/26/1996 Heikki Tuuri
*******************************************************/

/*******************************************************************//**
Returns pointer to nth element in an undo number array.
@return	pointer to the nth element */
UNIV_INLINE
trx_undo_inf_t*
trx_undo_arr_get_nth_info(
/*======================*/
	trx_undo_arr_t*	arr,	/*!< in: undo number array */
	ulint		n)	/*!< in: position */
{
	ut_ad(arr);
	ut_ad(n < arr->n_cells);

	return(arr->infos + n);
}
