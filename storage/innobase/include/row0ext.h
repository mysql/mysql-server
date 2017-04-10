/*****************************************************************************

Copyright (c) 2006, 2016, Oracle and/or its affiliates. All Rights Reserved.

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
#include "page0size.h"

#ifdef UNIV_DEBUG
# define row_ext_create(n_ext, ext, flags, tuple, is_sdi, heap)	\
	row_ext_create_func(n_ext, ext, flags, tuple, is_sdi, heap)
#else /* UNIV_DEBUG */
# define row_ext_create(n_ext, ext, flags, tuple, is_sdi, heap)	\
	row_ext_create_func(n_ext, ext, flags, tuple, heap)
#endif /* UNIV_DEBUG */

/** Creates a cache of column prefixes of externally stored columns.
@param[in]	n_ext	number of externally stored columns
@param[in]	ext	col_no's of externally stored columns in the InnoDB
table object, as reported by dict_col_get_no(); NOT relative to the records
in the clustered index
@param[in]	flags	table->flags
@param[in]	tuple	data tuple containing the field references of the
externally stored columns; must be indexed by col_no; the clustered index record
must be covered by a lock or a page latch to prevent deletion (rollback
or purge)
@param[in]	is_sdi	true for SDI Indexes
@param[in]	heap	heap where created
@return own: column prefix cache */
row_ext_t*
row_ext_create_func(
	ulint		n_ext,
	const ulint*	ext,
	ulint		flags,
	const dtuple_t*	tuple,
#ifdef UNIV_DEBUG
	bool		is_sdi,
#endif /* UNIV_DEBUG */
	mem_heap_t*	heap);

/** Looks up a column prefix of an externally stored column.
@param[in,out]	ext	column prefix cache
@param[in]	i	index of ext->ext[]
@param[out]	len	length of prefix, in bytes, at most the length
			determined by DICT_MAX_FIELD_LEN_BY_FORMAT()
@return column prefix, or NULL if the column is not stored externally,
or pointer to field_ref_zero if the BLOB pointer is unset */
UNIV_INLINE
const byte*
row_ext_lookup_ith(
	const row_ext_t*	ext,
	ulint			i,
	ulint*			len);

/** Looks up a column prefix of an externally stored column.
@param[in]	ext	column prefix cache
@param[in]	col	column number in the InnoDB table object, as reported
			by dict_col_get_no(); NOT relative to the records in
			the clustered index
@param[out]	len	length of prefix, in bytes, at most the length
			determined by DICT_MAX_FIELD_LEN_BY_FORMAT()
@return column prefix, or NULL if the column is not stored externally,
or pointer to field_ref_zero if the BLOB pointer is unset */
UNIV_INLINE
const byte*
row_ext_lookup(
	const row_ext_t*	ext,
	ulint			col,
	ulint*			len);

/** Prefixes of externally stored columns */
struct row_ext_t{
	ulint		n_ext;	/*!< number of externally stored columns */
	const ulint*	ext;	/*!< col_no's of externally stored columns */
	byte*		buf;	/*!< backing store of the column prefix cache */
	ulint		max_len;/*!< maximum prefix length, it could be
				REC_ANTELOPE_MAX_INDEX_COL_LEN or
				REC_VERSION_56_MAX_INDEX_COL_LEN depending
				on row format */
	page_size_t	page_size;
				/*!< page size of the externally stored
				columns */
	ulint		len[1];	/*!< prefix lengths; 0 if not cached */
};

#include "row0ext.ic"

#endif
