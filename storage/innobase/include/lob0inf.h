/*****************************************************************************

Copyright (c) 2016, 2022, Oracle and/or its affiliates.

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
#ifndef lob0inf_h
#define lob0inf_h

#include <list>

#include "fut0lst.h"
#include "lob0index.h"
#include "lob0lob.h"
#include "mtr0mtr.h"
#include "table.h"
#include "trx0trx.h"

struct lob_diff_t;

namespace lob {

/** Insert a large object (LOB) into the system.
@param[in]      ctx     the B-tree context for this LOB operation.
@param[in]      trx     transaction doing the insertion.
@param[in,out]  ref     the LOB reference.
@param[in]      field   the LOB field.
@param[in]      field_j the LOB field index in big rec vector.
@return DB_SUCCESS on success, error code on failure.*/
dberr_t insert(InsertContext *ctx, trx_t *trx, ref_t &ref,
               big_rec_field_t *field, ulint field_j);

/** Insert a compressed large object (LOB) into the system.
@param[in]      ctx     the B-tree context for this LOB operation.
@param[in]      trx     transaction doing the insertion.
@param[in,out]  ref     the LOB reference.
@param[in]      field   the LOB field.
@param[in]      field_j the LOB field index in big rec vector.
@return DB_SUCCESS on success, error code on failure.*/
dberr_t z_insert(InsertContext *ctx, trx_t *trx, ref_t &ref,
                 big_rec_field_t *field, ulint field_j);

/** Fetch a large object (LOB) from the system.
@param[in]  ctx    the read context information.
@param[in]  ref    the LOB reference identifying the LOB.
@param[in]  offset read the LOB from the given offset.
@param[in]  len    the length of LOB data that needs to be fetched.
@param[out] buf    the output buffer (owned by caller) of minimum len bytes.
@return the amount of data (in bytes) that was actually read. */
ulint read(ReadContext *ctx, ref_t ref, ulint offset, ulint len, byte *buf);

/** Fetch a compressed large object (ZLOB) from the system.
@param[in] ctx    the read context information.
@param[in] ref    the LOB reference identifying the LOB.
@param[in] offset read the LOB from the given offset.
@param[in] len    the length of LOB data that needs to be fetched.
@param[out] buf   the output buffer (owned by caller) of minimum len bytes.
@return the amount of data (in bytes) that was actually read. */
ulint z_read(lob::ReadContext *ctx, lob::ref_t ref, ulint offset, ulint len,
             byte *buf);

/** Print information about the given LOB.
@param[in]  trx  the current transaction.
@param[in]  index  the clust index that contains the LOB.
@param[in]  out    the output stream into which LOB info is printed.
@param[in]  ref    the LOB reference
@param[in]  fatal  if true assert at end of function. */
void print(trx_t *trx, dict_index_t *index, std::ostream &out, ref_t ref,
           bool fatal);

/** Print information about the given compressed lob. */
dberr_t z_print_info(const dict_index_t *index, const lob::ref_t &ref,
                     std::ostream &out);

/** Update a portion of the given LOB.
@param[in] trx       the transaction that is doing the modification.
@param[in] index     the clustered index containing the LOB.
@param[in] upd       update vector
@param[in] field_no  the LOB field number
@return DB_SUCCESS on success, error code on failure. */
dberr_t update(trx_t *trx, dict_index_t *index, const upd_t *upd,
               ulint field_no);

/** Update a portion of the given LOB.
@param[in] trx       the transaction that is doing the modification.
@param[in] index     the clustered index containing the LOB.
@param[in] upd       update vector
@param[in] field_no  the LOB field number
@return DB_SUCCESS on success, error code on failure. */
dberr_t z_update(trx_t *trx, dict_index_t *index, const upd_t *upd,
                 ulint field_no);

/** Get the list of index entries affected by the given partial update
vector.
@param[in]      ref     LOB reference object.
@param[in]      index   Clustered index to which LOB belongs.
@param[in]      bdiff   Single partial update vector
@param[out]     entries Affected LOB index entries.
@param[in]      mtr         Mini-transaction
@return DB_SUCCESS on success, error code on failure. */
dberr_t get_affected_index_entries(const ref_t &ref, dict_index_t *index,
                                   const Binary_diff &bdiff,
                                   List_iem_t &entries, mtr_t *mtr);

/** Apply the undo log on the LOB
@param[in]  mtr   Mini-transaction context.
@param[in]  index Clustered index to which LOB belongs.
@param[in]  ref   LOB reference object.
@param[in]  uf    Update vector for LOB field.
@return DB_SUCCESS on success, error code on failure. */
dberr_t apply_undolog(mtr_t *mtr, dict_index_t *index, ref_t ref,
                      const upd_field_t *uf);

/** Get information about the given LOB.
@param[in]      ref               LOB reference.
@param[in]      index           Clustered index to which LOB belongs.
@param[out]     lob_version       LOB version number.
@param[out]     last_trx_id   trx_id that modified the lob last.
@param[out]     last_undo_no  Trx undo no that modified the lob last.
@param[out]     page_type       the Page type of first lob page.
@param[in]      mtr                     Mini-transaction context.
@return always returns DB_SUCCESS. */
dberr_t get_info(ref_t &ref, dict_index_t *index, ulint &lob_version,
                 trx_id_t &last_trx_id, undo_no_t &last_undo_no,
                 page_type_t &page_type, mtr_t *mtr);

/** Validate the size of the given LOB.
@param[in]  lob_size  Expected size of the LOB, mostly obtained from
                      the LOB reference.
@param[in]  index     Clustered index containing the LOB.
@param[in]  node_loc  Location of the first LOB index entry.
@param[in]  mtr       Mini-transaction context.
@return true if size is valid, false otherwise. */
bool validate_size(const ulint lob_size, dict_index_t *index,
                   fil_addr_t node_loc, mtr_t *mtr);

}  // namespace lob

#endif  // lob0inf_h
