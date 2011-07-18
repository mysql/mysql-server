/*****************************************************************************

Copyright (c) 2005, 2010, Innobase Oy. All Rights Reserved.

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
@file include/handler0alter.h
Smart ALTER TABLE
*******************************************************/

/*************************************************************//**
Copies an InnoDB record to table->record[0]. */
UNIV_INTERN
void
innobase_rec_to_mysql(
/*==================*/
	struct TABLE*		table,		/*!< in/out: MySQL table */
	const rec_t*		rec,		/*!< in: record */
	const dict_index_t*	index,		/*!< in: index */
	const ulint*		offsets);	/*!< in: rec_get_offsets(
						rec, index, ...) */

/*************************************************************//**
Resets table->record[0]. */
UNIV_INTERN
void
innobase_rec_reset(
/*===============*/
	struct TABLE*		table);		/*!< in/out: MySQL table */
