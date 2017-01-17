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
/** Adds a column to index.
@param[in,out]	index		index
@param[in]	table		table
@param[in]	col		column
@param[in]	prefix_len	column prefix length
@param[in]	is_ascending	true=ASC, false=DESC */
void
dict_index_add_col(
	dict_index_t*		index,
	const dict_table_t*	table,
	dict_col_t*		col,
	ulint			prefix_len,
	bool			is_ascending);

#endif
