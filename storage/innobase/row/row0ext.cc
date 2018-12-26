/*****************************************************************************

Copyright (c) 2006, 2018, Oracle and/or its affiliates. All Rights Reserved.

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

/** @file row/row0ext.cc
 Caching of externally stored column prefixes

 Created September 2006 Marko Makela
 *******************************************************/

#include "row0ext.h"
#include "lob0lob.h"

#ifdef UNIV_NONINL
#include "row0ext.ic"
#endif

#include "btr0cur.h"

#ifdef UNIV_DEBUG
#define row_ext_cache_fill(index, ext, i, page_size, is_sdi, dfield) \
  row_ext_cache_fill_func(index, ext, i, page_size, is_sdi, dfield)
#else /* UNIV_DEBUG */
#define row_ext_cache_fill(index, ext, i, page_size, is_sdi, dfield) \
  row_ext_cache_fill_func(index, ext, i, page_size, dfield)
#endif /* UNIV_DEBUG */

/** Fills the column prefix cache of an externally stored column.
@param[in]	index		clustered index from which LOB is fetched.
@param[in,out]	ext		column prefix cache
@param[in]	i		index of ext->ext[]
@param[in]	page_size	page size
@param[in]	dfield		data field
@param[in]	is_sdi		true for SDI Index */
static void row_ext_cache_fill_func(const dict_index_t *index, row_ext_t *ext,
                                    ulint i, const page_size_t &page_size,
#ifdef UNIV_DEBUG
                                    bool is_sdi,
#endif /* UNIV_DEBUG */
                                    const dfield_t *dfield) {
  const byte *field = static_cast<const byte *>(dfield_get_data(dfield));
  ulint f_len = dfield_get_len(dfield);
  byte *buf = ext->buf + i * ext->max_len;
  lob::ref_t blobref(dfield->blobref());

  ut_ad(ext->max_len > 0);
  ut_ad(i < ext->n_ext);
  ut_ad(dfield_is_ext(dfield));
  ut_a(f_len >= BTR_EXTERN_FIELD_REF_SIZE);

  if (blobref.is_null()) {
    /* The BLOB pointer is not set: we cannot fetch it */
    ext->len[i] = 0;
  } else {
    if (ext->max_len == REC_VERSION_56_MAX_INDEX_COL_LEN &&
        f_len > BTR_EXTERN_FIELD_REF_SIZE) {
      /* In this case, the field is in B format or beyond,
      (refer to the definition of row_ext_t.max_len)
      and the field is already fill with prefix, otherwise
      f_len would be BTR_EXTERN_FIELD_REF_SIZE.
      So there is no need to re-read the prefix externally,
      but just copy the local prefix to buf. Please note
      if the ext->len[i] is zero, it means an error
      as above. */
      memcpy(buf, field, f_len - BTR_EXTERN_FIELD_REF_SIZE);
      ext->len[i] = f_len - BTR_EXTERN_FIELD_REF_SIZE;
    } else {
      /* Fetch at most ext->max_len of the column.
      The column should be non-empty.  However,
      trx_rollback_or_clean_all_recovered() may try to
      access a half-deleted BLOB if the server previously
      crashed during the execution of
      btr_free_externally_stored_field(). */
      ext->len[i] = lob::btr_copy_externally_stored_field_prefix(
          nullptr, index, buf, ext->max_len, page_size, field, is_sdi, f_len);
    }
  }
}

/** Creates a cache of column prefixes of externally stored columns.
@param[in]	index	the index to which LOB belongs.
@param[in]	n_ext	number of externally stored columns
@param[in]	ext	col_no's of externally stored columns in the InnoDB
                        table object, as reported by dict_col_get_no(); NOT
                        relative to the records in the clustered index
@param[in]	flags	table->flags
@param[in]	tuple	data tuple containing the field references of the
                        externally stored columns; must be indexed by col_no;
                        the clustered index record must be covered by a lock
                        or a page latch to prevent deletion (rollback
                        or purge)
@param[in]	is_sdi	true for SDI Indexes
@param[in,out]	heap	heap where created
@return own: column prefix cache */
row_ext_t *row_ext_create_func(const dict_index_t *index, ulint n_ext,
                               const ulint *ext, ulint flags,
                               const dtuple_t *tuple,
#ifdef UNIV_DEBUG
                               bool is_sdi,
#endif /* UNIV_DEBUG */
                               mem_heap_t *heap) {
  ulint i;
  const page_size_t &page_size = dict_tf_get_page_size(flags);

  /* LOBs are available in clustered index only. */
  ut_ad(index->is_clustered());

  row_ext_t *ret;

  ut_ad(n_ext > 0);

  ret = static_cast<row_ext_t *>(
      mem_heap_alloc(heap, (sizeof *ret) + (n_ext - 1) * sizeof ret->len));

  ret->index = index;
  ret->n_ext = n_ext;
  ret->ext = ext;
  ret->max_len = DICT_MAX_FIELD_LEN_BY_FORMAT_FLAG(flags);
  ret->page_size.copy_from(page_size);

  ret->buf = static_cast<byte *>(mem_heap_alloc(heap, n_ext * ret->max_len));

#ifdef UNIV_DEBUG
  memset(ret->buf, 0xaa, n_ext * ret->max_len);
  UNIV_MEM_ALLOC(ret->buf, n_ext * ret->max_len);
#endif

  /* Fetch the BLOB prefixes */
  for (i = 0; i < n_ext; i++) {
    const dfield_t *dfield;

    dfield = dtuple_get_nth_field(tuple, ext[i]);
    row_ext_cache_fill(index, ret, i, page_size, is_sdi, dfield);
  }

  return (ret);
}
