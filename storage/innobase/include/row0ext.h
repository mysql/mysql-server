/*****************************************************************************

Copyright (c) 2006, 2017, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

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
# define row_ext_create(index, n_ext, ext, flags, tuple, is_sdi, heap)	\
	row_ext_create_func(index, n_ext, ext, flags, tuple, is_sdi, heap)
#else /* UNIV_DEBUG */
# define row_ext_create(index, n_ext, ext, flags, tuple, is_sdi, heap)	\
	row_ext_create_func(index, n_ext, ext, flags, tuple, heap)
#endif /* UNIV_DEBUG */

/** Creates a cache of column prefixes of externally stored columns.
@param[in]	index	the index to which LOB belongs.
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
	const dict_index_t*	index,
	ulint			n_ext,
	const ulint*		ext,
	ulint			flags,
	const dtuple_t*		tuple,
#ifdef UNIV_DEBUG
	bool			is_sdi,
#endif /* UNIV_DEBUG */
	mem_heap_t*		heap);

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

	/** The clustered index from where LOB is fetched. */
	const dict_index_t*	index;

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

	/* NOTE: Do NOT add new members here. */

	std::ostream& print(std::ostream& out) const {
		out << "[row_ext_t:";
		for (ulint i = 0; i < n_ext; i++) {
			if (len[i] > 0) {
				byte* ptr = (buf + i * max_len);
				ut_print_buf(out, ptr, len[i]);
			}
		}
		out << "]" << std::endl;
		return(out);
	}
};

inline
std::ostream&
operator<< (std::ostream& out, const row_ext_t& obj)
{
	return(obj.print(out));
}

#include "row0ext.ic"

#endif
