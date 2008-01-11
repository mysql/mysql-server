/******************************************************
Caching of externally stored column prefixes

(c) 2006 Innobase Oy

Created September 2006 Marko Makela
*******************************************************/

#include "row0ext.h"

#ifdef UNIV_NONINL
#include "row0ext.ic"
#endif

#include "btr0cur.h"

/************************************************************************
Fills the column prefix cache of an externally stored column. */
static
void
row_ext_cache_fill(
/*===============*/
	row_ext_t*	ext,	/* in/out: column prefix cache */
	ulint		i,	/* in: index of ext->ext[] */
	ulint		zip_size,/* compressed page size in bytes, or 0 */
	const dfield_t*	dfield)	/* in: data field */
{
	const byte*	field	= dfield_get_data(dfield);
	ulint		f_len	= dfield_get_len(dfield);
	byte*		buf	= ext->buf + i * REC_MAX_INDEX_COL_LEN;

	ut_ad(i < ext->n_ext);
	ut_ad(dfield_is_ext(dfield));
	ut_a(f_len >= BTR_EXTERN_FIELD_REF_SIZE);

	if (UNIV_UNLIKELY(!memcmp(field_ref_zero,
				  field + f_len - BTR_EXTERN_FIELD_REF_SIZE,
				  BTR_EXTERN_FIELD_REF_SIZE))) {
		/* The BLOB pointer is not set: we cannot fetch it */
		ext->len[i] = 0;
	} else {
		/* Fetch at most REC_MAX_INDEX_COL_LEN of the column.
		The column must be non-empty. */
		ext->len[i] = btr_copy_externally_stored_field_prefix(
			buf, REC_MAX_INDEX_COL_LEN, zip_size, field, f_len);
		ut_a(ext->len[i]);
	}
}

/************************************************************************
Creates a cache of column prefixes of externally stored columns. */

row_ext_t*
row_ext_create(
/*===========*/
				/* out,own: column prefix cache */
	ulint		n_ext,	/* in: number of externally stored columns */
	const ulint*	ext,	/* in: col_no's of externally stored columns
				in the InnoDB table object, as reported by
				dict_col_get_no(); NOT relative to the records
				in the clustered index */
	const dtuple_t*	tuple,	/* in: data tuple containing the field
				references of the externally stored
				columns; must be indexed by col_no;
				the clustered index record must be
				covered by a lock or a page latch
				to prevent deletion (rollback or purge). */
	ulint		zip_size,/* compressed page size in bytes, or 0 */
	mem_heap_t*	heap)	/* in: heap where created */
{
	ulint		i;
	row_ext_t*	ret = mem_heap_alloc(heap, (sizeof *ret)
					     + (n_ext - 1) * sizeof ret->len);

	ut_ad(ut_is_2pow(zip_size));
	ut_ad(zip_size <= UNIV_PAGE_SIZE);

	ret->n_ext = n_ext;
	ret->ext = ext;
	ret->buf = mem_heap_alloc(heap, n_ext * REC_MAX_INDEX_COL_LEN);
#ifdef UNIV_DEBUG
	memset(ret->buf, 0xaa, n_ext * REC_MAX_INDEX_COL_LEN);
	UNIV_MEM_ALLOC(ret->buf, n_ext * REC_MAX_INDEX_COL_LEN);
#endif

	/* Fetch the BLOB prefixes */
	for (i = 0; i < n_ext; i++) {
		const dfield_t*	dfield;

		dfield = dtuple_get_nth_field(tuple, ext[i]);
		row_ext_cache_fill(ret, i, zip_size, dfield);
	}

	return(ret);
}
