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
Looks up and caches a column prefix of an externally stored column. */

byte*
row_ext_lookup_low(
/*===============*/
				/* out: column prefix, or
				pointer to field_ref_zero
				if the BLOB pointer is unset */
	row_ext_t*	ext,	/* in/out: column prefix cache */
	ulint		i,	/* in: index of ext->ext[] */
	const byte*	field,	/* in: locally stored part of the column */
	ulint		f_len,	/* in: length of field, in bytes */
	ulint*		len)	/* out: length of prefix, in bytes,
				at most REC_MAX_INDEX_COL_LEN */
{
	byte*	buf	= ext->buf + i * REC_MAX_INDEX_COL_LEN;

	ut_ad(i < ext->n_ext);
	ut_a(f_len >= BTR_EXTERN_FIELD_REF_SIZE);

	if (UNIV_UNLIKELY(!memcmp(field_ref_zero,
				  field + f_len - BTR_EXTERN_FIELD_REF_SIZE,
				  BTR_EXTERN_FIELD_REF_SIZE))) {
		/* The BLOB pointer is not set: we cannot fetch it */
		*len = 0;
		return((byte*) field_ref_zero);
	}

	*len = ext->len[i] = btr_copy_externally_stored_field_prefix(
		buf,
		REC_MAX_INDEX_COL_LEN, ext->zip_size, field, f_len);
	return(buf);
}
