/*****************************************************************************

Copyright (c) 2010, 2013, Oracle and/or its affiliates. All Rights Reserved.

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
@file include/dict0priv.h
Data dictionary private functions

Created  Fri 2 Jul 2010 13:30:38 EST - Sunny Bains
*******************************************************/

#ifndef dict0priv_h
#define dict0priv_h

/**********************************************************************//**
Gets a table; loads it to the dictionary cache if necessary. A low-level
function. Note: Not to be called from outside dict0*c functions.
@return	table, NULL if not found */
UNIV_INLINE
dict_table_t*
dict_table_get_low(
/*===============*/
	const char*	table_name);		/*!< in: table name */

/**********************************************************************//**
Checks if a table is in the dictionary cache.
@return	table, NULL if not found */
UNIV_INLINE
dict_table_t*
dict_table_check_if_in_cache_low(
/*=============================*/
	const char*	table_name);		/*!< in: table name */

/**********************************************************************//**
Returns a table object based on table id.
@return	table, NULL if does not exist */
UNIV_INLINE
dict_table_t*
dict_table_open_on_id_low(
/*=====================*/
	table_id_t		table_id,	/*!< in: table id */
	dict_err_ignore_t	ignore_err);	/*!< in: errors to ignore
						when loading the table */

#ifndef UNIV_NONINL
#include "dict0priv.ic"
#endif

#endif /* dict0priv.h */
