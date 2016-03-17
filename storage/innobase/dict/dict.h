/*****************************************************************************

Copyright (c) 1996, 2016, Oracle and/or its affiliates. All Rights Reserved.
Copyright (c) 2012, Facebook Inc.

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
@file dict/dict.h
Data dictionary system

Created 1/8/1996 Heikki Tuuri
*******************************************************/

/** NOTE: The functions in this file should only use functions from
other files in library. The code in this file is used to make a library for
external tools. */

#ifndef dict_dict_h
#define dict_dict_h
/*******************************************************************//**
Adds a column to index. */
void
dict_index_add_col(
/*===============*/
	dict_index_t*		index,		/*!< in/out: index */
	const dict_table_t*	table,		/*!< in: table */
	dict_col_t*		col,		/*!< in: column */
	ulint			prefix_len);	/*!< in: column prefix length */

/**********************************************************************//**
Returns a column's name.
@return column name. NOTE: not guaranteed to stay valid if table is
modified in any way (columns added, etc.). */
const char*
dict_table_get_col_name(
/*====================*/
	const dict_table_t*	table,	/*!< in: table */
	ulint			col_nr)	/*!< in: column number */
	MY_ATTRIBUTE((warn_unused_result));
#endif
