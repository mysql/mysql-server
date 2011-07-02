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
@file row/row0ext.c
Caching of externally stored column prefixes

Created September 2006 Marko Makela
*******************************************************/

#include "row0ext.h"

#ifdef UNIV_NONINL
#include "row0ext.ic"
#endif

#include "btr0cur.h"

/********************************************************************//**
Fills the column prefix cache of an externally stored column. */
static
void
row_ext_cache_fill(
/*===============*/
	row_ext_t*	ext,	/*!< in/out: column prefix cache */
	ulint		i,	/*!< in: index of ext->ext[] */
	ulint		zip_size,/*!< compressed page size in bytes, or 0 */
	const dfield_t*	dfield)	/*!< in: data field */
{
	const byte*	field	= dfield_get_data(dfield);
	ulint		f_len	= dfield_get_len(dfield);
	byte*		buf	= ext->buf + i * ext->max_len;

	ut_ad(ext->max_len > 0);
	ut_ad(i < ext->n_ext);
	ut_ad(dfield_is_ext(dfield));
	ut_a(f_len >= BTR_EXTERN_FIELD_REF_SIZE);

	if (UNIV_UNLIKELY(!memcmp(field_ref_zero,
				  field + f_len - BTR_EXTERN_FIELD_REF_SIZE,
				  BTR_EXTERN_FIELD_REF_SIZE))) {
		/* The BLOB pointer is not set: we cannot fetch it */
		ext->len[i] = 0;
	} else {
		/* Fetch at most ext->max_len of the column.
		The column should be non-empty.  However,
		trx_rollback_or_clean_all_recovered() may try to
		access a half-deleted BLOB if the server previously
		crashed during the execution of
		btr_free_externally_stored_field(). */
		ext->len[i] = btr_copy_externally_stored_field_prefix(
			buf, ext->max_len, zip_size, field, f_len);
	}
}

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
	ulint		flags,	/*!< in: table->flags */
	const dtuple_t*	tuple,	/*!< in: data tuple containing the field
				references of the externally stored
				columns; must be indexed by col_no;
				the clustered index record must be
				covered by a lock or a page latch
				to prevent deletion (rollback or purge). */
	mem_heap_t*	heap)	/*!< in: heap where created */
{
	ulint		i;
	ulint		zip_size = dict_table_flags_to_zip_size(flags);

	row_ext_t*	ret = mem_heap_alloc(heap, (sizeof *ret)
					     + (n_ext - 1) * sizeof ret->len);

	ut_ad(ut_is_2pow(zip_size));
	ut_ad(zip_size <= UNIV_PAGE_SIZE);

	ret->n_ext = n_ext;
	ret->ext = ext;
	ret->max_len = DICT_MAX_FIELD_LEN_BY_FORMAT_FLAG(flags);

	ret->buf = mem_heap_alloc(heap, n_ext * ret->max_len);
#ifdef UNIV_DEBUG
	memset(ret->buf, 0xaa, n_ext * ret->max_len);
	UNIV_MEM_ALLOC(ret->buf, n_ext * ret->max_len);
#endif

	/* Fetch the BLOB prefixes */
	for (i = 0; i < n_ext; i++) {
		const dfield_t*	dfield;

		dfield = dtuple_get_nth_field(tuple, ext[i]);
		row_ext_cache_fill(ret, i, zip_size, dfield);
	}

	return(ret);
}
