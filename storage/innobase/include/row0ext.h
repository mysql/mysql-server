/*****************************************************************************

Copyright (c) 2006, 2009, Innobase Oy. All Rights Reserved.

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
@file include/row0ext.h
Caching of externally stored column prefixes

Created September 2006 Marko Makela
*******************************************************/

#ifndef row0ext_h
#define row0ext_h

#include "univ.i"
#include "row0types.h"
#include "data0types.h"
#include "mem0mem.h"
#include "dict0types.h"

/********************************************************************//**
Creates a cache of column prefixes of externally stored columns.
@return	own: column prefix cache */
UNIV_INTERN
row_ext_t*
row_ext_create(
/*===========*/
	ulint		n_ext,	/*!< in: number of externally stored columns */
	const ulint*	ext,	/*!< in: col_no's of externally stored columns
				in the InnoDB table object, as reported by
				dict_col_get_no(); NOT relative to the records
				in the clustered index */
	ulint		flags, /*!< in: table->flags */
	const dtuple_t*	tuple,	/*!< in: data tuple containing the field
				references of the externally stored
				columns; must be indexed by col_no;
				the clustered index record must be
				covered by a lock or a page latch
				to prevent deletion (rollback or purge). */
	mem_heap_t*	heap);	/*!< in: heap where created */

/********************************************************************//**
Looks up a column prefix of an externally stored column.
@return column prefix, or NULL if the column is not stored externally,
or pointer to field_ref_zero if the BLOB pointer is unset */
UNIV_INLINE
const byte*
row_ext_lookup_ith(
/*===============*/
	const row_ext_t*	ext,	/*!< in/out: column prefix cache */
	ulint			i,	/*!< in: index of ext->ext[] */
	ulint*			len);	/*!< out: length of prefix, in bytes,
					at most the length determined by
					DICT_MAX_FIELD_LEN_BY_FORMAT() */
/********************************************************************//**
Looks up a column prefix of an externally stored column.
@return column prefix, or NULL if the column is not stored externally,
or pointer to field_ref_zero if the BLOB pointer is unset */
UNIV_INLINE
const byte*
row_ext_lookup(
/*===========*/
	const row_ext_t*	ext,	/*!< in: column prefix cache */
	ulint			col,	/*!< in: column number in the InnoDB
					table object, as reported by
					dict_col_get_no(); NOT relative to the
					records in the clustered index */
	ulint*			len);	/*!< out: length of prefix, in bytes,
					at most the length determined by
					DICT_MAX_FIELD_LEN_BY_FORMAT() */

/** Prefixes of externally stored columns */
struct row_ext_struct{
	ulint		n_ext;	/*!< number of externally stored columns */
	const ulint*	ext;	/*!< col_no's of externally stored columns */
	byte*		buf;	/*!< backing store of the column prefix cache */
	ulint		max_len;/*!< maximum prefix length, it could be
				REC_ANTELOPE_MAX_INDEX_COL_LEN or
				REC_VERSION_56_MAX_INDEX_COL_LEN depending
				on row format */
	ulint		len[1];	/*!< prefix lengths; 0 if not cached */
};

#ifndef UNIV_NONINL
#include "row0ext.ic"
#endif

#endif
