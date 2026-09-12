// Copyright 2026 Google LLC

#ifndef _INCLUDE_VECTOR0BUILD_H_
#define _INCLUDE_VECTOR0BUILD_H_
#include "data0data.h"

#include "row0undo.h"
#include "row0upd.h"
#include "vector0dd.h"
#include "vector0index.h"
#include "trx0rec.h"
#include "univ.i"



namespace ib_vector {
/* Checks if embedding is NULL
@param[in]  index       clustered index
@param[in]  rec         record on index page
@param[in]  offsets     offsets into the rec
@param[in]  vector_pos  position of the vector column in the row
@return true if embedding is NULL */
bool embedding_is_null(const dict_index_t *index, const rec_t *rec,
                              ulint *offsets, ulint vector_pos);

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
                              Vector& embedding);

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
                                                mem_heap_t *heap);

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
                                                  mem_heap_t *heap);
} /* namespace ib_vector */
#endif /* _INCLUDE_VECTOR0BUILD_H_ */
