/******************************************************
Caching of externally stored column prefixes

(c) 2006 Innobase Oy

Created September 2006 Marko Makela
*******************************************************/

#ifndef row0ext_h
#define row0ext_h

#include "univ.i"
#include "row0types.h"
#include "mem0mem.h"

/************************************************************************
Creates a cache of column prefixes of externally stored columns. */
UNIV_INLINE
row_ext_t*
row_ext_create(
/*===========*/
				/* out,own: column prefix cache */
	ulint		n_ext,	/* in: number of externally stored columns */
	const ulint*	ext,	/* in: col_no's of externally stored columns
				in the InnoDB table object, as reported by
				dict_col_get_no(); NOT relative to the records
				in the clustered index */
	ulint		zip_size,/* compressed page size, or 0 */
	mem_heap_t*	heap);	/* in: heap where created */

/************************************************************************
Looks up a column prefix of an externally stored column. */
UNIV_INLINE
byte*
row_ext_lookup_ith(
/*===============*/
				/* out: column prefix, or NULL if
				the column is not stored externally,
				or pointer to field_ref_zero
				if the BLOB pointer is unset */
	row_ext_t*	ext,	/* in/out: column prefix cache */
	ulint		i,	/* in: ith element of ext */
	const byte*	field,	/* in: locally stored part of the column */
	ulint		f_len,	/* in: length of field, in bytes */
	ulint*		len);	/* out: length of prefix, in bytes,
				at most REC_MAX_INDEX_COL_LEN */
/************************************************************************
Looks up a column prefix of an externally stored column. */
UNIV_INLINE
byte*
row_ext_lookup(
/*===========*/
				/* out: column prefix, or NULL if
				the column is not stored externally,
				or pointer to field_ref_zero
				if the BLOB pointer is unset */
	row_ext_t*	ext,	/* in/out: column prefix cache */
	ulint		col,	/* in: column number in the InnoDB
				table object, as reported by
				dict_col_get_no(); NOT relative to the
				records in the clustered index */
	const byte*	field,	/* in: locally stored part of the column */
	ulint		f_len,	/* in: length of field, in bytes */
	ulint*		len);	/* out: length of prefix, in bytes,
				at most REC_MAX_INDEX_COL_LEN */

/* Prefixes of externally stored columns */
struct row_ext_struct{
	ulint		n_ext;	/* number of externally stored columns */
	const ulint*	ext;	/* col_no's of externally stored columns */
	ulint		zip_size;/* compressed page size, or 0 */
	byte*		buf;	/* backing store of the column prefix cache */
	ulint		len[1];	/* prefix lengths; 0 if not cached */
};

#ifndef UNIV_NONINL
#include "row0ext.ic"
#endif

#endif
