// Copyright 2026 Google LLC

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "data0data.h"
#include "data0type.h"
#include "db0err.h"
#include "dict0dict.h"
#include "dict0mem.h"
#include "include/mysqld_error.h"
#include "lob0lob.h"
#include "mem0mem.h"
#include "my_dbug.h"
#include "rem/rec.h"
#include "rem0rec.h"
#include "rem0types.h"
#include "rem0wrec.h"
#include "vector0dd.h"
#include "vector0pk.h"
#include "vector0vector.h"
#include "vector0types.h"
#include "vector0build.h"
#include "trx0trx.h"
#include "univ.i"
#include "ut0core.h"
#include "ut0counter.h"
#include "ut0dbg.h"
#include "ut0log.h"
#include "ut0new.h"

namespace ib_vector {
/* Forward declaration */
class VectorIndex;

/* Fills embedding vector. The field must be non-NULL i.e.: data must be present
@param[in]  index       clustered index
@param[in]  data        pointer to data
@param[in]  len         length of data in bytes
@param[out] emb   vector to be filled */
static void copy_data_to_vector(const dict_index_t *index, const byte *data,
                                ulint len, Vector& emb) {
  ut_a(len != UNIV_SQL_NULL);
  auto dim = get_vector_dim(index);
  ut_a(valid_dimensions(dim));
  ut_a(len == dim * 4);
  emb.resize(dim);
  std::memcpy(emb.data(), data, len);
}

/* Checks if embedding is NULL
@param[in]  index       clustered index
@param[in]  rec         record on index page
@param[in]  offsets     offsets into the rec
@param[in]  vector_pos  position of the vector column in the row
@return true if embedding is NULL */
bool embedding_is_null(const dict_index_t *index, const rec_t *rec,
                              ulint *offsets, ulint vector_pos) {
  ulint len = UNIV_SQL_NULL;
  rec_get_nth_field(index, rec, offsets, vector_pos, &len);
  return len == UNIV_SQL_NULL;
}

/* Copies embedding from a record on the page.
@param[in]  index       clustered index
@param[in]  rec         record on index page
@param[in]  offsets     offsets into the rec
@param[in]  vector_pos  position of the vector column in the row
@param[in]  heap        heap from where memory is allocated
@return pair of data and len. UNIV_SQL_NULL if embedding is NULL. */
std::pair<byte *, ulint> get_embedding_from_rec(const dict_index_t *index,
                                                const rec_t *rec,
                                                ulint *offsets,
                                                ulint vector_pos,
                                                mem_heap_t *heap) {
  ulint len = UNIV_SQL_NULL;
  auto data = rec_get_nth_field(index, rec, offsets, vector_pos, &len);
  if (len == UNIV_SQL_NULL) {
    return {nullptr, UNIV_SQL_NULL};
  }

  if (rec_offs_nth_extern(index, offsets, vector_pos)) {
    /* Stored offline (externally). Read from BLOB pages and copy.
     * We are passing nullptr as trx implying that we want to read
     * READ_UNCOMMITTED. This should work because we assume that caller has
     * minimally locked the row. In case a BLOB is being modified we can
     * possibly get a nultptr back. But it shouldn't happen in our case as
     * either the base table (in case of build) or the base table row (in case
     * of DML) is locked. */
    data = lob::btr_rec_copy_externally_stored_field(
        nullptr, index, rec, offsets, dict_table_page_size(index->table),
        vector_pos, &len, nullptr, dict_index_is_sdi(index), heap);
    if (data == nullptr) {
      ib::error() << "NULL externally stored field encountered";
      return {nullptr, UNIV_SQL_NULL};
    }
  }

  return {const_cast<byte *>(data), len};
}

/* Copies embedding from a record on the page.
@param[in]  index       clustered index
@param[in]  rec         record on index page
@param[in]  offsets     offsets into the rec
@param[in]  vector_pos  position of the vector column in the row
@param[in]  heap        heap from where memory is allocated
@param[out] embedding   vector to be filled
@return length of data read i.e.: 4 * embedding.size() or UNIV_SQL_NULL if
vector data was not present. */
ulint copy_embedding_from_rec(const dict_index_t *index,
                                     const rec_t *rec, ulint *offsets,
                                     ulint vector_pos, mem_heap_t *heap,
                                     Vector& embedding) {
  auto [data, len] =
      get_embedding_from_rec(index, rec, offsets, vector_pos, heap);
  if (data == nullptr) {
    ut_a(len == UNIV_SQL_NULL);
    return UNIV_SQL_NULL;
  }

  copy_data_to_vector(index, data, len, embedding);
  return len;
}

/* Get embedding from a data field. Important to note that in case of
externally stored columns this function does not consult row_ext_t. Instead,
it attempts to read the external content from blob pages. What this means is
that we should never use this function when the external storage is not on
blob pages.
@param[in]  index       clustered index
@param[in]  field       vector column field
@param[in]  heap        heap from where memory is allocated
@return pair of data and len. UNIV_SQL_NULL if embedding is NULL. */
std::pair<byte *, ulint> get_embedding_from_field(const dict_index_t *index,
                                                  const dfield_t *field,
                                                  mem_heap_t *heap) {
  ut_ad(dtype_get_mtype(dfield_get_type(field)) == DATA_BLOB);
  auto data = dfield_get_data(field);
  ulint len = dfield_get_len(field);
  if (len == UNIV_SQL_NULL) {
    return {nullptr, UNIV_SQL_NULL};
  }

  if (field->ext) {
    /* Stored offline (externally). Read from BLOB pages and copy. */
    ulint local_len = len;
    data = lob::btr_copy_externally_stored_field(
        nullptr, index, &len, nullptr, static_cast<const byte *>(data),
        dict_table_page_size(index->table), local_len, dict_index_is_sdi(index),
        heap);
    if (data == nullptr) {
      ib::error() << "NULL externally stored field encountered";
      return {nullptr, UNIV_SQL_NULL};
    }
  }

  return {reinterpret_cast<byte *>(data), len};
}

/* Copies embedding from a data field. Important to note that in case of
externally stored columns this function does not consult row_ext_t. Instead,
it attempts to read the external content from blob pages. What this means is
that we should never use this function when the external storage is not on
blob pages.
@param[in]  index       clustered index
@param[in]  field       vector column field
@param[in]  heap        heap from where memory is allocated
@param[out] embedding   vector to be filled
@return length of data read i.e.: 4 * embedding.size() or UNIV_SQL_NULL if
vector data was not present. */
ulint copy_embedding_from_field(const dict_index_t *index,
                                const dfield_t *field, mem_heap_t *heap,
                                Vector& embedding) {
  auto [data, len] = get_embedding_from_field(index, field, heap);
  if (data == nullptr) {
    ut_a(len == UNIV_SQL_NULL);
    return UNIV_SQL_NULL;
  }

  copy_data_to_vector(index, data, len, embedding);
  return len;
}

} /* namespace ib_vector */
