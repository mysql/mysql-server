/*****************************************************************************

Copyright (c) 2011, 2023, Oracle and/or its affiliates.

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

#include "my_psi_config.h"

/** @file include/row0log.h
 Modification log for online index creation and online table rebuild

 Created 2011-05-26 Marko Makela
 *******************************************************/

#ifndef row0log_h
#define row0log_h

#include "univ.i"

#include "data0types.h"
#include "dict0types.h"
#include "mtr0types.h"
#include "que0types.h"
#include "rem0types.h"
#include "row0types.h"
#include "trx0types.h"

class Alter_stage;

/** Allocate the row log for an index and flag the index
for online creation.
@param[in] index    Index.
@param[in] table    New table being rebuilt, or NULL when creating a secondary
index.
@param[in] same_pk  Whether the definition of the PRIMARY KEY has remained the
same.
@param[in] add_cols Default values of added columns, or nullptr.
@param[in] col_map  Mapping of old column numbers to new ones, or nullptr if
!table.
@param[in] path     Where to create temporary file.
@retval true if success, false if not */
bool row_log_allocate(dict_index_t *index, dict_table_t *table, bool same_pk,
                      const dtuple_t *add_cols, const ulint *col_map,
                      const char *path);

/** Free the row log for an index that was being created online. */
void row_log_free(row_log_t *&log); /*!< in,own: row log */

#ifndef UNIV_HOTBACKUP
/** Free the row log for an index on which online creation was aborted. */
static inline void row_log_abort_sec(
    dict_index_t *index); /*!< in/out: index (x-latched) */

/** Try to log an operation to a secondary index that is (or was) being created.
@param[in,out] index Index, S- or X-latched.
@param[in] tuple Index tuple.
@param[in] trx_id Transaction ID for insert, or 0 for delete.
@retval true if the operation was logged or can be ignored
@retval false if online index creation is not taking place */
[[nodiscard]] static inline bool row_log_online_op_try(dict_index_t *index,
                                                       const dtuple_t *tuple,
                                                       trx_id_t trx_id);
#endif /* !UNIV_HOTBACKUP */
/** Logs an operation to a secondary index that is (or was) being created. */
void row_log_online_op(
    dict_index_t *index,   /*!< in/out: index, S or X latched */
    const dtuple_t *tuple, /*!< in: index tuple */
    trx_id_t trx_id)       /*!< in: transaction ID for insert,
                           or 0 for delete */
    UNIV_COLD;

/** Gets the error status of the online index rebuild log.
 @return DB_SUCCESS or error code */
[[nodiscard]] dberr_t row_log_table_get_error(
    const dict_index_t *index); /*!< in: clustered index of a table
                               that is being rebuilt online */

/** Check whether a virtual column is indexed in the new table being
created during alter table
@param[in]      index   cluster index
@param[in]      v_no    virtual column number
@return true if it is indexed, else false */
bool row_log_col_is_indexed(const dict_index_t *index, ulint v_no);

/** Logs a delete operation to a table that is being rebuilt.
 This will be merged in row_log_table_apply_delete(). */
void row_log_table_delete(
    const rec_t *rec,       /*!< in: clustered index leaf page record,
                            page X-latched */
    const dtuple_t *ventry, /*!< in: dtuple holding virtual column info */
    dict_index_t *index,    /*!< in/out: clustered index, S-latched
                            or X-latched */
    const ulint *offsets,   /*!< in: rec_get_offsets(rec,index) */
    const byte *sys)        /*!< in: DB_TRX_ID,DB_ROLL_PTR that should
                            be logged, or NULL to use those in rec */
    UNIV_COLD;

/** Logs an update operation to a table that is being rebuilt.
 This will be merged in row_log_table_apply_update(). */
void row_log_table_update(
    const rec_t *rec,          /*!< in: clustered index leaf page record,
                               page X-latched */
    dict_index_t *index,       /*!< in/out: clustered index, S-latched
                               or X-latched */
    const ulint *offsets,      /*!< in: rec_get_offsets(rec,index) */
    const dtuple_t *old_pk,    /*!< in: row_log_table_get_pk()
                               before the update */
    const dtuple_t *new_v_row, /*!< in: dtuple contains the new virtual
                             columns */
    const dtuple_t *old_v_row) /*!< in: dtuple contains the old virtual
                             columns */
    UNIV_COLD;

/** Constructs the old PRIMARY KEY and DB_TRX_ID,DB_ROLL_PTR
 of a table that is being rebuilt.
 @return tuple of PRIMARY KEY,DB_TRX_ID,DB_ROLL_PTR in the rebuilt table,
 or NULL if the PRIMARY KEY definition does not change */
[[nodiscard]] const dtuple_t *row_log_table_get_pk(
    const rec_t *rec,     /*!< in: clustered index leaf page record,
                          page X-latched */
    dict_index_t *index,  /*!< in/out: clustered index, S-latched
                          or X-latched */
    const ulint *offsets, /*!< in: rec_get_offsets(rec,index),
                          or NULL */
    byte *sys,            /*!< out: DB_TRX_ID,DB_ROLL_PTR for
                          row_log_table_delete(), or NULL */
    mem_heap_t **heap)    /*!< in/out: memory heap where allocated */
    UNIV_COLD;

/** Logs an insert to a table that is being rebuilt.
 This will be merged in row_log_table_apply_insert(). */
void row_log_table_insert(
    const rec_t *rec,       /*!< in: clustered index leaf page record,
                            page X-latched */
    const dtuple_t *ventry, /*!< in: dtuple holding virtual column info */
    dict_index_t *index,    /*!< in/out: clustered index, S-latched
                            or X-latched */
    const ulint *offsets)   /*!< in: rec_get_offsets(rec,index) */
    UNIV_COLD;
/** Notes that a BLOB is being freed during online ALTER TABLE. */
void row_log_table_blob_free(
    dict_index_t *index, /*!< in/out: clustered index, X-latched */
    page_no_t page_no)   /*!< in: starting page number of the BLOB */
    UNIV_COLD;
/** Notes that a BLOB is being allocated during online ALTER TABLE. */
void row_log_table_blob_alloc(
    dict_index_t *index, /*!< in/out: clustered index, X-latched */
    page_no_t page_no)   /*!< in: starting page number of the BLOB */
    UNIV_COLD;

/** Apply the row_log_table log to a table upon completing rebuild.
@param[in]      thr             query graph
@param[in]      old_table       old table
@param[in,out]  table           MySQL table (for reporting duplicates)
@param[in,out]  stage           performance schema accounting object, used by
ALTER TABLE. stage->begin_phase_log_table() will be called initially and then
stage->inc() will be called for each block of log that is applied.
@return DB_SUCCESS, or error code on failure */
[[nodiscard]] dberr_t row_log_table_apply(que_thr_t *thr,
                                          dict_table_t *old_table,
                                          struct TABLE *table,
                                          Alter_stage *stage);

/** Get the latest transaction ID that has invoked row_log_online_op()
 during online creation.
 @return latest transaction ID, or 0 if nothing was logged */
[[nodiscard]] trx_id_t row_log_get_max_trx(
    dict_index_t *index); /*!< in: index, must be locked */

/** Apply the row log to the index upon completing index creation.
@param[in]      trx     transaction (for checking if the operation was
interrupted)
@param[in,out]  index   secondary index
@param[in,out]  table   MySQL table (for reporting duplicates)
@param[in,out]  stage   performance schema accounting object, used by
ALTER TABLE. stage->begin_phase_log_index() will be called initially and then
stage->inc() will be called for each block of log that is applied.
@return DB_SUCCESS, or error code on failure */
[[nodiscard]] dberr_t row_log_apply(const trx_t *trx, dict_index_t *index,
                                    struct TABLE *table, Alter_stage *stage);

#ifdef HAVE_PSI_STAGE_INTERFACE
/** Estimate how much work is to be done by the log apply phase
of an ALTER TABLE for this index.
@param[in]      index   index whose log to assess
@return work to be done by log-apply in abstract units
*/
ulint row_log_estimate_work(const dict_index_t *index);
#endif /* HAVE_PSI_STAGE_INTERFACE */

#include "row0log.ic"

#endif /* row0log.h */
