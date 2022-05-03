/*****************************************************************************

Copyright (c) 2006, 2022, Oracle and/or its affiliates.

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

/** @file include/fts0fts.h
 Full text search header file

 Created 2011/09/02 Sunny Bains
 ***********************************************************************/

#ifndef fts0fts_h
#define fts0fts_h

#include "ha_prototypes.h"

#include "data0type.h"
#include "data0types.h"
#include "dict0types.h"
#include "ft_global.h"
#include "hash0hash.h"
#include "mem0mem.h"
#include "mysql/plugin_ftparser.h"
#include "que0types.h"
#include "rem0types.h"
#include "row0types.h"
#include "trx0types.h"
#include "ut0rbt.h"
#include "ut0vec.h"
#include "ut0wqueue.h"

/** "NULL" value of a document id. */
#define FTS_NULL_DOC_ID 0

/** FTS hidden column that is used to map to and from the row */
#define FTS_DOC_ID_COL_NAME "FTS_DOC_ID"

/** The name of the index created by FTS */
#define FTS_DOC_ID_INDEX_NAME "FTS_DOC_ID_INDEX"

#define FTS_DOC_ID_INDEX_NAME_LEN 16

/** Doc ID is a 8 byte value */
#define FTS_DOC_ID_LEN 8

/** The number of fields to sort when we build FT index with
FIC. Three fields are sort: (word, doc_id, position) */
#define FTS_NUM_FIELDS_SORT 3

/** Maximum number of rows in a table, smaller than which, we will
optimize using a 4 byte Doc ID for FIC merge sort to reduce sort size */
#define MAX_DOC_ID_OPT_VAL 1073741824

/** Document id type. */
typedef uint64_t doc_id_t;

/** doc_id_t printf format */
#define FTS_DOC_ID_FORMAT IB_ID_FMT

/** Convert document id to the InnoDB (BIG ENDIAN) storage format. */
#define fts_write_doc_id(d, s) mach_write_to_8(d, s)

/** Read a document id to internal format. */
#define fts_read_doc_id(s) mach_read_from_8(s)

/** Bind the doc id to a variable */
#define fts_bind_doc_id(i, n, v) pars_info_bind_int8_literal(i, n, v)

/** Defines for FTS query mode, they have the same values as
those defined in mysql file ft_global.h */
#define FTS_NL 0
#define FTS_BOOL 1
#define FTS_SORTED 2
#define FTS_EXPAND 4
#define FTS_NO_RANKING 8
#define FTS_PROXIMITY 16
#define FTS_PHRASE 32
#define FTS_OPT_RANKING 64

#define FTS_INDEX_TABLE_IND_NAME "FTS_INDEX_TABLE_IND"
#define FTS_COMMON_TABLE_IND_NAME "FTS_COMMON_TABLE_IND"

/** The number of FTS index partitions for a fulltext index. */
constexpr size_t FTS_NUM_AUX_INDEX = 6;

/** The number of FTS AUX common table for a fulltext index. */
constexpr size_t FTS_NUM_AUX_COMMON = 5;

/** Threshold where our optimize thread automatically kicks in */
#define FTS_OPTIMIZE_THRESHOLD 10000000

/** Threshold to avoid exhausting of doc ids. Consecutive doc id difference
should not exceed FTS_DOC_ID_MAX_STEP */
#define FTS_DOC_ID_MAX_STEP 65535

/** Maximum possible Fulltext word length */
#define FTS_MAX_WORD_LEN HA_FT_MAXBYTELEN

/** Maximum possible Fulltext word length (in characters) */
#define FTS_MAX_WORD_LEN_IN_CHAR HA_FT_MAXCHARLEN

/** Number of columns in FTS AUX Tables */
#define FTS_DELETED_TABLE_NUM_COLS 1
#define FTS_CONFIG_TABLE_NUM_COLS 2
#define FTS_AUX_INDEX_TABLE_NUM_COLS 5

/** DELETED_TABLE(doc_id BIGINT UNSIGNED) */
#define FTS_DELETED_TABLE_COL_LEN 8
/** CONFIG_TABLE(key CHAR(50), value CHAR(200)) */
#define FTS_CONFIG_TABLE_KEY_COL_LEN 50
#define FTS_CONFIG_TABLE_VALUE_COL_LEN 200

#define FTS_INDEX_WORD_LEN FTS_MAX_WORD_LEN
#define FTS_INDEX_FIRST_DOC_ID_LEN 8
#define FTS_INDEX_LAST_DOC_ID_LEN 8
#define FTS_INDEX_DOC_COUNT_LEN 4
/* BLOB COLUMN, 0 means VARIABLE SIZE */
#define FTS_INDEX_ILIST_LEN 0
/* Maximum nested expression in fulltext binary search string */
#define FTS_MAX_NESTED_EXP 31

extern const char *FTS_PREFIX;
extern const char *FTS_SUFFIX_BEING_DELETED;
extern const char *FTS_SUFFIX_BEING_DELETED_CACHE;
extern const char *FTS_SUFFIX_CONFIG;
extern const char *FTS_SUFFIX_DELETED;
extern const char *FTS_SUFFIX_DELETED_CACHE;

extern const char *FTS_PREFIX_5_7;
extern const char *FTS_SUFFIX_CONFIG_5_7;

/** Variable specifying the number of word to optimize for each optimize table
call */
extern ulong fts_num_word_optimize;

/** Variable specifying whether we do additional FTS diagnostic printout
in the log */
extern bool fts_enable_diag_print;

/** FTS rank type, which will be between 0 .. 1 inclusive */
typedef float fts_rank_t;

/** Structure to manage FTS AUX table name and MDL during its drop */
struct aux_name_vec_t {
  /** AUX table name */
  std::vector<char *> aux_name;
};

/** Type of a row during a transaction. FTS_NOTHING means the row can be
forgotten from the FTS system's POV, FTS_INVALID is an internal value used
to mark invalid states.

NOTE: Do not change the order or value of these, fts_trx_row_get_new_state
depends on them being exactly as they are. */
enum fts_row_state {
  FTS_INSERT = 0,
  FTS_MODIFY,
  FTS_DELETE,
  FTS_NOTHING,
  FTS_INVALID
};

/** The FTS table types. */
enum fts_table_type_t {
  FTS_INDEX_TABLE, /*!< FTS auxiliary table that is
                   specific to a particular FTS index
                   on a table */

  FTS_COMMON_TABLE, /*!< FTS auxiliary table that is common
                    for all FTS index on a table */

  FTS_OBSOLETED_TABLE /*!< FTS obsoleted tables like DOC_ID,
                      ADDED, STOPWORDS */
};

struct fts_doc_t;
struct fts_cache_t;
struct fts_token_t;
struct fts_doc_ids_t;
struct fts_index_cache_t;

/** Initialize the "fts_table" for internal query into FTS auxiliary
tables */
#define FTS_INIT_FTS_TABLE(fts_table, m_suffix, m_type, m_table) \
  do {                                                           \
    (fts_table)->suffix = m_suffix;                              \
    (fts_table)->type = m_type;                                  \
    (fts_table)->table_id = m_table->id;                         \
    (fts_table)->parent = m_table->name.m_name;                  \
    (fts_table)->table = m_table;                                \
  } while (0);

#define FTS_INIT_INDEX_TABLE(fts_table, m_suffix, m_type, m_index) \
  do {                                                             \
    (fts_table)->suffix = m_suffix;                                \
    (fts_table)->type = m_type;                                    \
    (fts_table)->table_id = m_index->table->id;                    \
    (fts_table)->parent = m_index->table->name.m_name;             \
    (fts_table)->table = m_index->table;                           \
    (fts_table)->index_id = m_index->id;                           \
  } while (0);

/** Information about changes in a single transaction affecting
the FTS system. */
struct fts_trx_t {
  trx_t *trx; /*!< InnoDB transaction */

  ib_vector_t *savepoints; /*!< Active savepoints, must have at
                           least one element, the implied
                           savepoint */
  ib_vector_t *last_stmt;  /*!< last_stmt */

  mem_heap_t *heap; /*!< heap */
};

/** Information required for transaction savepoint handling. */
struct fts_savepoint_t {
  char *name; /*!< First entry is always NULL, the
              default instance. Otherwise the name
              of the savepoint */

  ib_rbt_t *tables; /*!< Modified FTS tables */
};

/** Information about changed rows in a transaction for a single table. */
struct fts_trx_table_t {
  dict_table_t *table; /*!< table */

  fts_trx_t *fts_trx; /*!< link to parent */

  ib_rbt_t *rows; /*!< rows changed; indexed by doc-id,
                  cells are fts_trx_row_t* */

  fts_doc_ids_t *added_doc_ids; /*!< list of added doc ids (NULL until
                                the first addition) */

  /*!< for adding doc ids */
  que_t *docs_added_graph;
};

/** Information about one changed row in a transaction. */
struct fts_trx_row_t {
  doc_id_t doc_id; /*!< Id of the ins/upd/del document */

  fts_row_state state; /*!< state of the row */

  ib_vector_t *fts_indexes; /*!< The indexes that are affected */
};

/** List of document ids that were added during a transaction. This
list is passed on to a background 'Add' thread and OPTIMIZE, so it
needs its own memory heap. */
struct fts_doc_ids_t {
  ib_vector_t *doc_ids; /*!< document ids (each element is
                        of type doc_id_t). */

  ib_alloc_t *self_heap; /*!< Allocator used to create an
                         instance of this type and the
                         doc_ids vector */
};

// FIXME: Get rid of this if possible.
/** Since MySQL's character set support for Unicode is woefully inadequate
(it supports basic operations like isalpha etc. only for 8-bit characters),
we have to implement our own. We use UTF-16 without surrogate processing
as our in-memory format. This typedef is a single such character. */
typedef unsigned short ib_uc_t;

/** An UTF-16 ro UTF-8 string. */
struct fts_string_t {
  byte *f_str;    /*!< string, not necessary terminated in
                  any way */
  ulint f_len;    /*!< Length of the string in bytes */
  ulint f_n_char; /*!< Number of characters */
};

/** Query ranked doc ids. */
struct fts_ranking_t {
  doc_id_t doc_id; /*!< Document id */

  fts_rank_t rank; /*!< Rank is between 0 .. 1 */

  byte *words;     /*!< this contains the words
                   that were queried
                   and found in this document */
  ulint words_len; /*!< words len */
};

/** Query result. */
struct fts_result_t {
  ib_rbt_node_t *current; /*!< Current element */

  ib_rbt_t *rankings_by_id;   /*!< RB tree of type fts_ranking_t
                              indexed by doc id */
  ib_rbt_t *rankings_by_rank; /*!< RB tree of type fts_ranking_t
                             indexed by rank */
};

/** This is used to generate the FTS auxiliary table name, we need the
table id and the index id to generate the column specific FTS auxiliary
table name. */
struct fts_table_t {
  const char *parent; /*!< Parent table name, this is
                      required only for the database
                      name */

  fts_table_type_t type; /*!< The auxiliary table type */

  table_id_t table_id; /*!< The table id */

  space_index_t index_id; /*!< The index id */

  const char *suffix;        /*!< The suffix of the fts auxiliary
                             table name, can be NULL, not used
                             everywhere (yet) */
  const dict_table_t *table; /*!< Parent table */
  CHARSET_INFO *charset;     /*!< charset info if it is for FTS
                             index auxiliary table */
};

enum fts_status {
  BG_THREAD_STOP = 1, /*!< true if the FTS background thread
                      has finished reading the ADDED table,
                      meaning more items can be added to
                      the table. */

  BG_THREAD_READY = 2, /*!< true if the FTS background thread
                       is ready */

  ADD_THREAD_STARTED = 4, /*!< true if the FTS add thread
                          has started */

  ADDED_TABLE_SYNCED = 8, /*!< true if the ADDED table record is
                          sync-ed after crash recovery */
};

typedef enum fts_status fts_status_t;

/** The state of the FTS sub system. */
class fts_t {
 public:
  /** fts_t constructor.
  @param[in]    table   table with FTS indexes
  @param[in,out]        heap    memory heap where 'this' is stored */
  fts_t(dict_table_t *table, mem_heap_t *heap);

  /** fts_t destructor. */
  ~fts_t();

  /** Mutex protecting bg_threads* and fts_add_wq. */
  ib_mutex_t bg_threads_mutex;

  /** Number of background threads accessing this table. */
  ulint bg_threads;

  /** Status bit regarding fts running state. true if background
  threads running should stop themselves. */
  ulint fts_status;

  /** Work queue for scheduling jobs for the FTS 'Add' thread, or NULL
  if the thread has not yet been created. Each work item is a
  fts_trx_doc_ids_t*. */
  ib_wqueue_t *add_wq;

  /** FTS memory buffer for this table, or NULL if the table has no FTS
  index. */
  fts_cache_t *cache;

  /** FTS doc id hidden column number in the CLUSTERED index. */
  ulint doc_col;

  /** Vector of FTS indexes, this is mainly for caching purposes. */
  ib_vector_t *indexes;

  /** Heap for fts_t allocation. */
  mem_heap_t *fts_heap;
};

struct fts_stopword_t;

/** status bits for fts_stopword_t status field. */
#define STOPWORD_NOT_INIT 0x1
#define STOPWORD_OFF 0x2
#define STOPWORD_FROM_DEFAULT 0x4
#define STOPWORD_USER_TABLE 0x8

extern const char *fts_default_stopword[];

/** Variable specifying the maximum FTS cache size for each table */
extern ulong fts_max_cache_size;

/** Variable specifying the total memory allocated for FTS cache */
extern ulong fts_max_total_cache_size;

/** Variable specifying the FTS result cache limit for each query */
extern ulong fts_result_cache_limit;

/** Variable specifying the maximum FTS max token size */
extern ulong fts_max_token_size;

/** Variable specifying the minimum FTS max token size */
extern ulong fts_min_token_size;

/** Whether the total memory used for FTS cache is exhausted, and we will
need a sync to free some memory */
extern bool fts_need_sync;

/** Variable specifying the table that has Fulltext index to display its
content through information schema table */
extern char *fts_internal_tbl_name;

#define fts_que_graph_free(graph) \
  do {                            \
    que_graph_free(graph);        \
  } while (0)

/** Create a FTS cache. */
fts_cache_t *fts_cache_create(
    dict_table_t *table); /*!< table owns the FTS cache */

/** Create a FTS index cache.
 @return Index Cache */
fts_index_cache_t *fts_cache_index_cache_create(
    dict_table_t *table,  /*!< in: table with FTS index */
    dict_index_t *index); /*!< in: FTS index */

/** Remove a FTS index cache
@param[in]      table   table with FTS index
@param[in]      index   FTS index */
void fts_cache_index_cache_remove(dict_table_t *table, dict_index_t *index);

/** Get the next available document id. This function creates a new
 transaction to generate the document id.
 @return DB_SUCCESS if OK */
dberr_t fts_get_next_doc_id(const dict_table_t *table, /*!< in: table */
                            doc_id_t *doc_id); /*!< out: new document id */
/** Update the next and last Doc ID in the CONFIG table to be the input
 "doc_id" value (+ 1). We would do so after each FTS index build or
 table truncate */
void fts_update_next_doc_id(
    trx_t *trx,                /*!< in/out: transaction */
    const dict_table_t *table, /*!< in: table */
    const char *table_name,    /*!< in: table name, or NULL */
    doc_id_t doc_id);          /*!< in: DOC ID to set */

/** Create a new document id.
@param[in]      table  Row is of this table.
@param[in,out]  row    Add doc id value to this row. This is the current row
that is being inserted.
@param[in]      heap   Memory heap on which the doc_id object will be created.
@return DB_SUCCESS if all went well else error */
dberr_t fts_create_doc_id(dict_table_t *table, dtuple_t *row, mem_heap_t *heap);

/** Create a new fts_doc_ids_t.
 @return new fts_doc_ids_t. */
fts_doc_ids_t *fts_doc_ids_create(void);

/** Free a fts_doc_ids_t. */
void fts_doc_ids_free(fts_doc_ids_t *doc_ids); /*!< in: doc_ids to free */

/** Notify the FTS system about an operation on an FTS-indexed table.
@param[in] trx Innodb transaction
@param[in] table Table
@param[in] doc_id Doc id
@param[in] state State of the row
@param[in] fts_indexes Fts indexes affected (null=all) */
void fts_trx_add_op(trx_t *trx, dict_table_t *table, doc_id_t doc_id,
                    fts_row_state state, ib_vector_t *fts_indexes);

/** Free an FTS trx. */
void fts_trx_free(fts_trx_t *fts_trx); /*!< in, own: FTS trx */

/** Check if common tables already exist
@param[in]      table   table with fts index
@return true on success, false on failure */
bool fts_check_common_tables_exist(const dict_table_t *table);

/** Creates the common auxiliary tables needed for supporting an FTS index
on the given table. row_mysql_lock_data_dictionary must have been called
before this.
The following tables are created.
CREATE TABLE $FTS_PREFIX_DELETED
        (doc_id BIGINT UNSIGNED, UNIQUE CLUSTERED INDEX on doc_id)
CREATE TABLE $FTS_PREFIX_DELETED_CACHE
        (doc_id BIGINT UNSIGNED, UNIQUE CLUSTERED INDEX on doc_id)
CREATE TABLE $FTS_PREFIX_BEING_DELETED
        (doc_id BIGINT UNSIGNED, UNIQUE CLUSTERED INDEX on doc_id)
CREATE TABLE $FTS_PREFIX_BEING_DELETED_CACHE
        (doc_id BIGINT UNSIGNED, UNIQUE CLUSTERED INDEX on doc_id)
CREATE TABLE $FTS_PREFIX_CONFIG
        (key CHAR(50), value CHAR(200), UNIQUE CLUSTERED INDEX on key)
@param[in,out]  trx                     transaction
@param[in]      table                   table with FTS index
@param[in]      name                    table name normalized
@param[in]      skip_doc_id_index       Skip index on doc id
@return DB_SUCCESS if succeed */
[[nodiscard]] dberr_t fts_create_common_tables(trx_t *trx,
                                               const dict_table_t *table,
                                               const char *name,
                                               bool skip_doc_id_index);

/** Creates the column specific ancillary tables needed for supporting an
FTS index on the given table. row_mysql_lock_data_dictionary must have
been called before this.

All FTS AUX Index tables have the following schema.
CREATE TABLE $FTS_PREFIX_INDEX_[1-6](
        word            VARCHAR(FTS_MAX_WORD_LEN),
        first_doc_id    INT NOT NULL,
        last_doc_id     UNSIGNED NOT NULL,
        doc_count       UNSIGNED INT NOT NULL,
        ilist           VARBINARY NOT NULL,
        UNIQUE CLUSTERED INDEX ON (word, first_doc_id))
@param[in,out]  trx     transaction
@param[in]      index   index instance
@return DB_SUCCESS or error code */
[[nodiscard]] dberr_t fts_create_index_tables(trx_t *trx, dict_index_t *index);

/** Create auxiliary index tables for an FTS index.
@param[in,out]  trx             transaction
@param[in]      index           the index instance
@param[in]      table_name      table name
@param[in]      table_id        the table id
@return DB_SUCCESS or error code */
[[nodiscard]] dberr_t fts_create_index_tables_low(trx_t *trx,
                                                  dict_index_t *index,
                                                  const char *table_name,
                                                  table_id_t table_id);

/** Add the FTS document id hidden column.
@param[in,out] table Table with FTS index
@param[in] heap Temporary memory heap, or NULL */
void fts_add_doc_id_column(dict_table_t *table, mem_heap_t *heap);

/** Drops the ancillary tables needed for supporting an FTS index on a
given table. row_mysql_lock_data_dictionary must have been called before
this.
@param[in,out]  trx     transaction
@param[in]      table   table has the fts index
@param[in,out]  aux_vec fts aux table name vector
@return DB_SUCCESS or error code */
dberr_t fts_drop_tables(trx_t *trx, dict_table_t *table,
                        aux_name_vec_t *aux_vec);

/** Lock all FTS AUX tables (for dropping table)
@param[in]      thd     thread locking the AUX table
@param[in]      table   table has the fts index
@return DB_SUCCESS or error code */
dberr_t fts_lock_all_aux_tables(THD *thd, dict_table_t *table);

/** Drop FTS AUX table DD table objects in vector
@param[in]      aux_vec         aux table name vector
@param[in]      file_per_table  whether file per table
@return true on success, false on failure. */
bool fts_drop_dd_tables(const aux_name_vec_t *aux_vec, bool file_per_table);

/** Free FTS AUX table names in vector
@param[in]      aux_vec         aux table name vector
*/
void fts_free_aux_names(aux_name_vec_t *aux_vec);

/** The given transaction is about to be committed; do whatever is necessary
 from the FTS system's POV.
 @return DB_SUCCESS or error code */
[[nodiscard]] dberr_t fts_commit(trx_t *trx); /*!< in: transaction */

/** FTS Query entry point.
@param[in]      trx             transaction
@param[in]      index           fts index to search
@param[in]      flags           FTS search mode
@param[in]      query_str       FTS query
@param[in]      query_len       FTS query string len in bytes
@param[in,out]  result          result doc ids
@param[in]      limit           limit value
@return DB_SUCCESS if successful otherwise error code */
[[nodiscard]] dberr_t fts_query(trx_t *trx, dict_index_t *index, uint flags,
                                const byte *query_str, ulint query_len,
                                fts_result_t **result, ulonglong limit);

/** Retrieve the FTS Relevance Ranking result for doc with doc_id
 @return the relevance ranking value. */
float fts_retrieve_ranking(
    fts_result_t *result, /*!< in: FTS result structure */
    doc_id_t doc_id);     /*!< in: the interested document
                          doc_id */

/** FTS Query sort result, returned by fts_query() on fts_ranking_t::rank. */
void fts_query_sort_result_on_rank(fts_result_t *result); /*!< out: result
                                                          instance to sort.*/

/** FTS Query free result, returned by fts_query(). */
void fts_query_free_result(fts_result_t *result); /*!< in: result instance
                                                  to free.*/

/** Extract the doc id from the FTS hidden column. */
doc_id_t fts_get_doc_id_from_row(dict_table_t *table, /*!< in: table */
                                 dtuple_t *row); /*!< in: row whose FTS doc id
                                                 we want to extract.*/

/** Extract the doc id from the record that belongs to index.
@param[in]      table   table
@param[in]      rec     record contains FTS_DOC_ID
@param[in]      index   index of rec
@param[in]      heap    heap memory
@return doc id that was extracted from rec */
doc_id_t fts_get_doc_id_from_rec(dict_table_t *table, const rec_t *rec,
                                 const dict_index_t *index, mem_heap_t *heap);

/** Add new fts doc id to the update vector.
@param[in]      table           the table that contains the FTS index.
@param[in,out]  ufield          the fts doc id field in the update vector.
                                No new memory is allocated for this in this
                                function.
@param[in,out]  next_doc_id     the fts doc id that has been added to the
                                update vector.  If 0, a new fts doc id is
                                automatically generated.  The memory provided
                                for this argument will be used by the update
                                vector. Ensure that the life time of this
                                memory matches that of the update vector.
@return the fts doc id used in the update vector */
doc_id_t fts_update_doc_id(dict_table_t *table, upd_field_t *ufield,
                           doc_id_t *next_doc_id);

/** FTS initialize. */
void fts_startup(void);

#if 0  // TODO: Enable this in WL#6608
/******************************************************************//**
Signal FTS threads to initiate shutdown. */
void
fts_start_shutdown(
        dict_table_t*   table,                  /*!< in: table with FTS
                                                indexes */
        fts_t*          fts);                   /*!< in: fts instance to
                                                shutdown */

/******************************************************************//**
Wait for FTS threads to shutdown. */
void
fts_shutdown(
        dict_table_t*   table,                  /*!< in: table with FTS
                                                indexes */
        fts_t*          fts);                   /*!< in: fts instance to
                                                shutdown */
#endif

/** Create an instance of fts_t.
 @return instance of fts_t */
fts_t *fts_create(dict_table_t *table); /*!< out: table with FTS
                                        indexes */

/** Free the FTS resources. */
void fts_free(dict_table_t *table); /*!< in/out: table with
                                    FTS indexes */

/** Run OPTIMIZE on the given table.
 @return DB_SUCCESS if all OK */
dberr_t fts_optimize_table(dict_table_t *table); /*!< in: table to optimiza */

/** Startup the optimize thread and create the work queue. */
void fts_optimize_init(void);

/** Since we do a horizontal split on the index table, we need to drop
all the split tables.
@param[in]      trx             transaction
@param[in]      index           fts index
@param[out]     aux_vec         dropped table names vector
@return DB_SUCCESS or error code */
dberr_t fts_drop_index_tables(trx_t *trx, dict_index_t *index,
                              aux_name_vec_t *aux_vec);

/** Empty all common talbes.
@param[in,out]  trx     transaction
@param[in]      table   dict table
@return DB_SUCCESS or error code. */
dberr_t fts_empty_common_tables(trx_t *trx, dict_table_t *table);

/** Remove the table from the OPTIMIZER's list. We do wait for
 acknowledgement from the consumer of the message. */
void fts_optimize_remove_table(dict_table_t *table); /*!< in: table to remove */

/** Shutdown fts optimize thread. */
void fts_optimize_shutdown();

/** Send sync fts cache for the table.
@param[in]      table   table to sync */
void fts_optimize_request_sync_table(dict_table_t *table);

/** Take a FTS savepoint.
@param[in] fts_trx Fts transaction
@param[in] name Savepoint name */
void fts_savepoint_take(fts_trx_t *fts_trx, const char *name);

/** Refresh last statement savepoint. */
void fts_savepoint_laststmt_refresh(trx_t *trx); /*!< in: transaction */

/** Release the savepoint data identified by  name. */
void fts_savepoint_release(trx_t *trx,        /*!< in: transaction */
                           const char *name); /*!< in: savepoint name */

/** Clear cache.
@param[in,out]  cache   fts cache */
void fts_cache_clear(fts_cache_t *cache);

/** Initialize things in cache. */
void fts_cache_init(fts_cache_t *cache); /*!< in: cache */

/** Rollback to and including savepoint identified by name. */
void fts_savepoint_rollback(trx_t *trx,        /*!< in: transaction */
                            const char *name); /*!< in: savepoint name */

/** Rollback to and including savepoint identified by name. */
void fts_savepoint_rollback_last_stmt(trx_t *trx); /*!< in: transaction */

/* Get parent table name if it's a fts aux table
@param[in]      aux_table_name  aux table name
@param[in]      aux_table_len   aux table length
@return parent table name, or NULL */
char *fts_get_parent_table_name(const char *aux_table_name,
                                ulint aux_table_len);

/** Run SYNC on the table, i.e., write out data from the cache to the
FTS auxiliary INDEX table and clear the cache at the end.
@param[in,out]  table           fts table
@param[in]      unlock_cache    whether unlock cache when write node
@param[in]      wait            whether wait for existing sync to finish
@param[in]      has_dict        whether has dict operation lock
@return DB_SUCCESS on success, error code on failure. */
dberr_t fts_sync_table(dict_table_t *table, bool unlock_cache, bool wait,
                       bool has_dict);

/** Create an FTS index cache. */
CHARSET_INFO *fts_index_get_charset(dict_index_t *index); /*!< in: FTS index */

/** Get the initial Doc ID by consulting the CONFIG table
 @return initial Doc ID */
doc_id_t fts_init_doc_id(const dict_table_t *table); /*!< in: table */

/** Compare two character string according to their charset.
@param[in] cs Character set
@param[in] p1 Key
@param[in] p2 Node */
extern int innobase_fts_text_cmp(const void *cs, const void *p1,
                                 const void *p2);

/** Makes all characters in a string lower case.
@param[in] cs Character set
@param[in] src String to put in lower case
@param[in] src_len Input string length
@param[in] dst Buffer for result string
@param[in] dst_len Buffer size */
extern size_t innobase_fts_casedn_str(CHARSET_INFO *cs, char *src,
                                      size_t src_len, char *dst,
                                      size_t dst_len);

/** Compare two character string according to their charset.
@param[in] cs Character set
@param[in] p1 Key
@param[in] p2 Node */
extern int innobase_fts_text_cmp_prefix(const void *cs, const void *p1,
                                        const void *p2);

/** Get the next token from the given string and store it in *token. */
extern ulint innobase_mysql_fts_get_token(
    CHARSET_INFO *charset, /*!< in: Character set */
    const byte *start,     /*!< in: start of text */
    const byte *end,       /*!< in: one character past
                           end of text */
    fts_string_t *token);  /*!< out: token's text */

/** Drop dd table & tablespace for fts aux table
@param[in]      name            table name
@param[in]      file_per_table  flag whether use file per table
@return true on success, false on failure. */
bool innobase_fts_drop_dd_table(const char *name, bool file_per_table);

/** Get token char size by charset
 @return the number of token char size */
ulint fts_get_token_size(const CHARSET_INFO *cs, /*!< in: Character set */
                         const char *token,      /*!< in: token */
                         ulint len);             /*!< in: token length */

/** FULLTEXT tokenizer internal in MYSQL_FTPARSER_SIMPLE_MODE
 @return 0 if tokenize successfully */
int fts_tokenize_document_internal(
    MYSQL_FTPARSER_PARAM *param, /*!< in: parser parameter */
    char *doc,                   /*!< in: document to tokenize */
    int len);                    /*!< in: document length */

/** Fetch COUNT(*) from specified table.
 @return the number of rows in the table */
ulint fts_get_rows_count(fts_table_t *fts_table); /*!< in: fts table to read */

/** Get maximum Doc ID in a table if index "FTS_DOC_ID_INDEX" exists
 @return max Doc ID or 0 if index "FTS_DOC_ID_INDEX" does not exist */
doc_id_t fts_get_max_doc_id(dict_table_t *table); /*!< in: user table */

/** Check whether user supplied stopword table exists and is of
 the right format.
 @return the stopword column charset if qualifies */
CHARSET_INFO *fts_valid_stopword_table(
    const char *stopword_table_name); /*!< in: Stopword table
                                      name */
/** This function loads specified stopword into FTS cache
 @return true if success */
bool fts_load_stopword(
    const dict_table_t *table,          /*!< in: Table with FTS */
    trx_t *trx,                         /*!< in: Transaction */
    const char *global_stopword_table,  /*!< in: Global stopword table
                                        name */
    const char *session_stopword_table, /*!< in: Session stopword table
                                        name */
    bool stopword_is_on,                /*!< in: Whether stopword
                                         option is turned on/off */
    bool reload);                       /*!< in: Whether it is during
                                         reload of FTS table */

/** Read the rows from the FTS index
 @return DB_SUCCESS if OK */
dberr_t fts_table_fetch_doc_ids(trx_t *trx,              /*!< in: transaction */
                                fts_table_t *fts_table,  /*!< in: aux table */
                                fts_doc_ids_t *doc_ids); /*!< in: For collecting
                                                         doc ids */
/** This function brings FTS index in sync when FTS index is first
 used. There are documents that have not yet sync-ed to auxiliary
 tables from last server abnormally shutdown, we will need to bring
 such document into FTS cache before any further operations
 @return true if all OK */
bool fts_init_index(dict_table_t *table,  /*!< in: Table with FTS */
                    bool has_cache_lock); /*!< in: Whether we already
                                           have cache lock */
/** Add a newly create index in FTS cache */
void fts_add_index(dict_index_t *index,  /*!< FTS index to be added */
                   dict_table_t *table); /*!< table */

/** Drop auxiliary tables related to an FTS index
@param[in]      table           Table where indexes are dropped
@param[in]      index           Index to be dropped
@param[in]      trx             Transaction for the drop
@param[in,out]  aux_vec         Aux table name vector
@return DB_SUCCESS or error number */
dberr_t fts_drop_index(dict_table_t *table, dict_index_t *index, trx_t *trx,
                       aux_name_vec_t *aux_vec);

/** Rename auxiliary tables for all fts index for a table
 @return DB_SUCCESS or error code */
dberr_t fts_rename_aux_tables(dict_table_t *table,  /*!< in: user Table */
                              const char *new_name, /*!< in: new table name */
                              trx_t *trx,           /*!< in: transaction */
                              bool replay);         /*!< Whether in replay
                                                    stage */

/** Check indexes in the fts->indexes is also present in index cache and
 table->indexes list
 @return true if all indexes match */
bool fts_check_cached_index(
    dict_table_t *table); /*!< in: Table where indexes are dropped */

/** Fetch the document from tuple, tokenize the text data and
insert the text data into fts auxiliary table and
its cache. Moreover this tuple fields doesn't contain any information
about externally stored field. This tuple contains data directly
converted from mysql.
@param[in]      ftt     FTS transaction table
@param[in]      doc_id  doc id
@param[in]      tuple   tuple from where data can be retrieved
                        and tuple should be arranged in table
                        schema order. */
void fts_add_doc_from_tuple(fts_trx_table_t *ftt, doc_id_t doc_id,
                            const dtuple_t *tuple);

/** Create an FTS trx.
@param[in,out]  trx     InnoDB Transaction
@return FTS transaction. */
fts_trx_t *fts_trx_create(trx_t *trx);

/** For storing table info when checking for orphaned tables. */
struct fts_aux_table_t {
  /** Table id */
  table_id_t id;

  /** Parent table id */
  table_id_t parent_id;

  /** Table FT index id */
  table_id_t index_id;

  /** Name of the table */
  char *name;

  /** FTS table type */
  fts_table_type_t type;
};

/** Check if a table is an FTS auxiliary table name.
@param[out]     table   FTS table info
@param[in]      name    Table name
@param[in]      len     Length of table name
@return true if the name matches an auxiliary table name pattern */
bool fts_is_aux_table_name(fts_aux_table_t *table, const char *name, ulint len);

/** Freeze all auiliary tables to be not evictable if exist, with dict_mutex
held
@param[in]      table           InnoDB table object */
void fts_freeze_aux_tables(const dict_table_t *table);

/** Allow all the auxiliary tables of specified base table to be evictable
if they exist, if not exist just ignore
@param[in]      table           InnoDB table object
@param[in]      dict_locked     True if we have dict_sys mutex */
void fts_detach_aux_tables(const dict_table_t *table, bool dict_locked);

/** Update DD system table for auxiliary common tables for an FTS index.
@param[in]      table           dict table instance
@return true on success, false on failure */
bool fts_create_common_dd_tables(const dict_table_t *table);

/** Check if a table has FTS index needs to have its auxiliary index
tables' metadata updated in DD
@param[in,out]  table           table to check
@return DB_SUCCESS or error code */
dberr_t fts_create_index_dd_tables(dict_table_t *table);

/** Upgrade FTS AUX Tables. The FTS common and aux tables are
renamed because they have table_id in their name. We move table_ids
by DICT_MAX_DD_TABLES offset. Aux tables are registered into DD
after rename.
@param[in]      table           InnoDB table object
@return DB_SUCCESS or error code */
dberr_t fts_upgrade_aux_tables(dict_table_t *table);

/** Rename FTS AUX tablespace name from 8.0 format to 5.7 format.
This will be done on upgrade failure
@param[in]      table           parent table
@param[in]      rollback        rollback the rename from 8.0 to 5.7
                                if true, rename to 5.7 format
                                if false, mark the table as evictable
@return DB_SUCCESS on success, DB_ERROR on error */
dberr_t fts_upgrade_rename(const dict_table_t *table, bool rollback);

#endif
