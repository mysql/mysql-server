/*****************************************************************************

Copyright (c) 2000, 2022, Oracle and/or its affiliates.

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

/** @file include/row0mysql.h
 Interface between Innobase row operations and MySQL.
 Contains also create table and other data dictionary operations.

 Created 9/17/2000 Heikki Tuuri
 *******************************************************/

#ifndef row0mysql_h
#define row0mysql_h

#ifndef UNIV_HOTBACKUP
#include "ha_prototypes.h"
#endif /* !UNIV_HOTBACKUP */

#include <stddef.h>
#include <sys/types.h>
#include <algorithm>

#include "btr0pcur.h"
#include "data0data.h"
#include "data0type.h"
#include "db0err.h"
#include "dict0types.h"
#include "fts0fts.h"
#include "gis0type.h"
#include "lob0undo.h"
#include "lock0types.h"
#include "mem0mem.h"
#include "my_compiler.h"
#include "my_inttypes.h"
#include "que0types.h"
#include "rem0types.h"
#include "row0types.h"
#include "sess0sess.h"
#include "sql_cmd.h"
#include "trx0types.h"
#include "univ.i"
#include "ut0bool_scope_guard.h"

// Forward declarations
class THD;
class ha_innobase;
class innodb_session_t;
namespace dd {
class Table;
}
struct TABLE;
struct btr_pcur_t;
struct dfield_t;
struct dict_field_t;
struct dict_foreign_t;
struct dict_index_t;
struct dict_table_t;
struct dict_v_col_t;
struct dtuple_t;
struct ins_node_t;
struct mtr_t;
struct que_fork_t;
struct que_thr_t;
struct trx_t;
struct upd_node_t;
struct upd_t;

#ifndef UNIV_HOTBACKUP
extern bool row_rollback_on_timeout;

struct row_prebuilt_t;

/** Frees the blob heap in prebuilt when no longer needed. */
void row_mysql_prebuilt_free_blob_heap(
    row_prebuilt_t *prebuilt); /*!< in: prebuilt struct of a
                               ha_innobase:: table handle */
/** Stores a >= 5.0.3 format true VARCHAR length to dest, in the MySQL row
 format.
 @return pointer to the data, we skip the 1 or 2 bytes at the start
 that are used to store the len */
byte *row_mysql_store_true_var_len(
    byte *dest,    /*!< in: where to store */
    ulint len,     /*!< in: length, must fit in two bytes */
    ulint lenlen); /*!< in: storage length of len: either 1 or 2 bytes */
/** Reads a >= 5.0.3 format true VARCHAR length, in the MySQL row format, and
 returns a pointer to the data.
 @return pointer to the data, we skip the 1 or 2 bytes at the start
 that are used to store the len */
const byte *row_mysql_read_true_varchar(
    ulint *len,        /*!< out: variable-length field length */
    const byte *field, /*!< in: field in the MySQL format */
    ulint lenlen);     /*!< in: storage length of len: either 1
                       or 2 bytes */

/** Stores a reference to a BLOB in the MySQL format.
@param[in] dest Where to store
@param[in,out] col_len Dest buffer size: determines into how many bytes the blob
length is stored, the space for the length may vary from 1 to 4 bytes
@param[in] data Blob data; if the value to store is sql null this should be null
pointer
@param[in] len Blob length; if the value to store is sql null this should be 0;
remember also to set the null bit in the mysql record header! */
void row_mysql_store_blob_ref(byte *dest, ulint col_len, const void *data,
                              ulint len);

/** Reads a reference to a BLOB in the MySQL format.
@param[out] len                 BLOB length.
@param[in] ref                  BLOB reference in the MySQL format.
@param[in] col_len              BLOB reference length (not BLOB length).
@return pointer to BLOB data */
const byte *row_mysql_read_blob_ref(ulint *len, const byte *ref, ulint col_len);

/** Converts InnoDB geometry data format to MySQL data format. */
void row_mysql_store_geometry(
    byte *dest,      /*!< in/out: where to store */
    ulint dest_len,  /*!< in: dest buffer size: determines into
                     how many bytes the geometry length is stored,
                     the space for the length may vary from 1
                     to 4 bytes */
    const byte *src, /*!< in: geometry data; if the value to store
                     is SQL NULL this should be NULL pointer */
    ulint src_len);  /*!< in: geometry length; if the value to store
                     is SQL NULL this should be 0; remember
                     also to set the NULL bit in the MySQL record
                     header! */

/** Pad a column with spaces.
@param[in] mbminlen Minimum size of a character, in bytes
@param[out] pad Padded buffer
@param[in] len Number of bytes to pad */
void row_mysql_pad_col(ulint mbminlen, byte *pad, ulint len);

/** Stores a non-SQL-NULL field given in the MySQL format in the InnoDB format.
 The counterpart of this function is row_sel_field_store_in_mysql_format() in
 row0sel.cc.
 @return up to which byte we used buf in the conversion */
byte *row_mysql_store_col_in_innobase_format(
    dfield_t *dfield,       /*!< in/out: dfield where dtype
                            information must be already set when
                            this function is called! */
    byte *buf,              /*!< in/out: buffer for a converted
                            integer value; this must be at least
                            col_len long then! NOTE that dfield
                            may also get a pointer to 'buf',
                            therefore do not discard this as long
                            as dfield is used! */
    bool row_format_col,    /*!< true if the mysql_data is from
                             a MySQL row, false if from a MySQL
                             key value;
                             in MySQL, a true VARCHAR storage
                             format differs in a row and in a
                             key value: in a key value the length
                             is always stored in 2 bytes! */
    const byte *mysql_data, /*!< in: MySQL column value, not
                            SQL NULL; NOTE that dfield may also
                            get a pointer to mysql_data,
                            therefore do not discard this as long
                            as dfield is used! */
    ulint col_len,          /*!< in: MySQL column length; NOTE that
                            this is the storage length of the
                            column in the MySQL format row, not
                            necessarily the length of the actual
                            payload data; if the column is a true
                            VARCHAR then this is irrelevant */
    ulint comp);            /*!< in: nonzero=compact format */
/** Handles user errors and lock waits detected by the database engine.
 @return true if it was a lock wait and we should continue running the
 query thread */
bool row_mysql_handle_errors(
    dberr_t *new_err,      /*!< out: possible new error encountered in
                           rollback, or the old error which was
                           during the function entry */
    trx_t *trx,            /*!< in: transaction */
    que_thr_t *thr,        /*!< in: query thread, or NULL */
    trx_savept_t *savept); /*!< in: savepoint, or NULL */
/** Create a prebuilt struct for a MySQL table handle.
 @return own: a prebuilt struct */
row_prebuilt_t *row_create_prebuilt(
    dict_table_t *table,  /*!< in: Innobase table handle */
    ulint mysql_row_len); /*!< in: length in bytes of a row in
                          the MySQL format */

/** Free a prebuilt struct for a MySQL table handle.
@param[in,out] prebuilt Prebuilt struct
@param[in] dict_locked True=data dictionary locked */
void row_prebuilt_free(row_prebuilt_t *prebuilt, bool dict_locked);

/** Updates the transaction pointers in query graphs stored in the prebuilt
struct.
@param[in,out] prebuilt         Prebuilt struct in MySQL handle.
@param[in,out] trx              Transaction handle. */
void row_update_prebuilt_trx(row_prebuilt_t *prebuilt, trx_t *trx);

/** Sets an AUTO_INC type lock on the table mentioned in prebuilt. The
 AUTO_INC lock gives exclusive access to the auto-inc counter of the
 table. The lock is reserved only for the duration of an SQL statement.
 It is not compatible with another AUTO_INC or exclusive lock on the
 table.
 @return error code or DB_SUCCESS */
[[nodiscard]] dberr_t row_lock_table_autoinc_for_mysql(
    row_prebuilt_t *prebuilt); /*!< in: prebuilt struct in the MySQL
                              table handle */

/** Sets a table lock on the table mentioned in prebuilt.
@param[in,out]  prebuilt        table handle
@return error code or DB_SUCCESS */
dberr_t row_lock_table(row_prebuilt_t *prebuilt);

/** Does an insert for MySQL.
@param[in]      mysql_rec       row in the MySQL format
@param[in,out]  prebuilt        prebuilt struct in MySQL handle
@return error code or DB_SUCCESS*/
[[nodiscard]] dberr_t row_insert_for_mysql(const byte *mysql_rec,
                                           row_prebuilt_t *prebuilt);

/** Builds a dummy query graph used in selects. */
void row_prebuild_sel_graph(row_prebuilt_t *prebuilt); /*!< in: prebuilt struct
                                                       in MySQL handle */
/** Gets pointer to a prebuilt update vector used in updates. If the update
 graph has not yet been built in the prebuilt struct, then this function
 first builds it.
 @return prebuilt update vector */
upd_t *row_get_prebuilt_update_vector(
    row_prebuilt_t *prebuilt); /*!< in: prebuilt struct in MySQL
                               handle */
/** Checks if a table is such that we automatically created a clustered
 index on it (on row id).
 @return true if the clustered index was generated automatically */
bool row_table_got_default_clust_index(
    const dict_table_t *table); /*!< in: table */

/** Does an update or delete of a row for MySQL.
@param[in]      mysql_rec       row in the MySQL format
@param[in,out]  prebuilt        prebuilt struct in MySQL handle
@return error code or DB_SUCCESS */
[[nodiscard]] dberr_t row_update_for_mysql(const byte *mysql_rec,
                                           row_prebuilt_t *prebuilt);

/** Delete all rows for the given table by freeing/truncating indexes.
@param[in,out]  table   table handler */
void row_delete_all_rows(dict_table_t *table);

#endif /* !UNIV_HOTBACKUP */

/** Checks if a table name contains the string "/#sql" which denotes temporary
 tables in MySQL.
 @return true if temporary table */
[[nodiscard]] bool row_is_mysql_tmp_table_name(const char *name);
/*!< in: table name in the form
'database/tablename' */

#ifndef UNIV_HOTBACKUP
/** Creates an query graph node of 'update' type to be used in the MySQL
 interface.
 @return own: update node */
upd_node_t *row_create_update_node_for_mysql(
    dict_table_t *table, /*!< in: table to update */
    mem_heap_t *heap);   /*!< in: mem heap from which allocated */
/** Does a cascaded delete or set null in a foreign key operation.
 @return error code or DB_SUCCESS */
[[nodiscard]] dberr_t row_update_cascade_for_mysql(
    que_thr_t *thr,      /*!< in: query thread */
    upd_node_t *node,    /*!< in: update node used in the cascade
                         or set null operation */
    dict_table_t *table) /*!< in: table where we do the operation */
    MY_ATTRIBUTE((nonnull));

/** Locks the data dictionary exclusively for performing a table create or other
 data dictionary modification operation.
@param[in,out] trx Transaction
@param[in] location Location */
void row_mysql_lock_data_dictionary(trx_t *trx, ut::Location location);

/** Unlocks the data dictionary exclusive lock. */
void row_mysql_unlock_data_dictionary(trx_t *trx); /*!< in/out: transaction */

/** Locks the data dictionary in shared mode from modifications, for performing
 foreign key check, rollback, or other operation invisible to MySQL.
@param[in,out] trx Transaction
@param[in] location Location */
void row_mysql_freeze_data_dictionary(trx_t *trx, ut::Location location);

/** Unlocks the data dictionary shared lock. */
void row_mysql_unfreeze_data_dictionary(trx_t *trx); /*!< in/out: transaction */

/** Creates a table for MySQL. On success the in-memory table could be
kept in non-LRU list while on failure the 'table' object will be freed.
@param[in,out]	table		table definition(will be freed, or on
                                DB_SUCCESS added to the data dictionary cache)
@param[in]      compression     compression algorithm to use, can be nullptr
@param[in]      create_info     HA_CREATE_INFO object
@param[in,out]  trx             transaction
@param[in]      heap            temp memory heap or nullptr
@return error code or DB_SUCCESS */
[[nodiscard]] dberr_t row_create_table_for_mysql(
    dict_table_t *&table, const char *compression,
    const HA_CREATE_INFO *create_info, trx_t *trx, mem_heap_t *heap);

/** Does an index creation operation for MySQL. TODO: currently failure
 to create an index results in dropping the whole table! This is no problem
 currently as all indexes must be created at the same time as the table.
 @return error number or DB_SUCCESS */
[[nodiscard]] dberr_t row_create_index_for_mysql(
    dict_index_t *index,        /*!< in, own: index definition
                                (will be freed) */
    trx_t *trx,                 /*!< in: transaction handle */
    const ulint *field_lengths, /*!< in: if not NULL, must contain
                                dict_index_get_n_fields(index)
                                actual field lengths for the
                                index columns, which are
                                then checked for not being too
                                large. */
    dict_table_t *handler);     /* ! in/out: table handler. */

/** Loads foreign key constraints for the table being created. This
 function should be called after the indexes for a table have been
 created. Each foreign key constraint must be accompanied with indexes
 in both participating tables. The indexes are allowed to contain more
 fields than mentioned in the constraint.

 @param[in]     trx             transaction
 @param[in]     name            table full name in normalized form
 @param[in]     dd_table        MySQL dd::Table for the table
 @return error code or DB_SUCCESS */
[[nodiscard]] dberr_t row_table_load_foreign_constraints(
    trx_t *trx, const char *name, const dd::Table *dd_table);

/** The master thread in srv0srv.cc calls this regularly to drop tables which
 we must drop in background after queries to them have ended. Such lazy
 dropping of tables is needed in ALTER TABLE on Unix.
 @return how many tables dropped + remaining tables in list */
ulint row_drop_tables_for_mysql_in_background(void);

/** Sets an exclusive lock on a table.
 @return error code or DB_SUCCESS */
[[nodiscard]] dberr_t row_mysql_lock_table(
    trx_t *trx,           /*!< in/out: transaction */
    dict_table_t *table,  /*!< in: table to lock */
    enum lock_mode mode,  /*!< in: LOCK_X or LOCK_S */
    const char *op_info); /*!< in: string for trx->op_info */

/** Drop a tablespace as part of dropping or renaming a table.
This deletes the fil_space_t if found and the file on disk.
@param[in]      space_id        Tablespace ID
@param[in]      filepath        File path of tablespace to delete
@return error code or DB_SUCCESS */
dberr_t row_drop_tablespace(space_id_t space_id, const char *filepath);

/** Drop a table for MySQL. If the data dictionary was not already locked
by the transaction, the transaction will be committed.  Otherwise, the
data dictionary will remain locked.
@param[in]      name            Table name
@param[in]      trx             Transaction handle
@param[in]      nonatomic       Whether it is permitted to release
and reacquire dict_operation_lock
@param[in,out]  handler         Table handler or NULL
@return error code or DB_SUCCESS */
dberr_t row_drop_table_for_mysql(const char *name, trx_t *trx, bool nonatomic,
                                 dict_table_t *handler = nullptr);
/** Drop a table for MySQL. If the data dictionary was not already locked
by the transaction, the transaction will be committed.  Otherwise, the
data dictionary will remain locked.
@param[in]      name            table name
@param[in,out]  trx             data dictionary transaction
@return error code or DB_SUCCESS */
inline dberr_t row_drop_table_for_mysql(const char *name, trx_t *trx) {
  return (row_drop_table_for_mysql(name, trx, true, nullptr));
}

/** Discards the tablespace of a table which stored in an .ibd file. Discarding
 means that this function deletes the .ibd file and assigns a new table id for
 the table. Also the flag table->ibd_file_missing is set true.
 @return error code or DB_SUCCESS */
[[nodiscard]] dberr_t row_discard_tablespace_for_mysql(
    const char *name, /*!< in: table name */
    trx_t *trx);      /*!< in: transaction handle */

/** Drop a database for MySQL.
@param[in]      name    database name which ends at '/'
@param[in]      trx     transaction handle
@param[out]     found   number of dropped tables
@return error code or DB_SUCCESS */
dberr_t row_drop_database_for_mysql(const char *name, trx_t *trx, ulint *found);

/** Renames a table for MySQL.
@param[in]      old_name        old table name
@param[in]      new_name        new table name
@param[in]      dd_table        dd::Table for new table
@param[in,out]  trx             transaction
@param[in]      replay          whether in replay stage
@return error code or DB_SUCCESS */
[[nodiscard]] dberr_t row_rename_table_for_mysql(const char *old_name,
                                                 const char *new_name,
                                                 const dd::Table *dd_table,
                                                 trx_t *trx, bool replay);

/** Read the total number of records in a consistent view.
@param[in,out]  trx             Covering transaction.
@param[in]  indexes             Indexes to scan.
@param[in]  max_threads         Maximum number of threads to use.
@param[out] n_rows              Number of rows seen.
@return DB_SUCCESS or error code. */
dberr_t row_mysql_parallel_select_count_star(
    trx_t *trx, std::vector<dict_index_t *> &indexes, size_t max_threads,
    ulint *n_rows);

/** Scans an index for either COUNT(*) or CHECK TABLE.
If CHECK TABLE; Checks that the index contains entries in an ascending order,
unique constraint is not broken, and calculates the number of index entries
in the read view of the current transaction.
@param[in,out]  prebuilt    Prebuilt struct in MySQL handle.
@param[in,out]  index       Index to scan.
@param[in]      n_threads   Number of threads to use for the scan
@param[in]      check_keys  True if called form check table.
@param[out]     n_rows      Number of entries seen in the consistent read.
@return DB_SUCCESS or other error */
[[nodiscard]] dberr_t row_scan_index_for_mysql(row_prebuilt_t *prebuilt,
                                               dict_index_t *index,
                                               size_t n_threads,
                                               bool check_keys, ulint *n_rows);
/** Initialize this module */
void row_mysql_init(void);

/** Close this module */
void row_mysql_close(void);

/* A struct describing a place for an individual column in the MySQL
row format which is presented to the table handler in ha_innobase.
This template struct is used to speed up row transformations between
Innobase and MySQL. */

struct mysql_row_templ_t {
  ulint col_no;                 /*!< column number of the column */
  ulint rec_field_no;           /*!< field number of the column in an
                                Innobase record in the current index;
                                not defined if template_type is
                                ROW_MYSQL_WHOLE_ROW */
  ulint clust_rec_field_no;     /*!< field number of the column in an
                                Innobase record in the clustered index;
                                not defined if template_type is
                                ROW_MYSQL_WHOLE_ROW */
  ulint icp_rec_field_no;       /*!< field number of the column in an
                                Innobase record in the current index;
                                only defined for columns that could be
                                used to evaluate a pushed down index
                                condition and/or end-range condition */
  ulint mysql_col_offset;       /*!< offset of the column in the MySQL
                                row format */
  ulint mysql_col_len;          /*!< length of the column in the MySQL
                                row format */
  ulint mysql_mvidx_len;        /*!< index length on multi-value array */
  ulint mysql_null_byte_offset; /*!< MySQL NULL bit byte offset in a
                                MySQL record */
  ulint mysql_null_bit_mask;    /*!< bit mask to get the NULL bit,
                                zero if column cannot be NULL */
  ulint type;                   /*!< column type in Innobase mtype
                                numbers DATA_CHAR... */
  ulint mysql_type;             /*!< MySQL type code; this is always
                                < 256 */
  ulint mysql_length_bytes;     /*!< if mysql_type
                                == DATA_MYSQL_TRUE_VARCHAR, this tells
                                whether we should use 1 or 2 bytes to
                                store the MySQL true VARCHAR data
                                length at the start of row in the MySQL
                                format (NOTE that the MySQL key value
                                format always uses 2 bytes for the data
                                len) */
  ulint charset;                /*!< MySQL charset-collation code
                                of the column, or zero */
  ulint mbminlen;               /*!< minimum length of a char, in bytes,
                                or zero if not a char type */
  ulint mbmaxlen;               /*!< maximum length of a char, in bytes,
                                or zero if not a char type */
  ulint is_unsigned;            /*!< if a column type is an integer
                                type and this field is != 0, then
                                it is an unsigned integer type */
  ulint is_virtual;             /*!< if a column is a virtual column */
  ulint is_multi_val;           /*!< if a column is a Multi-Value Array virtual
                                column */
};

constexpr uint32_t MYSQL_FETCH_CACHE_SIZE = 8;
/* After fetching this many rows, we start caching them in fetch_cache */
constexpr uint32_t MYSQL_FETCH_CACHE_THRESHOLD = 4;

constexpr uint32_t ROW_PREBUILT_ALLOCATED = 78540783;
constexpr uint32_t ROW_PREBUILT_FREED = 26423527;

/** A struct for (sometimes lazily) prebuilt structures in an Innobase table
handle used within MySQL; these are used to save CPU time. */

struct row_prebuilt_t {
  ulint magic_n;               /*!< this magic number is set to
                               ROW_PREBUILT_ALLOCATED when created,
                               or ROW_PREBUILT_FREED when the
                               struct has been freed */
  dict_table_t *table;         /*!< Innobase table handle */
  dict_index_t *index;         /*!< current index for a search, if
                               any */
  trx_t *trx;                  /*!< current transaction handle */
  unsigned sql_stat_start : 1; /*!< true when we start processing of
                              an SQL statement: we may have to set
                              an intention lock on the table,
                              create a consistent read view etc. */
  unsigned clust_index_was_generated : 1;
  /*!< if the user did not define a
  primary key in MySQL, then Innobase
  automatically generated a clustered
  index where the ordering column is
  the row id: in this case this flag
  is set to true */
  unsigned index_usable : 1;               /*!< caches the value of
                                           index->is_usable(trx) */
  unsigned read_just_key : 1;              /*!< set to 1 when MySQL calls
                                           ha_innobase::extra with the
                                           argument HA_EXTRA_KEYREAD; it is enough
                                           to read just columns defined in
                                           the index (i.e., no read of the
                                           clustered index record necessary) */
  unsigned used_in_HANDLER : 1;            /*!< true if we have been using this
                                         handle in a MySQL HANDLER low level
                                         index cursor command: then we must
                                         store the pcur position even in a
                                         unique search from a clustered index,
                                         because HANDLER allows NEXT and PREV
                                         in such a situation */
  unsigned template_type : 2;              /*!< ROW_MYSQL_WHOLE_ROW,
                                           ROW_MYSQL_REC_FIELDS,
                                           ROW_MYSQL_DUMMY_TEMPLATE, or
                                           ROW_MYSQL_NO_TEMPLATE */
  unsigned n_template : 10;                /*!< number of elements in the
                                           template */
  unsigned null_bitmap_len : 10;           /*!< number of bytes in the SQL NULL
                                        bitmap at the start of a row in the
                                        MySQL format */
  unsigned need_to_access_clustered : 1;   /*!< if we are fetching
                               columns through a secondary index
                               and at least one column is not in
                               the secondary index, then this is
                               set to true */
  unsigned templ_contains_blob : 1;        /*!< true if the template contains
                                     a column with DATA_LARGE_MTYPE(
                                     get_innobase_type_from_mysql_type())
                                     is true;
                                     not to be confused with InnoDB
                                     externally stored columns
                                     (VARCHAR can be off-page too) */
  unsigned templ_contains_fixed_point : 1; /*!< true if the
                              template contains a column with
                              DATA_POINT. Since InnoDB regards
                              DATA_POINT as non-BLOB type, the
                              templ_contains_blob can't tell us
                              if there is DATA_POINT */

  /** 1 if extra(HA_EXTRA_INSERT_WITH_UPDATE) was requested, which happens
  when ON DUPLICATE KEY UPDATE clause is present, 0 otherwise */
  unsigned on_duplicate_key_update : 1;

  /** 1 if extra(HA_EXTRA_WRITE_CAN_REPLACE) was requested, which happen when
  REPLACE is done instead of regular INSERT, 0 otherwise */
  unsigned replace : 1;

  /** template used to transform rows fast between MySQL and Innobase formats;
  memory for this template is not allocated from 'heap' */
  mysql_row_templ_t *mysql_template;

  /** memory heap from which these auxiliary structures are allocated when
  needed */
  mem_heap_t *heap;

  /** memory heap from which innodb_api_buf is allocated per session */
  mem_heap_t *cursor_heap;

  /** Innobase SQL insert node used to perform inserts to the table */
  ins_node_t *ins_node;

  /** buffer for storing data converted to the Innobase format from the MySQL
  format */
  byte *ins_upd_rec_buff;

  /* buffer for converting data format for multi-value virtual columns */
  multi_value_data *mv_data;
  const byte *default_rec; /*!< the default values of all columns
                           (a "default row") in MySQL format */
  ulint hint_need_to_fetch_extra_cols;
  /*!< normally this is set to 0; if this
  is set to ROW_RETRIEVE_PRIMARY_KEY,
  then we should at least retrieve all
  columns in the primary key; if this
  is set to ROW_RETRIEVE_ALL_COLS, then
  we must retrieve all columns in the
  key (if read_just_key == 1), or all
  columns in the table */
  upd_node_t *upd_node;   /*!< Innobase SQL update node used
                          to perform updates and deletes */
  trx_id_t trx_id;        /*!< The table->def_trx_id when
                          ins_graph was built */
  que_fork_t *ins_graph;  /*!< Innobase SQL query graph used
                          in inserts. Will be rebuilt on
                          trx_id or n_indexes mismatch. */
  que_fork_t *upd_graph;  /*!< Innobase SQL query graph used
                          in updates or deletes */
  btr_pcur_t *pcur;       /*!< persistent cursor used in selects
                          and updates */
  btr_pcur_t *clust_pcur; /*!< persistent cursor used in
                          some selects and updates */
  que_fork_t *sel_graph;  /*!< dummy query graph used in
                          selects */
  dtuple_t *search_tuple; /*!< prebuilt dtuple used in selects */

  /** prebuilt dtuple used in selects where the end of range is known */
  dtuple_t *m_stop_tuple;

  /** Set to true in row_search_mvcc when a row matching exactly the length and
  value of stop_tuple was found, so that the next iteration of row_search_mvcc
  knows it can simply return DB_RECORD_NOT_FOUND. If true, then for sure, at
  least one such matching row was seen. If false, it might be false negative, as
  not all control paths lead to setting this field to true in case a matching
  row is visited. */
  bool m_stop_tuple_found;

 private:
  /** Set to true iff we are inside read_range_first() or read_range_next() */
  bool m_is_reading_range;

 public:
  bool is_reading_range() const { return m_is_reading_range; }

  class row_is_reading_range_guard_t : private ut::bool_scope_guard_t {
   public:
    explicit row_is_reading_range_guard_t(row_prebuilt_t &prebuilt)
        : ut::bool_scope_guard_t(prebuilt.m_is_reading_range) {}
  };

  row_is_reading_range_guard_t get_is_reading_range_guard() {
    /* We implement row_is_reading_range_guard_t as a simple bool_scope_guard_t
    because we trust that scopes are never nested and thus we don't need to
    count their "openings" and "closings", so we assert that.*/
    ut_ad(!m_is_reading_range);
    return row_is_reading_range_guard_t(*this);
  }

  byte row_id[DATA_ROW_ID_LEN];
  /*!< if the clustered index was
  generated, the row id of the
  last row fetched is stored
  here */
  doc_id_t fts_doc_id;          /* if the table has an FTS index on
                                it then we fetch the doc_id.
                                FTS-FIXME: Currently we fetch it always
                                but in the future we must only fetch
                                it when FTS columns are being
                                updated */
  dtuple_t *clust_ref;          /*!< prebuilt dtuple used in
                                sel/upd/del */
  ulint select_lock_type;       /*!< LOCK_NONE, LOCK_S, or LOCK_X */
  enum select_mode select_mode; /*!< SELECT_ORDINARY,
                                SELECT_SKIP_LOKCED, or SELECT_NO_WAIT */
  ulint row_read_type;          /*!< ROW_READ_WITH_LOCKS if row locks
                                should be the obtained for records
                                under an UPDATE or DELETE cursor.
                                If trx_t::allow_semi_consistent()
                                returns true, this can be set to
                                ROW_READ_TRY_SEMI_CONSISTENT, so that
                                if the row under an UPDATE or DELETE
                                cursor was locked by another
                                transaction, InnoDB will resort
                                to reading the last committed value
                                ('semi-consistent read').  Then,
                                this field will be set to
                                ROW_READ_DID_SEMI_CONSISTENT to
                                indicate that.  If the row does not
                                match the WHERE condition, MySQL will
                                invoke handler::unlock_row() to
                                clear the flag back to
                                ROW_READ_TRY_SEMI_CONSISTENT and
                                to simply skip the row.  If
                                the row matches, the next call to
                                row_search_for_mysql() will lock
                                the row.
                                This eliminates lock waits in some
                                cases; note that this breaks
                                serializability. */

  /** byte offset of the end of the last requested column*/
  ulint mysql_prefix_len;

  /** length in bytes of a row in the MySQL format */
  ulint mysql_row_len;

  /** number of rows fetched after positioning the current cursor  */
  ulint n_rows_fetched;

  /** ROW_SEL_NEXT or ROW_SEL_PREV */
  ulint fetch_direction;

  byte *fetch_cache[MYSQL_FETCH_CACHE_SIZE];
  /*!< a cache for fetched rows if we
  fetch many rows from the same cursor:
  it saves CPU time to fetch them in a
  batch; we reserve mysql_row_len
  bytes for each such row; these
  pointers point 4 bytes past the
  allocated mem buf start, because
  there is a 4 byte magic number at the
  start and at the end */
  ulint fetch_cache_first;   /*!< position of the first not yet
                           fetched row in fetch_cache */
  ulint n_fetch_cached;      /*!< number of not yet fetched rows
                             in fetch_cache */
  mem_heap_t *blob_heap;     /*!< in SELECTS BLOB fields are copied
                             to this heap */
  mem_heap_t *old_vers_heap; /*!< memory heap where a previous
                             version is built in consistent read */
  enum {
    LOCK_PCUR,
    LOCK_CLUST_PCUR,
    LOCK_COUNT,
  };
  /** normally false;
  if session is using READ COMMITTED or READ UNCOMMITTED isolation level, set in
  row_search_for_mysql() if we set a new record lock on the secondary or
  clustered index;
  this is used in row_try_unlock() when releasing the lock under the cursor if
  we determine after retrieving the row that it does not need to be locked
  ('mini-rollback')

    [LOCK_PCUR] corresponds to pcur, the first index we looked up
                (can be secondary or clustered!)

    [LOCK_CLUST_PCUR] corresponds to clust_pcur, which if used at all, is always
                      the clustered index.

  The meaning of these booleans is:
    true = we've created a rec lock, which we might release as we "own" it
    false = we should not release any lock for this index as we either reused
            some existing lock, or there is some other reason, we should keep it
  */
  std::bitset<LOCK_COUNT> new_rec_lock;
  bool keep_other_fields_on_keyread; /*!< when using fetch
                        cache with HA_EXTRA_KEYREAD, don't
                        overwrite other fields in mysql row
                        row buffer.*/
  bool in_fts_query;                 /*!< Whether we are in a FTS query */
  bool fts_doc_id_in_read_set;       /*!< true if table has externally
                             defined FTS_DOC_ID coulmn. */
  /*----------------------*/
  ulonglong autoinc_last_value;
  /*!< last value of AUTO-INC interval */
  ulonglong autoinc_increment; /*!< The increment step of the auto
                             increment column. Value must be
                             greater than or equal to 1. Required to
                             calculate the next value */
  ulonglong autoinc_offset;    /*!< The offset passed to
                               get_auto_increment() by MySQL. Required
                               to calculate the next value */
  dberr_t autoinc_error;       /*!< The actual error code encountered
                               while trying to init or read the
                               autoinc value from the table. We
                               store it here so that we can return
                               it to MySQL */
  /*----------------------*/
  bool idx_cond;         /*!< True if index condition pushdown
                         is used, false otherwise. */
  ulint idx_cond_n_cols; /*!< Number of fields in idx_cond_cols.
                         0 if and only if idx_cond == false. */
  /*----------------------*/
  unsigned innodb_api : 1;     /*!< whether this is a InnoDB API
                               query */
  const rec_t *innodb_api_rec; /*!< InnoDB API search result */
  void *innodb_api_buf;        /*!< Buffer holding copy of the physical
                               Innodb API search record */
  ulint innodb_api_rec_size;   /*!< Size of the Innodb API record */
  /*----------------------*/

  /*----------------------*/
  rtr_info_t *rtr_info; /*!< R-tree Search Info */
  /*----------------------*/

  ulint magic_n2; /*!< this should be the same as
                  magic_n */

  bool ins_sel_stmt; /*!< if true then ins_sel_statement. */

  innodb_session_t *session; /*!< InnoDB session handler. */
  byte *srch_key_val1;       /*!< buffer used in converting
                             search key values from MySQL format
                             to InnoDB format.*/
  byte *srch_key_val2;       /*!< buffer used in converting
                             search key values from MySQL format
                             to InnoDB format.*/
  uint srch_key_val_len;     /*!< Size of search key */
  /** Disable prefetch. */
  bool m_no_prefetch;

  /** true, if server has called ha_extra(HA_EXTRA_NO_READ_LOCKING) */
  bool no_read_locking;

  /** true, if we were asked to skip AUTOINC locking for the table. */
  bool no_autoinc_locking;

  /** Return materialized key for secondary index scan */
  bool m_read_virtual_key;

  /** Whether this is a temporary(intrinsic) table read to keep the position
  for this MySQL TABLE object */
  bool m_temp_read_shared;

  /** Whether tree modifying operation happened on a temporary (intrinsic)
  table index tree. In this case, it could be split, but no shrink. */
  bool m_temp_tree_modified;

  /** The MySQL table object */
  TABLE *m_mysql_table;

  /** The MySQL handler object. */
  ha_innobase *m_mysql_handler;

  /** limit value to avoid fts result overflow */
  ulonglong m_fts_limit;

  /** True if exceeded the end_range while filling the prefetch cache. */
  bool m_end_range;

  /** Undo information for LOB mvcc */
  lob::undo_vers_t m_lob_undo;

  lob::undo_vers_t *get_lob_undo() { return (&m_lob_undo); }

  void lob_undo_reset() { m_lob_undo.reset(); }

  /** Can a record buffer or a prefetch cache be utilized for prefetching
  records in this scan?
  @retval true   if records can be prefetched
  @retval false  if records cannot be prefetched */
  bool can_prefetch_records() const;

  /** Determines if the query is REPLACE or ON DUPLICATE KEY UPDATE in which
  case duplicate values should be allowed (and further processed) instead of
  causing an error.
  @return true iff duplicated values should be allowed */
  bool allow_duplicates() { return (replace || on_duplicate_key_update); }

  /** This is an no-op unless trx is using a READ COMMITTED or READ UNCOMMITTED
  isolation level.
  Before calling this function row_search_for_mysql() must have stored to
  new_rec_locks[] the information which new record locks really were set.
  This function removes a newly set index record locks under pcur or clust_pcur.
  Thus, this implements a 'mini-rollback' that releases the latest index record
  locks we've just set.

  @param[in]    has_latches_on_recs     true if called so that we have the
                                        latches on the records under pcur
                                        and clust_pcur, and we do not need
                                        to reposition the cursors. */
  void try_unlock(bool has_latches_on_recs);

 private:
  /** A helper function for init_search_tuples_types() which prepares the shape
  of the tuple to match the index
  @param[in]  tuple   this->search_tuple or this->m_stop_tuple */
  void init_tuple_types(dtuple_t *tuple) {
    dtuple_set_n_fields(tuple, index->n_fields);
    dict_index_copy_types(tuple, index, index->n_fields);
  }

 public:
  /** Counts how many elements of @see new_rec_lock[] array are set to true. */
  size_t new_rec_locks_count() const { return new_rec_lock.count(); }

  /** Initializes search_tuple and m_stop_tuple shape so they match the index */
  void init_search_tuples_types() {
    init_tuple_types(search_tuple);
    init_tuple_types(m_stop_tuple);
  }

  /** Resets both search_tuple and m_stop_tuple */
  void clear_search_tuples() {
    dtuple_set_n_fields(search_tuple, 0);
    dtuple_set_n_fields(m_stop_tuple, 0);
  }

  /** Inside this function perform activity that needs to be done at the
  end of statement.  */
  void end_stmt();

  /** @return true iff the operation can skip concurrency ticket. */
  bool skip_concurrency_ticket() const;

  /** It is unsafe to copy this struct, and moving it would be non-trivial,
  because we want to keep in sync with row_is_reading_range_guard_t. Therefore
  it is much safer/easier to just forbid such operations.  */
  row_prebuilt_t(row_prebuilt_t const &) = delete;
  row_prebuilt_t &operator=(row_prebuilt_t const &) = delete;
  row_prebuilt_t &operator=(row_prebuilt_t &&) = delete;
  row_prebuilt_t(row_prebuilt_t &&) = delete;
};

/** Callback for row_mysql_sys_index_iterate() */
struct SysIndexCallback {
  virtual ~SysIndexCallback() = default;

  /** Callback method
  @param mtr current mini-transaction
  @param pcur persistent cursor. */
  virtual void operator()(mtr_t *mtr, btr_pcur_t *pcur) noexcept = 0;
};

/** Get the updated parent field value from the update vector for the
given col_no.
@param[in]      foreign         foreign key information
@param[in]      update          updated parent vector.
@param[in]      col_no          base column position of the child table to check
@return updated field from the parent update vector, else NULL */
dfield_t *innobase_get_field_from_update_vector(dict_foreign_t *foreign,
                                                upd_t *update, uint32_t col_no);

/** Get the computed value by supplying the base column values.
@param[in,out]  row             the data row
@param[in]      col             virtual column
@param[in]      index           index on the virtual column
@param[in,out]  local_heap      heap memory for processing large data etc.
@param[in,out]  heap            memory heap that copies the actual index row
@param[in]      ifield          index field
@param[in]      thd             MySQL thread handle
@param[in,out]  mysql_table     mysql table object
@param[in]      old_table       during ALTER TABLE, this is the old table
                                or NULL.
@param[in]      parent_update   update vector for the parent row
@param[in]      foreign         foreign key information
@return the field filled with computed value, or NULL if just want
to store the value in passed in "my_rec" */
dfield_t *innobase_get_computed_value(
    const dtuple_t *row, const dict_v_col_t *col, const dict_index_t *index,
    mem_heap_t **local_heap, mem_heap_t *heap, const dict_field_t *ifield,
    THD *thd, TABLE *mysql_table, const dict_table_t *old_table,
    upd_t *parent_update, dict_foreign_t *foreign);

/** Parse out multi-values from a MySQL record
@param[in]      mysql_table     MySQL table structure
@param[in]      f_idx           field index of the multi-value column
@param[in,out]  dfield          field structure to store parsed multi-value
@param[in,out]  value           nullptr or the multi-value structure
                                to store the parsed values
@param[in]      old_val         old value if exists
@param[in]      comp            true if InnoDB table uses compact row format
@param[in,out]  heap            memory heap */
void innobase_get_multi_value(const TABLE *mysql_table, ulint f_idx,
                              dfield_t *dfield, multi_value_data *value,
                              uint old_val, ulint comp, mem_heap_t *heap);

/** Get the computed value by supplying the base column values.
@param[in,out]  table   the table whose virtual column template to be built */
void innobase_init_vc_templ(dict_table_t *table);

/** Change dbname and table name in table->vc_templ.
@param[in,out]  table   the table whose virtual column template
dbname and tbname to be renamed. */
void innobase_rename_vc_templ(dict_table_t *table);

constexpr uint32_t ROW_PREBUILT_FETCH_MAGIC_N = 465765687;

constexpr uint32_t ROW_MYSQL_WHOLE_ROW = 0;
constexpr uint32_t ROW_MYSQL_REC_FIELDS = 1;
constexpr uint32_t ROW_MYSQL_NO_TEMPLATE = 2;
/** dummy template used in row_scan_and_check_index */
constexpr uint32_t ROW_MYSQL_DUMMY_TEMPLATE = 3;

/* Values for hint_need_to_fetch_extra_cols */
constexpr uint32_t ROW_RETRIEVE_PRIMARY_KEY = 1;
constexpr uint32_t ROW_RETRIEVE_ALL_COLS = 2;

/* Values for row_read_type */
constexpr uint32_t ROW_READ_WITH_LOCKS = 0;
constexpr uint32_t ROW_READ_TRY_SEMI_CONSISTENT = 1;
constexpr uint32_t ROW_READ_DID_SEMI_CONSISTENT = 2;

#ifdef UNIV_DEBUG
/** Wait for the background drop list to become empty. */
void row_wait_for_background_drop_list_empty();
#endif /* UNIV_DEBUG */
#endif /* !UNIV_HOTBACKUP */

#endif /* row0mysql.h */
