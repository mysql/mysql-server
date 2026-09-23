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
                                Vector& embedding);

/* When a DML operation happens on a table with a vector column on which we
have built a vector index, we need to capture it and make appropriate changes
in the index. We intercept InnoDB DML operations and map them to what needs
to change in the vector index here. */

/* Row level operation performed by InnoDB. Note that this does not directly
map to higher level DML operation requested by SQL. For example, an UPDATE
to the primary key will result in a delete marking plus insert. */
enum ib_row_op_t {
  /* No operation requested. */
  NONE,

  /* A fresh insert. It can be that the original DML is an update which
  resulted in delete marking + insert. */
  INSERT,

  /* Delete operation i.e.: delete marking a record in InnoDB jargon. */
  DELETE,

  /* An update. The vector column may or may not have changed. */
  UPDATE,

  /* We never insert a row in the table as part of UNDO. But if we are UNDOing
  a delete mark operation it is logically equivalent to INSERT. This corresponds
  to TRX_UNDO_DEL_MARK_REC. We'd have deleted the entry from the vector index.
  Hence we need to reinsert it. */
  INSERT_IN_UNDO,

  /* We might be UNDOing a regular fresh insert or it can be an insert by modify
  i.e.: where we saw a delete mark record and reused it by modifying it. This
  second case corresponds to TRX_UNDO_UPD_DEL_REC. */
  DELETE_IN_UNDO,

  /* UNDOing an UPDATE. This corresponds to TRX_UNDO_UPD_EXIST_REC */
  UPDATE_IN_UNDO,
};

/* Type of operation to be performed on vector index. */
enum vector_row_op_t {
  VECTOR_NONE,
  VECTOR_INSERT,
  VECTOR_DELETE,
  VECTOR_UPDATE,
};

} /* namespace ib_vector */
#endif /* _INCLUDE_VECTOR0BUILD_H_ */
