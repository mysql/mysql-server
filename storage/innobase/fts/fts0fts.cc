/*****************************************************************************

Copyright (c) 2011, 2022, Oracle and/or its affiliates.

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

/** @file fts/fts0fts.cc
 Full Text Search interface
 ***********************************************************************/

#include <current_thd.h>
#include <sys/types.h>
#include <new>

#include "btr0pcur.h"
#include "dict0priv.h"
#include "dict0stats.h"
#include "dict0types.h"
#include "fts0fts.h"
#include "fts0plugin.h"
#include "fts0priv.h"
#include "fts0types.h"
#include "fts0types.ic"
#include "fts0vlc.ic"
#include "ha_prototypes.h"
#include "lob0lob.h"

#include "my_dbug.h"

#include "dict0dd.h"
#include "lob0lob.h"
#include "row0mysql.h"
#include "row0sel.h"
#include "row0upd.h"
#include "sync0sync.h"
#include "trx0roll.h"
#include "ut0new.h"

static const ulint FTS_MAX_ID_LEN = 32;

/** Column name from the FTS config table */
#define FTS_MAX_CACHE_SIZE_IN_MB "cache_size_in_mb"

/** Verify if a aux table name is a obsolete table
by looking up the key word in the obsolete table names */
#define FTS_IS_OBSOLETE_AUX_TABLE(table_name) \
  (strstr((table_name), "DOC_ID") != NULL ||  \
   strstr((table_name), "ADDED") != NULL ||   \
   strstr((table_name), "STOPWORDS") != NULL)

/** This is maximum FTS cache for each table and would be
a configurable variable */
ulong fts_max_cache_size;

/** Whether the total memory used for FTS cache is exhausted, and we will
need a sync to free some memory */
bool fts_need_sync = false;

/** Variable specifying the total memory allocated for FTS cache */
ulong fts_max_total_cache_size;

/** This is FTS result cache limit for each query and would be
a configurable variable */
ulong fts_result_cache_limit;

/** Variable specifying the maximum FTS max token size */
ulong fts_max_token_size;

/** Variable specifying the minimum FTS max token size */
ulong fts_min_token_size;

// FIXME: testing
static std::chrono::steady_clock::duration elapsed_time;
static ulint n_nodes = 0;

#ifdef FTS_CACHE_SIZE_DEBUG
/** The cache size permissible lower limit (1K) */
static const ulint FTS_CACHE_SIZE_LOWER_LIMIT_IN_MB = 1;

/** The cache size permissible upper limit (1G) */
static const ulint FTS_CACHE_SIZE_UPPER_LIMIT_IN_MB = 1024;
#endif

/** Time to sleep after DEADLOCK error before retrying operation in
milliseconds. */
static constexpr uint32_t FTS_DEADLOCK_RETRY_WAIT_MS = 100;

/** variable to record innodb_fts_internal_tbl_name for information
schema table INNODB_FTS_INSERTED etc. */
char *fts_internal_tbl_name = nullptr;

/** InnoDB default stopword list:
There are different versions of stopwords, the stop words listed
below comes from "Google Stopword" list. Reference:
http://meta.wikimedia.org/wiki/Stop_word_list/google_stop_word_list.
The final version of InnoDB default stopword list is still pending
for decision */
const char *fts_default_stopword[] = {
    "a",    "about", "an",  "are",  "as",   "at",    "be",   "by",
    "com",  "de",    "en",  "for",  "from", "how",   "i",    "in",
    "is",   "it",    "la",  "of",   "on",   "or",    "that", "the",
    "this", "to",    "was", "what", "when", "where", "who",  "will",
    "with", "und",   "the", "www",  nullptr};

/** FTS auxiliary table prefix that are common to all FT indexes.*/
const char *FTS_PREFIX = "fts_";

/** FTS auxiliary table prefix that are common to all FT indexes.*/
const char *FTS_PREFIX_5_7 = "FTS_";

/** FTS auxiliary table suffixes that are common to all FT indexes. */
const char *fts_common_tables[] = {"being_deleted", "being_deleted_cache",
                                   "config",        "deleted",
                                   "deleted_cache", nullptr};

const char *FTS_SUFFIX_BEING_DELETED = fts_common_tables[0];
const char *FTS_SUFFIX_BEING_DELETED_CACHE = fts_common_tables[1];
const char *FTS_SUFFIX_CONFIG = fts_common_tables[2];
const char *FTS_SUFFIX_DELETED = fts_common_tables[3];
const char *FTS_SUFFIX_DELETED_CACHE = fts_common_tables[4];

/** FTS auxiliary table suffixes that are common to all FT indexes. */
const char *fts_common_tables_5_7[] = {"BEING_DELETED", "BEING_DELETED_CACHE",
                                       "CONFIG",        "DELETED",
                                       "DELETED_CACHE", nullptr};

const char *FTS_SUFFIX_CONFIG_5_7 = fts_common_tables_5_7[2];

/** FTS auxiliary INDEX split intervals. */
const fts_index_selector_t fts_index_selector[] = {
    {9, "index_1"},  {65, "index_2"}, {70, "index_3"}, {75, "index_4"},
    {80, "index_5"}, {85, "index_6"}, {0, nullptr}};

/** FTS auxiliary INDEX split intervals. */
const fts_index_selector_t fts_index_selector_5_7[] = {
    {9, "INDEX_1"},  {65, "INDEX_2"}, {70, "INDEX_3"}, {75, "INDEX_4"},
    {80, "INDEX_5"}, {85, "INDEX_6"}, {0, nullptr}};

/** Default config values for FTS indexes on a table. */
static const char *fts_config_table_insert_values_sql =
    "BEGIN\n"
    "\n"
    "INSERT INTO $config_table VALUES('" FTS_MAX_CACHE_SIZE_IN_MB
    "', '256');\n"
    ""
    "INSERT INTO $config_table VALUES('" FTS_OPTIMIZE_LIMIT_IN_SECS
    "', '180');\n"
    ""
    "INSERT INTO $config_table VALUES ('" FTS_SYNCED_DOC_ID
    "', '0');\n"
    ""
    "INSERT INTO $config_table VALUES ('" FTS_TOTAL_DELETED_COUNT
    "', '0');\n"
    "" /* Note: 0 == FTS_TABLE_STATE_RUNNING */
    "INSERT INTO $config_table VALUES ('" FTS_TABLE_STATE "', '0');\n";

/** FTS tokenize parameter for plugin parser */
struct fts_tokenize_param_t {
  fts_doc_t *result_doc; /*!< Result doc for tokens */
  ulint add_pos;         /*!< Added position for tokens */
};

/** Run SYNC on the table, i.e., write out data from the cache to the
FTS auxiliary INDEX table and clear the cache at the end.
@param[in,out]  sync            sync state
@param[in]      unlock_cache    whether unlock cache lock when write node
@param[in]      wait            whether wait when a sync is in progress
@param[in]      has_dict        whether has dict operation lock
@return DB_SUCCESS if all OK */
static dberr_t fts_sync(fts_sync_t *sync, bool unlock_cache, bool wait,
                        bool has_dict);

/** Release all resources help by the words rb tree e.g., the node ilist. */
static void fts_words_free(ib_rbt_t *words); /*!< in: rb tree of words */
#ifdef FTS_CACHE_SIZE_DEBUG
/** Read the max cache size parameter from the config table. */
static void fts_update_max_cache_size(fts_sync_t *sync); /*!< in: sync state */
#endif

/** This function fetches the document just inserted right before
we commit the transaction, and tokenize the inserted text data
and insert into FTS auxiliary table and its cache.
@param[in]      ftt             FTS transaction table
@param[in]      doc_id          doc id
@param[in]      fts_indexes     affected FTS indexes
@return true if successful */
static ulint fts_add_doc_by_id(fts_trx_table_t *ftt, doc_id_t doc_id,
                               ib_vector_t *fts_indexes);

/** Update the last document id. This function could create a new
 transaction to update the last document id.
 @return DB_SUCCESS if OK */
static dberr_t fts_update_sync_doc_id(
    const dict_table_t *table, /*!< in: table */
    const char *table_name,    /*!< in: table name, or NULL */
    doc_id_t doc_id,           /*!< in: last document id */
    trx_t *trx);               /*!< in: update trx, or NULL */

/** Tokenize a document.
@param[in,out]  doc     document to tokenize
@param[out]     result  tokenization result
@param[in]      parser  pluggable parser */
static void fts_tokenize_document(fts_doc_t *doc, fts_doc_t *result,
                                  st_mysql_ftparser *parser);

/** Continue to tokenize a document.
@param[in,out]  doc     document to tokenize
@param[in]      add_pos add this position to all tokens from this tokenization
@param[out]     result  tokenization result
@param[in]      parser  pluggable parser */
static void fts_tokenize_document_next(fts_doc_t *doc, ulint add_pos,
                                       fts_doc_t *result,
                                       st_mysql_ftparser *parser);

/** Create the vector of fts_get_doc_t instances.
@param[in,out]  cache   fts cache
@return vector of fts_get_doc_t instances */
static ib_vector_t *fts_get_docs_create(fts_cache_t *cache);

/** Free the FTS cache.
@param[in,out]  cache to be freed */
static void fts_cache_destroy(fts_cache_t *cache) {
  rw_lock_free(&cache->lock);
  rw_lock_free(&cache->init_lock);
  mutex_free(&cache->optimize_lock);
  mutex_free(&cache->deleted_lock);
  mutex_free(&cache->doc_id_lock);
  os_event_destroy(cache->sync->event);

  if (cache->stopword_info.cached_stopword) {
    rbt_free(cache->stopword_info.cached_stopword);
  }

  if (cache->sync_heap->arg) {
    mem_heap_free(static_cast<mem_heap_t *>(cache->sync_heap->arg));
  }

  mem_heap_free(cache->cache_heap);
}

/** Get a character set based on precise type.
@param prtype precise type
@return the corresponding character set */
static inline CHARSET_INFO *fts_get_charset(ulint prtype) {
#ifdef UNIV_DEBUG
  switch (prtype & DATA_MYSQL_TYPE_MASK) {
    case MYSQL_TYPE_BIT:
    case MYSQL_TYPE_STRING:
    case MYSQL_TYPE_VAR_STRING:
    case MYSQL_TYPE_TINY_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_BLOB:
    case MYSQL_TYPE_LONG_BLOB:
    case MYSQL_TYPE_VARCHAR:
      break;
    default:
      ut_error;
  }
#endif /* UNIV_DEBUG */

  uint cs_num = (uint)dtype_get_charset_coll(prtype);

  if (CHARSET_INFO *cs = get_charset(cs_num, MYF(MY_WME))) {
    return (cs);
  }

  ib::fatal(UT_LOCATION_HERE, ER_IB_MSG_461)
      << "Unable to find charset-collation " << cs_num;
  return (nullptr);
}

/** This function loads the default InnoDB stopword list */
static void fts_load_default_stopword(
    fts_stopword_t *stopword_info) /*!< in: stopword info */
{
  fts_string_t str;
  mem_heap_t *heap;
  ib_alloc_t *allocator;
  ib_rbt_t *stop_words;

  allocator = stopword_info->heap;
  heap = static_cast<mem_heap_t *>(allocator->arg);

  if (!stopword_info->cached_stopword) {
    stopword_info->cached_stopword =
        rbt_create_arg_cmp(sizeof(fts_tokenizer_word_t), innobase_fts_text_cmp,
                           &my_charset_latin1);
  }

  stop_words = stopword_info->cached_stopword;

  str.f_n_char = 0;

  for (ulint i = 0; fts_default_stopword[i]; ++i) {
    char *word;
    fts_tokenizer_word_t new_word;

    /* We are going to duplicate the value below. */
    word = const_cast<char *>(fts_default_stopword[i]);

    new_word.nodes = ib_vector_create(allocator, sizeof(fts_node_t), 4);

    str.f_len = ut_strlen(word);
    str.f_str = reinterpret_cast<byte *>(word);

    fts_string_dup(&new_word.text, &str, heap);

    rbt_insert(stop_words, &new_word, &new_word);
  }

  stopword_info->status = STOPWORD_FROM_DEFAULT;
}

/** Callback function to read a single stopword value.
 @return Always return true */
static bool fts_read_stopword(void *row,      /*!< in: sel_node_t* */
                              void *user_arg) /*!< in: pointer to ib_vector_t */
{
  ib_alloc_t *allocator;
  fts_stopword_t *stopword_info;
  sel_node_t *sel_node;
  que_node_t *exp;
  ib_rbt_t *stop_words;
  dfield_t *dfield;
  fts_string_t str;
  mem_heap_t *heap;
  ib_rbt_bound_t parent;

  sel_node = static_cast<sel_node_t *>(row);
  stopword_info = static_cast<fts_stopword_t *>(user_arg);

  stop_words = stopword_info->cached_stopword;
  allocator = static_cast<ib_alloc_t *>(stopword_info->heap);
  heap = static_cast<mem_heap_t *>(allocator->arg);

  exp = sel_node->select_list;

  /* We only need to read the first column */
  dfield = que_node_get_val(exp);

  str.f_n_char = 0;
  str.f_str = static_cast<byte *>(dfield_get_data(dfield));
  str.f_len = dfield_get_len(dfield);

  /* Only create new node if it is a value not already existed */
  if (str.f_len != UNIV_SQL_NULL &&
      rbt_search(stop_words, &parent, &str) != 0) {
    fts_tokenizer_word_t new_word;

    new_word.nodes = ib_vector_create(allocator, sizeof(fts_node_t), 4);

    new_word.text.f_str =
        static_cast<byte *>(mem_heap_alloc(heap, str.f_len + 1));

    memcpy(new_word.text.f_str, str.f_str, str.f_len);

    new_word.text.f_n_char = 0;
    new_word.text.f_len = str.f_len;
    new_word.text.f_str[str.f_len] = 0;

    rbt_insert(stop_words, &new_word, &new_word);
  }

  return true;
}

/** Load user defined stopword from designated user table
 @return true if load operation is successful */
static bool fts_load_user_stopword(
    const char *stopword_table_name, /*!< in: Stopword table
                                     name */
    fts_stopword_t *stopword_info)   /*!< in: Stopword info */
{
  pars_info_t *info;
  que_t *graph;
  dberr_t error = DB_SUCCESS;
  bool ret = true;
  trx_t *trx;

  trx = trx_allocate_for_background();
  trx->op_info = "Load user stopword table into FTS cache";

  /* Validate the user table existence and in the right
  format */
  stopword_info->charset = fts_valid_stopword_table(stopword_table_name);
  if (!stopword_info->charset) {
    ret = false;
    goto cleanup;
  } else if (!stopword_info->cached_stopword) {
    /* Create the stopword RB tree with the stopword column
    charset. All comparison will use this charset */
    stopword_info->cached_stopword =
        rbt_create_arg_cmp(sizeof(fts_tokenizer_word_t), innobase_fts_text_cmp,
                           stopword_info->charset);
  }

  info = pars_info_create();

  pars_info_bind_id(info, true, "table_stopword", stopword_table_name);

  pars_info_bind_function(info, "my_func", fts_read_stopword, stopword_info);

  graph = fts_parse_sql(nullptr, info,
                        "DECLARE FUNCTION my_func;\n"
                        "DECLARE CURSOR c IS"
                        " SELECT value"
                        " FROM $table_stopword;\n"
                        "BEGIN\n"
                        "\n"
                        "OPEN c;\n"
                        "WHILE 1 = 1 LOOP\n"
                        "  FETCH c INTO my_func();\n"
                        "  IF c % NOTFOUND THEN\n"
                        "    EXIT;\n"
                        "  END IF;\n"
                        "END LOOP;\n"
                        "CLOSE c;");

  for (;;) {
    error = fts_eval_sql(trx, graph);

    if (error == DB_SUCCESS) {
      fts_sql_commit(trx);
      stopword_info->status = STOPWORD_USER_TABLE;
      break;
    } else {
      fts_sql_rollback(trx);

      if (error == DB_LOCK_WAIT_TIMEOUT) {
        ib::warn(ER_IB_MSG_462) << "Lock wait timeout reading user"
                                   " stopword table. Retrying!";

        trx->error_state = DB_SUCCESS;
      } else {
        ib::error(ER_IB_MSG_463) << "Error '" << ut_strerr(error)
                                 << "' while reading user stopword"
                                    " table.";
        ret = false;
        break;
      }
    }
  }

  que_graph_free(graph);

cleanup:
  trx_free_for_background(trx);
  return (ret);
}

/** Initialize the index cache. */
static void fts_index_cache_init(
    ib_alloc_t *allocator,          /*!< in: the allocator to use */
    fts_index_cache_t *index_cache) /*!< in: index cache */
{
  ulint i;

  ut_a(index_cache->words == nullptr);

  index_cache->words =
      rbt_create_arg_cmp(sizeof(fts_tokenizer_word_t), innobase_fts_text_cmp,
                         index_cache->charset);

  ut_a(index_cache->doc_stats == nullptr);

  index_cache->doc_stats =
      ib_vector_create(allocator, sizeof(fts_doc_stats_t), 4);

  for (i = 0; i < FTS_NUM_AUX_INDEX; ++i) {
    ut_a(index_cache->ins_graph[i] == nullptr);
    ut_a(index_cache->sel_graph[i] == nullptr);
  }
}

/** Initialize FTS cache. */
void fts_cache_init(fts_cache_t *cache) /*!< in: cache to initialize */
{
  ulint i;

  /* Just to make sure */
  ut_a(cache->sync_heap->arg == nullptr);

  cache->sync_heap->arg = mem_heap_create(1024, UT_LOCATION_HERE);

  cache->total_size = 0;
  cache->total_size_before_sync = 0;

  mutex_enter((ib_mutex_t *)&cache->deleted_lock);
  cache->deleted_doc_ids =
      ib_vector_create(cache->sync_heap, sizeof(fts_update_t), 4);
  mutex_exit((ib_mutex_t *)&cache->deleted_lock);

  /* Reset the cache data for all the FTS indexes. */
  for (i = 0; i < ib_vector_size(cache->indexes); ++i) {
    fts_index_cache_t *index_cache;

    index_cache =
        static_cast<fts_index_cache_t *>(ib_vector_get(cache->indexes, i));

    fts_index_cache_init(cache->sync_heap, index_cache);
  }
}

/** Create a FTS cache. */
fts_cache_t *fts_cache_create(
    dict_table_t *table) /*!< in: table owns the FTS cache */
{
  mem_heap_t *heap;
  fts_cache_t *cache;

  heap = static_cast<mem_heap_t *>(mem_heap_create(512, UT_LOCATION_HERE));

  cache = static_cast<fts_cache_t *>(mem_heap_zalloc(heap, sizeof(*cache)));

  cache->cache_heap = heap;

  rw_lock_create(fts_cache_rw_lock_key, &cache->lock, LATCH_ID_FTS_CACHE);

  rw_lock_create(fts_cache_init_rw_lock_key, &cache->init_lock,
                 LATCH_ID_FTS_CACHE_INIT);

  mutex_create(LATCH_ID_FTS_DELETE, &cache->deleted_lock);

  mutex_create(LATCH_ID_FTS_OPTIMIZE, &cache->optimize_lock);

  mutex_create(LATCH_ID_FTS_DOC_ID, &cache->doc_id_lock);

  /* This is the heap used to create the cache itself. */
  cache->self_heap = ib_heap_allocator_create(heap);

  /* This is a transient heap, used for storing sync data. */
  cache->sync_heap = ib_heap_allocator_create(heap);
  cache->sync_heap->arg = nullptr;

  cache->sync =
      static_cast<fts_sync_t *>(mem_heap_zalloc(heap, sizeof(fts_sync_t)));

  cache->sync->table = table;
  cache->sync->event = os_event_create();

  /* Create the index cache vector that will hold the inverted indexes. */
  cache->indexes =
      ib_vector_create(cache->self_heap, sizeof(fts_index_cache_t), 2);

  fts_cache_init(cache);

  cache->stopword_info.cached_stopword = nullptr;
  cache->stopword_info.charset = nullptr;

  cache->stopword_info.heap = cache->self_heap;

  cache->stopword_info.status = STOPWORD_NOT_INIT;

  return (cache);
}

/** Add a newly create index into FTS cache */
void fts_add_index(dict_index_t *index, /*!< FTS index to be added */
                   dict_table_t *table) /*!< table */
{
  fts_t *fts = table->fts;
  fts_cache_t *cache;
  fts_index_cache_t *index_cache;

  ut_ad(fts);
  cache = table->fts->cache;

  rw_lock_x_lock(&cache->init_lock, UT_LOCATION_HERE);

  ib_vector_push(fts->indexes, &index);

  index_cache = fts_find_index_cache(cache, index);

  if (!index_cache) {
    /* Add new index cache structure */
    index_cache = fts_cache_index_cache_create(table, index);
  }

  rw_lock_x_unlock(&cache->init_lock);
}

/** recalibrate get_doc structure after index_cache in cache->indexes changed */
static void fts_reset_get_doc(fts_cache_t *cache) /*!< in: FTS index cache */
{
  fts_get_doc_t *get_doc;
  ulint i;

  ut_ad(rw_lock_own(&cache->init_lock, RW_LOCK_X));

  ib_vector_reset(cache->get_docs);

  for (i = 0; i < ib_vector_size(cache->indexes); i++) {
    fts_index_cache_t *ind_cache;

    ind_cache =
        static_cast<fts_index_cache_t *>(ib_vector_get(cache->indexes, i));

    get_doc =
        static_cast<fts_get_doc_t *>(ib_vector_push(cache->get_docs, nullptr));

    memset(get_doc, 0x0, sizeof(*get_doc));

    get_doc->index_cache = ind_cache;
  }

  ut_ad(ib_vector_size(cache->get_docs) == ib_vector_size(cache->indexes));
}

/** Check an index is in the table->indexes list
 @return true if it exists */
static bool fts_in_dict_index(
    dict_table_t *table,       /*!< in: Table */
    dict_index_t *index_check) /*!< in: index to be checked */
{
  dict_index_t *index;

  for (index = table->first_index(); index != nullptr; index = index->next()) {
    if (index == index_check) {
      return true;
    }
  }

  return false;
}

/** Check an index is in the fts->cache->indexes list
 @return true if it exists */
static bool fts_in_index_cache(
    dict_table_t *table, /*!< in: Table */
    dict_index_t *index) /*!< in: index to be checked */
{
  ulint i;

  for (i = 0; i < ib_vector_size(table->fts->cache->indexes); i++) {
    fts_index_cache_t *index_cache;

    index_cache = static_cast<fts_index_cache_t *>(
        ib_vector_get(table->fts->cache->indexes, i));

    if (index_cache->index == index) {
      return true;
    }
  }

  return false;
}

/** Check indexes in the fts->indexes is also present in index cache and
 table->indexes list
 @return true if all indexes match */
bool fts_check_cached_index(
    dict_table_t *table) /*!< in: Table where indexes are dropped */
{
  ulint i;

  if (!table->fts || !table->fts->cache) {
    return true;
  }

  ut_a(ib_vector_size(table->fts->indexes) ==
       ib_vector_size(table->fts->cache->indexes));

  for (i = 0; i < ib_vector_size(table->fts->indexes); i++) {
    dict_index_t *index;

    index = static_cast<dict_index_t *>(ib_vector_getp(table->fts->indexes, i));

    if (!fts_in_index_cache(table, index)) {
      return false;
    }

    if (!fts_in_dict_index(table, index)) {
      return false;
    }
  }

  return true;
}

/** Drop auxiliary tables related to an FTS index
@param[in]      table           Table where indexes are dropped
@param[in]      index           Index to be dropped
@param[in]      trx             Transaction for the drop
@param[in,out]  aux_vec         Aux table name vector
@return DB_SUCCESS or error number */
dberr_t fts_drop_index(dict_table_t *table, dict_index_t *index, trx_t *trx,
                       aux_name_vec_t *aux_vec) {
  ib_vector_t *indexes = table->fts->indexes;
  dberr_t err = DB_SUCCESS;

  ut_a(indexes);

  if ((ib_vector_size(indexes) == 1 &&
       (index ==
        static_cast<dict_index_t *>(ib_vector_getp(table->fts->indexes, 0)))) ||
      ib_vector_is_empty(indexes)) {
    doc_id_t current_doc_id;
    doc_id_t first_doc_id;

    /* If we are dropping the only FTS index of the table,
    remove it from optimize thread */
    fts_optimize_remove_table(table);

    DICT_TF2_FLAG_UNSET(table, DICT_TF2_FTS);

    /* If Doc ID column is not added internally by FTS index,
    we can drop all FTS auxiliary tables. Otherwise, we will
    need to keep some common table such as CONFIG table, so
    as to keep track of incrementing Doc IDs */
    if (!DICT_TF2_FLAG_IS_SET(table, DICT_TF2_FTS_HAS_DOC_ID)) {
      err = fts_drop_tables(trx, table, aux_vec);

      fts_free(table);

      return (err);
    } else {
      if (!(index->type & DICT_CORRUPT)) {
        err = fts_empty_common_tables(trx, table);
        ut_ad(err == DB_SUCCESS);
      }
    }

    current_doc_id = table->fts->cache->next_doc_id;
    first_doc_id = table->fts->cache->first_doc_id;
    fts_cache_clear(table->fts->cache);
    fts_cache_destroy(table->fts->cache);
    table->fts->cache = fts_cache_create(table);
    table->fts->cache->next_doc_id = current_doc_id;
    table->fts->cache->first_doc_id = first_doc_id;
  } else {
    fts_cache_t *cache = table->fts->cache;
    fts_index_cache_t *index_cache;

    rw_lock_x_lock(&cache->init_lock, UT_LOCATION_HERE);

    index_cache = fts_find_index_cache(cache, index);

    if (index_cache != nullptr) {
      if (index_cache->words) {
        fts_words_free(index_cache->words);
        rbt_free(index_cache->words);
      }

      ib_vector_remove(cache->indexes, *(void **)index_cache);
    }

    if (cache->get_docs) {
      fts_reset_get_doc(cache);
    }

    rw_lock_x_unlock(&cache->init_lock);
  }

  err = fts_drop_index_tables(trx, index, aux_vec);

  ib_vector_remove(indexes, (const void *)index);

  return (err);
}

/** Create an FTS index cache. */
CHARSET_INFO *fts_index_get_charset(dict_index_t *index) /*!< in: FTS index */
{
  CHARSET_INFO *charset = nullptr;
  dict_field_t *field;
  ulint prtype;

  field = index->get_field(0);
  prtype = field->col->prtype;

  charset = fts_get_charset(prtype);

#ifdef FTS_DEBUG
  /* Set up charset info for this index. Please note all
  field of the FTS index should have the same charset */
  for (i = 1; i < index->n_fields; i++) {
    CHARSET_INFO *fld_charset;

    field = index->get_field(i);
    prtype = field->col->prtype;

    fld_charset = fts_get_charset(prtype);

    /* All FTS columns should have the same charset */
    if (charset) {
      ut_a(charset == fld_charset);
    } else {
      charset = fld_charset;
    }
  }
#endif

  return (charset);
}
/** Create an FTS index cache.
 @return Index Cache */
fts_index_cache_t *fts_cache_index_cache_create(
    dict_table_t *table, /*!< in: table with FTS index */
    dict_index_t *index) /*!< in: FTS index */
{
  ulint n_bytes;
  fts_index_cache_t *index_cache;
  fts_cache_t *cache = table->fts->cache;

  ut_a(cache != nullptr);

  ut_ad(rw_lock_own(&cache->init_lock, RW_LOCK_X));

  /* Must not already exist in the cache vector. */
  ut_a(fts_find_index_cache(cache, index) == nullptr);

  index_cache =
      static_cast<fts_index_cache_t *>(ib_vector_push(cache->indexes, nullptr));

  memset(index_cache, 0x0, sizeof(*index_cache));

  index_cache->index = index;

  index_cache->charset = fts_index_get_charset(index);

  n_bytes = sizeof(que_t *) * FTS_NUM_AUX_INDEX;

  index_cache->ins_graph = static_cast<que_t **>(mem_heap_zalloc(
      static_cast<mem_heap_t *>(cache->self_heap->arg), n_bytes));

  index_cache->sel_graph = static_cast<que_t **>(mem_heap_zalloc(
      static_cast<mem_heap_t *>(cache->self_heap->arg), n_bytes));

  fts_index_cache_init(cache->sync_heap, index_cache);

  if (cache->get_docs) {
    fts_reset_get_doc(cache);
  }

  return (index_cache);
}

/** Remove a FTS index cache
@param[in]      table   table with FTS index
@param[in]      index   FTS index */
void fts_cache_index_cache_remove(dict_table_t *table, dict_index_t *index) {
  ut_ad(table->fts != nullptr);
  ut_ad(index->type & DICT_FTS);

  fts_index_cache_t *index_cache;

  rw_lock_x_lock(&table->fts->cache->init_lock, UT_LOCATION_HERE);

  index_cache = static_cast<fts_index_cache_t *>(
      fts_find_index_cache(table->fts->cache, index));

  if (index_cache->words != nullptr) {
    rbt_free(index_cache->words);
    index_cache->words = nullptr;
  }

  ib_vector_remove(table->fts->cache->indexes,
                   *reinterpret_cast<void **>(index_cache));

  rw_lock_x_unlock(&table->fts->cache->init_lock);
}

/** Release all resources help by the words rb tree e.g., the node ilist. */
static void fts_words_free(ib_rbt_t *words) /*!< in: rb tree of words */
{
  const ib_rbt_node_t *rbt_node;

  /* Free the resources held by a word. */
  for (rbt_node = rbt_first(words); rbt_node != nullptr;
       rbt_node = rbt_first(words)) {
    ulint i;
    fts_tokenizer_word_t *word;

    word = rbt_value(fts_tokenizer_word_t, rbt_node);

    /* Free the ilists of this word. */
    for (i = 0; i < ib_vector_size(word->nodes); ++i) {
      fts_node_t *fts_node =
          static_cast<fts_node_t *>(ib_vector_get(word->nodes, i));

      ut::free(fts_node->ilist);
      fts_node->ilist = nullptr;
    }

    /* NOTE: We are responsible for free'ing the node */
    ut::free(rbt_remove_node(words, rbt_node));
  }
}

/** Clear cache.
@param[in,out]  cache   fts cache */
void fts_cache_clear(fts_cache_t *cache) {
  ulint i;

  for (i = 0; i < ib_vector_size(cache->indexes); ++i) {
    ulint j;
    fts_index_cache_t *index_cache;

    index_cache =
        static_cast<fts_index_cache_t *>(ib_vector_get(cache->indexes, i));

    fts_words_free(index_cache->words);

    rbt_free(index_cache->words);

    index_cache->words = nullptr;

    for (j = 0; j < FTS_NUM_AUX_INDEX; ++j) {
      if (index_cache->ins_graph[j] != nullptr) {
        que_graph_free(index_cache->ins_graph[j]);

        index_cache->ins_graph[j] = nullptr;
      }

      if (index_cache->sel_graph[j] != nullptr) {
        que_graph_free(index_cache->sel_graph[j]);

        index_cache->sel_graph[j] = nullptr;
      }
    }

    index_cache->doc_stats = nullptr;
  }

  mem_heap_free(static_cast<mem_heap_t *>(cache->sync_heap->arg));
  cache->sync_heap->arg = nullptr;

  fts_need_sync = false;

  cache->total_size = 0;

  mutex_enter((ib_mutex_t *)&cache->deleted_lock);
  cache->deleted_doc_ids = nullptr;
  mutex_exit((ib_mutex_t *)&cache->deleted_lock);
}

/** Search the index specific cache for a particular FTS index.
 @return the index cache else NULL */
static inline fts_index_cache_t *fts_get_index_cache(
    fts_cache_t *cache,        /*!< in: cache to search */
    const dict_index_t *index) /*!< in: index to search for */
{
  ulint i;

  ut_ad(rw_lock_own((rw_lock_t *)&cache->lock, RW_LOCK_X) ||
        rw_lock_own((rw_lock_t *)&cache->init_lock, RW_LOCK_X));

  for (i = 0; i < ib_vector_size(cache->indexes); ++i) {
    fts_index_cache_t *index_cache;

    index_cache =
        static_cast<fts_index_cache_t *>(ib_vector_get(cache->indexes, i));

    if (index_cache->index == index) {
      return (index_cache);
    }
  }

  return (nullptr);
}

#ifdef FTS_DEBUG
/** Search the index cache for a get_doc structure.
 @return the fts_get_doc_t item else NULL */
static fts_get_doc_t *fts_get_index_get_doc(
    fts_cache_t *cache,        /*!< in: cache to search */
    const dict_index_t *index) /*!< in: index to search for */
{
  ulint i;

  ut_ad(rw_lock_own((rw_lock_t *)&cache->init_lock, RW_LOCK_X));

  for (i = 0; i < ib_vector_size(cache->get_docs); ++i) {
    fts_get_doc_t *get_doc;

    get_doc = static_cast<fts_get_doc_t *>(ib_vector_get(cache->get_docs, i));

    if (get_doc->index_cache->index == index) {
      return (get_doc);
    }
  }

  return (NULL);
}
#endif

/** Find an existing word, or if not found, create one and return it.
 @return specified word token */
static fts_tokenizer_word_t *fts_tokenizer_word_get(
    fts_cache_t *cache,             /*!< in: cache */
    fts_index_cache_t *index_cache, /*!< in: index cache */
    fts_string_t *text)             /*!< in: node text */
{
  fts_tokenizer_word_t *word;
  ib_rbt_bound_t parent;

  ut_ad(rw_lock_own(&cache->lock, RW_LOCK_X));

  /* If it is a stopword, do not index it */
  if (!fts_check_token(text, cache->stopword_info.cached_stopword,
                       index_cache->index->is_ngram, index_cache->charset)) {
    return (nullptr);
  }

  /* Check if we found a match, if not then add word to tree. */
  if (rbt_search(index_cache->words, &parent, text) != 0) {
    mem_heap_t *heap;
    fts_tokenizer_word_t new_word;

    heap = static_cast<mem_heap_t *>(cache->sync_heap->arg);

    new_word.nodes = ib_vector_create(cache->sync_heap, sizeof(fts_node_t), 4);

    fts_string_dup(&new_word.text, text, heap);

    parent.last = rbt_add_node(index_cache->words, &parent, &new_word);

    /* Take into account the RB tree memory use and the vector. */
    cache->total_size += sizeof(new_word) + sizeof(ib_rbt_node_t) +
                         text->f_len + (sizeof(fts_node_t) * 4) +
                         sizeof(*new_word.nodes);

    ut_ad(rbt_validate(index_cache->words));
  }

  word = rbt_value(fts_tokenizer_word_t, parent.last);

  return (word);
}

/** Add the given doc_id/word positions to the given node's ilist. */
void fts_cache_node_add_positions(
    fts_cache_t *cache,     /*!< in: cache */
    fts_node_t *node,       /*!< in: word node */
    doc_id_t doc_id,        /*!< in: doc id */
    ib_vector_t *positions) /*!< in: fts_token_t::positions */
{
  ulint i;
  byte *ptr;
  byte *ilist;
  ulint enc_len;
  ulint last_pos;
  byte *ptr_start;
  ulint doc_id_delta;

#ifdef UNIV_DEBUG
  if (cache) {
    ut_ad(rw_lock_own(&cache->lock, RW_LOCK_X));
  }
#endif /* UNIV_DEBUG */

  ut_ad(doc_id >= node->last_doc_id);

  /* Calculate the space required to store the ilist. */
  doc_id_delta = (ulint)(doc_id - node->last_doc_id);
  enc_len = fts_get_encoded_len(doc_id_delta);

  last_pos = 0;
  for (i = 0; i < ib_vector_size(positions); i++) {
    ulint pos = *(static_cast<ulint *>(ib_vector_get(positions, i)));

    ut_ad(last_pos == 0 || pos > last_pos);

    enc_len += fts_get_encoded_len(pos - last_pos);
    last_pos = pos;
  }

  /* The 0x00 byte at the end of the token positions list. */
  enc_len++;

  if ((node->ilist_size_alloc - node->ilist_size) >= enc_len) {
    /* No need to allocate more space, we can fit in the new
    data at the end of the old one. */
    ilist = nullptr;
    ptr = node->ilist + node->ilist_size;
  } else {
    ulint new_size = node->ilist_size + enc_len;

    /* Over-reserve space by a fixed size for small lengths and
    by 20% for lengths >= 48 bytes. */
    if (new_size < 16) {
      new_size = 16;
    } else if (new_size < 32) {
      new_size = 32;
    } else if (new_size < 48) {
      new_size = 48;
    } else {
      new_size = (ulint)(1.2 * new_size);
    }

    ilist = static_cast<byte *>(
        ut::malloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, new_size));
    ptr = ilist + node->ilist_size;

    node->ilist_size_alloc = new_size;
    if (cache) {
      cache->total_size += new_size;
    }
  }

  ptr_start = ptr;

  /* Encode the new fragment. */
  ptr += fts_encode_int(doc_id_delta, ptr);

  last_pos = 0;
  for (i = 0; i < ib_vector_size(positions); i++) {
    ulint pos = *(static_cast<ulint *>(ib_vector_get(positions, i)));

    ptr += fts_encode_int(pos - last_pos, ptr);
    last_pos = pos;
  }

  *ptr++ = 0;

  ut_a(enc_len == (ulint)(ptr - ptr_start));

  if (ilist) {
    /* Copy old ilist to the start of the new one and switch the
    new one into place in the node. */
    if (node->ilist_size > 0) {
      memcpy(ilist, node->ilist, node->ilist_size);
      ut::free(node->ilist);
      if (cache) {
        cache->total_size -= node->ilist_size;
      }
    }

    node->ilist = ilist;
  }

  node->ilist_size += enc_len;

  if (node->first_doc_id == FTS_NULL_DOC_ID) {
    node->first_doc_id = doc_id;
  }

  node->last_doc_id = doc_id;
  ++node->doc_count;
}

/** Add document to the cache. */
static void fts_cache_add_doc(
    fts_cache_t *cache,             /*!< in: cache */
    fts_index_cache_t *index_cache, /*!< in: index cache */
    doc_id_t doc_id,                /*!< in: doc id to add */
    ib_rbt_t *tokens)               /*!< in: document tokens */
{
  const ib_rbt_node_t *node;
  ulint n_words;
  fts_doc_stats_t *doc_stats;

  if (!tokens) {
    return;
  }

  ut_ad(rw_lock_own(&cache->lock, RW_LOCK_X));

  n_words = rbt_size(tokens);

  for (node = rbt_first(tokens); node; node = rbt_first(tokens)) {
    fts_tokenizer_word_t *word;
    fts_node_t *fts_node = nullptr;
    fts_token_t *token = rbt_value(fts_token_t, node);

    /* Find and/or add token to the cache. */
    word = fts_tokenizer_word_get(cache, index_cache, &token->text);

    if (!word) {
      ut::free(rbt_remove_node(tokens, node));
      continue;
    }

    if (ib_vector_size(word->nodes) > 0) {
      fts_node = static_cast<fts_node_t *>(ib_vector_last(word->nodes));
    }

    if (fts_node == nullptr || fts_node->synced ||
        fts_node->ilist_size > FTS_ILIST_MAX_SIZE ||
        doc_id < fts_node->last_doc_id) {
      fts_node =
          static_cast<fts_node_t *>(ib_vector_push(word->nodes, nullptr));

      memset(fts_node, 0x0, sizeof(*fts_node));

      cache->total_size += sizeof(*fts_node);
    }

    fts_cache_node_add_positions(cache, fts_node, doc_id, token->positions);

    ut::free(rbt_remove_node(tokens, node));
  }

  ut_a(rbt_empty(tokens));

  /* Add to doc ids processed so far. */
  doc_stats = static_cast<fts_doc_stats_t *>(
      ib_vector_push(index_cache->doc_stats, nullptr));

  doc_stats->doc_id = doc_id;
  doc_stats->word_count = n_words;

  /* Add the doc stats memory usage too. */
  cache->total_size += sizeof(*doc_stats);

  if (doc_id > cache->sync->max_doc_id) {
    cache->sync->max_doc_id = doc_id;
  }
}

/** Drop FTS AUX table DD table objects in vector
@param[in]      aux_vec         aux table name vector
@param[in]      file_per_table  whether file per table
@return true on success, false on failure. */
bool fts_drop_dd_tables(const aux_name_vec_t *aux_vec, bool file_per_table) {
  bool ret = true;

  if (aux_vec == nullptr || aux_vec->aux_name.size() == 0) {
    return (true);
  }

  for (ulint i = 0; i < aux_vec->aux_name.size(); i++) {
    bool retval;

    retval = dd_drop_fts_table(aux_vec->aux_name[i], file_per_table);

    if (!retval) {
      ret = false;
    }
  }

  return (ret);
}

/** Free FTS AUX table names in vector
@param[in]      aux_vec         aux table name vector */
void fts_free_aux_names(aux_name_vec_t *aux_vec) {
  if (aux_vec == nullptr || aux_vec->aux_name.size() == 0) {
    return;
  }

  while (aux_vec->aux_name.size() > 0) {
    char *name = aux_vec->aux_name.back();
    ut::free(name);
    aux_vec->aux_name.pop_back();
  }

  ut_ad(aux_vec->aux_name.size() == 0);
}

/** Drops a table. If the table can't be found we return a SUCCESS code.
@param[in,out]  trx             transaction
@param[in]      table_name      table to drop
@param[in,out]  aux_vec         fts aux table name vector
@return DB_SUCCESS or error code */
static dberr_t fts_drop_table(trx_t *trx, const char *table_name,
                              aux_name_vec_t *aux_vec) {
  dict_table_t *table;
  dberr_t error = DB_SUCCESS;
  THD *thd = current_thd;
  MDL_ticket *mdl = nullptr;

  /* Check that the table exists in our data dictionary.
  Similar to regular drop table case, we will open table with
  DICT_ERR_IGNORE_INDEX_ROOT and DICT_ERR_IGNORE_CORRUPT option */
  table = dd_table_open_on_name(
      thd, &mdl, table_name, true,
      static_cast<dict_err_ignore_t>(DICT_ERR_IGNORE_INDEX_ROOT |
                                     DICT_ERR_IGNORE_CORRUPT));

  if (table != nullptr) {
    char table_name2[MAX_FULL_NAME_LEN];

    strcpy(table_name2, table_name);

    bool file_per_table = dict_table_is_file_per_table(table);

    dd_table_close(table, thd, &mdl, true);

    /* Pass nonatomic=false (dont allow data dict unlock),
    because the transaction may hold locks on SYS_* tables from
    previous calls to fts_drop_table(). */
    error = row_drop_table_for_mysql(table_name, trx, false, nullptr);

    if (error != DB_SUCCESS) {
      ib::error(ER_IB_MSG_464) << "Unable to drop FTS index aux table "
                               << table_name << ": " << ut_strerr(error);
      return (error);
    }

    if (aux_vec == nullptr) {
      dict_sys_mutex_exit();

      if (!dd_drop_fts_table(table_name2, file_per_table)) {
        error = DB_FAIL;
      }

      dict_sys_mutex_enter();
    } else {
      aux_vec->aux_name.push_back(mem_strdup(table_name2));
    }

  } else {
    error = DB_FAIL;
  }

  return (error);
}

/** Rename a single auxiliary table due to database name change.
 @return DB_SUCCESS or error code */
[[nodiscard]] static dberr_t fts_rename_one_aux_table(
    const char *new_name,           /*!< in: new parent tbl name */
    const char *fts_table_old_name, /*!< in: old aux tbl name */
    trx_t *trx,                     /*!< in: transaction */
    bool replay)                    /*!< Whether in replay stage */
{
  char fts_table_new_name[MAX_TABLE_NAME_LEN];
  ulint new_db_name_len = dict_get_db_name_len(new_name);
  ulint old_db_name_len = dict_get_db_name_len(fts_table_old_name);
  ulint table_new_name_len =
      strlen(fts_table_old_name) + new_db_name_len - old_db_name_len;

  /* Check if the new and old database names are the same, if so,
  nothing to do */
  ut_ad((new_db_name_len != old_db_name_len) ||
        strncmp(new_name, fts_table_old_name, old_db_name_len) != 0);

  /* Get the database name from "new_name", and table name
  from the fts_table_old_name */
  strncpy(fts_table_new_name, new_name, new_db_name_len);
  strncpy(fts_table_new_name + new_db_name_len, strchr(fts_table_old_name, '/'),
          table_new_name_len - new_db_name_len);
  fts_table_new_name[table_new_name_len] = 0;

  dberr_t error;
  error = row_rename_table_for_mysql(fts_table_old_name, fts_table_new_name,
                                     nullptr, trx, replay);

  if (error == DB_SUCCESS) {
    /* Update dd tablespace filename. */
    dict_table_t *table;
    table = dict_table_check_if_in_cache_low(fts_table_new_name);
    ut_ad(table != nullptr);

    /* Release dict_sys->mutex to avoid mutex reentrant. */
    table->acquire();
    dict_sys_mutex_exit();

    if (!replay && !dd_rename_fts_table(table, fts_table_old_name)) {
      ut_d(ut_error);
    }

    dict_sys_mutex_enter();
    table->release();
  }

  return (error);
}

/** Rename auxiliary tables for all fts index for a table. This(rename)
 is due to database name change
 @return DB_SUCCESS or error code */
dberr_t fts_rename_aux_tables(dict_table_t *table,  /*!< in: user Table */
                              const char *new_name, /*!< in: new table name */
                              trx_t *trx,           /*!< in: transaction */
                              bool replay)          /*!< in: Whether in replay
                                                        stage */
{
  ulint i;
  fts_table_t fts_table;

  FTS_INIT_FTS_TABLE(&fts_table, nullptr, FTS_COMMON_TABLE, table);

  /* Rename common auxiliary tables */
  for (i = 0; fts_common_tables[i] != nullptr; ++i) {
    char old_table_name[MAX_FULL_NAME_LEN];
    dberr_t err = DB_SUCCESS;

    fts_table.suffix = fts_common_tables[i];

    fts_get_table_name(&fts_table, old_table_name);

    err = fts_rename_one_aux_table(new_name, old_table_name, trx, replay);

    if (err != DB_SUCCESS) {
      return (err);
    }
  }

  fts_t *fts = table->fts;

  /* Rename index specific auxiliary tables */
  for (i = 0; fts->indexes != nullptr && i < ib_vector_size(fts->indexes);
       ++i) {
    dict_index_t *index;

    index = static_cast<dict_index_t *>(ib_vector_getp(fts->indexes, i));

    FTS_INIT_INDEX_TABLE(&fts_table, nullptr, FTS_INDEX_TABLE, index);

    for (ulint j = 0; j < FTS_NUM_AUX_INDEX; ++j) {
      dberr_t err;
      char old_table_name[MAX_FULL_NAME_LEN];

      fts_table.suffix = fts_get_suffix(j);

      fts_get_table_name(&fts_table, old_table_name);

      err = fts_rename_one_aux_table(new_name, old_table_name, trx, replay);

      DBUG_EXECUTE_IF("fts_rename_failure", err = DB_DEADLOCK;);

      if (err != DB_SUCCESS) {
        return (err);
      }
    }
  }

  return (DB_SUCCESS);
}

/** Drops the common ancillary tables needed for supporting an FTS index
on the given table. row_mysql_lock_data_dictionary must have been called
before this.
@param[in,out]  trx             transaction
@param[in,out]  fts_table       table with fts index
@param[in,out]  aux_vec         fts table name vector
@return DB_SUCCESS or error code */
[[nodiscard]] static dberr_t fts_drop_common_tables(trx_t *trx,
                                                    fts_table_t *fts_table,
                                                    aux_name_vec_t *aux_vec) {
  ulint i;
  dberr_t error = DB_SUCCESS;

  for (i = 0; fts_common_tables[i] != nullptr; ++i) {
    dberr_t err;
    char table_name[MAX_FULL_NAME_LEN];

    fts_table->suffix = fts_common_tables[i];

    fts_get_table_name(fts_table, table_name);

    err = fts_drop_table(trx, table_name, aux_vec);

    /* We only return the status of the last error. */
    if (err != DB_SUCCESS && err != DB_FAIL) {
      error = err;
    }
  }

  return (error);
}

/** Since we do a horizontal split on the index table, we need to drop
all the split tables.
@param[in]      trx             transaction
@param[in]      index           fts index
@param[out]     aux_vec         dropped table names vector
@return DB_SUCCESS or error code */
dberr_t fts_drop_index_tables(trx_t *trx, dict_index_t *index,
                              aux_name_vec_t *aux_vec) {
  ulint i;
  fts_table_t fts_table;
  dberr_t error = DB_SUCCESS;

  FTS_INIT_INDEX_TABLE(&fts_table, nullptr, FTS_INDEX_TABLE, index);

  for (i = 0; i < FTS_NUM_AUX_INDEX; ++i) {
    dberr_t err;
    char table_name[MAX_FULL_NAME_LEN];

    fts_table.suffix = fts_get_suffix(i);

    fts_get_table_name(&fts_table, table_name);

    err = fts_drop_table(trx, table_name, aux_vec);

    /* We only return the status of the last error. */
    if (err != DB_SUCCESS && err != DB_FAIL) {
      error = err;
    }
  }

  return (error);
}

/** Write the default settings to the config table.
@param[in]      fts_table       fts table
@return DB_SUCCESS or error code. */
static dberr_t fts_init_config_table(fts_table_t *fts_table) {
  pars_info_t *info;
  que_t *graph;
  char table_name[MAX_FULL_NAME_LEN];
  dberr_t error = DB_SUCCESS;
  trx_t *trx;

  ut_ad(!dict_sys_mutex_own());

  info = pars_info_create();

  fts_table->suffix = FTS_SUFFIX_CONFIG;
  fts_get_table_name(fts_table, table_name);
  pars_info_bind_id(info, true, "config_table", table_name);
  trx = trx_allocate_for_background();

  graph = fts_parse_sql(fts_table, info, fts_config_table_insert_values_sql);

  error = fts_eval_sql(trx, graph);

  que_graph_free(graph);

  if (error == DB_SUCCESS) {
    fts_sql_commit(trx);
  } else {
    fts_sql_rollback(trx);
  }

  trx_free_for_background(trx);

  return (error);
}

/** Empty a common talbes.
@param[in,out]  trx             transaction
@param[in]      fts_table       fts table
@return DB_SUCCESS or error code. */
static dberr_t fts_empty_table(trx_t *trx, fts_table_t *fts_table) {
  pars_info_t *info;
  que_t *graph;
  char table_name[MAX_FULL_NAME_LEN];
  dberr_t error = DB_SUCCESS;

  info = pars_info_create();

  fts_get_table_name(fts_table, table_name);
  pars_info_bind_id(info, true, "table_name", table_name);

  ut_ad(dict_sys_mutex_own());

  dict_sys_mutex_exit();

  graph = fts_parse_sql(fts_table, info, "BEGIN DELETE FROM $table_name;");

  error = fts_eval_sql(trx, graph);

  que_graph_free(graph);

  dict_sys_mutex_enter();

  return (error);
}

/** Empty all common talbes.
@param[in,out]  trx     transaction
@param[in]      table   dict table
@return DB_SUCCESS or error code. */
dberr_t fts_empty_common_tables(trx_t *trx, dict_table_t *table) {
  ulint i;
  fts_table_t fts_table;
  dberr_t error = DB_SUCCESS;

  FTS_INIT_FTS_TABLE(&fts_table, nullptr, FTS_COMMON_TABLE, table);

  for (i = 0; fts_common_tables[i] != nullptr; ++i) {
    dberr_t err;

    fts_table.suffix = fts_common_tables[i];

    /* "config" table should not be emptied, as it has the
    last used DOC ID info */
    if (i == 2) {
      ut_ad(ut_strcmp(fts_table.suffix, "config") == 0);
      continue;
    }

    err = fts_empty_table(trx, &fts_table);

    if (err != DB_SUCCESS) {
      error = err;
    }
  }

  return (error);
}

/** Drops FTS ancillary tables needed for supporting an FTS index
on the given table. row_mysql_lock_data_dictionary must have been called
before this.
@param[in,out]  trx     transaction
@param[in]      fts     fts instance
@param[in,out]  aux_vec fts aux table name vector
@return DB_SUCCESS or error code */
[[nodiscard]] static dberr_t fts_drop_all_index_tables(
    trx_t *trx, fts_t *fts, aux_name_vec_t *aux_vec) {
  dberr_t error = DB_SUCCESS;

  for (ulint i = 0; fts->indexes != nullptr && i < ib_vector_size(fts->indexes);
       ++i) {
    dberr_t err;
    dict_index_t *index;

    index = static_cast<dict_index_t *>(ib_vector_getp(fts->indexes, i));

    err = fts_drop_index_tables(trx, index, aux_vec);

    if (err != DB_SUCCESS) {
      error = err;
    }
  }

  return (error);
}

/** Drops the ancillary tables needed for supporting an FTS index on a
given table. row_mysql_lock_data_dictionary must have been called before
this.
@param[in,out]  trx     transaction
@param[in]      table   table has the fts index
@param[in,out]  aux_vec fts aux table name vector
@return DB_SUCCESS or error code */
dberr_t fts_drop_tables(trx_t *trx, dict_table_t *table,
                        aux_name_vec_t *aux_vec) {
  dberr_t error;
  fts_table_t fts_table;

  FTS_INIT_FTS_TABLE(&fts_table, nullptr, FTS_COMMON_TABLE, table);

  error = fts_drop_common_tables(trx, &fts_table, aux_vec);

  if (error == DB_SUCCESS) {
    error = fts_drop_all_index_tables(trx, table->fts, aux_vec);
  }

  return (error);
}

/** Lock all FTS AUX COMMON tables (for dropping table)
@param[in]      thd     thread locking the AUX table
@param[in,out]  fts_table       table with fts index
@return DB_SUCCESS or error code */
[[nodiscard]] static dberr_t fts_lock_common_tables(THD *thd,
                                                    fts_table_t *fts_table) {
  for (ulint i = 0; fts_common_tables[i] != nullptr; ++i) {
    fts_table->suffix = fts_common_tables[i];

    char table_name[MAX_FULL_NAME_LEN];
    fts_get_table_name(fts_table, table_name);

    std::string db_n;
    std::string table_n;
    dict_name::get_table(table_name, db_n, table_n);

    MDL_ticket *exclusiv_mdl = nullptr;
    if (dd::acquire_exclusive_table_mdl(thd, db_n.c_str(), table_n.c_str(),
                                        false, &exclusiv_mdl)) {
      return (DB_ERROR);
    }
  }

  return (DB_SUCCESS);
}

/** Lock all FTS INDEX AUX tables (for dropping table)
@param[in]      thd     thread locking the AUX table
@param[in]      index   fts index
@return DB_SUCCESS or error code */
dberr_t fts_lock_index_tables(THD *thd, dict_index_t *index) {
  ulint i;
  fts_table_t fts_table;

  FTS_INIT_INDEX_TABLE(&fts_table, nullptr, FTS_INDEX_TABLE, index);

  for (i = 0; i < FTS_NUM_AUX_INDEX; ++i) {
    fts_table.suffix = fts_get_suffix(i);

    char table_name[MAX_FULL_NAME_LEN];
    fts_get_table_name(&fts_table, table_name);

    std::string db_n;
    std::string table_n;
    dict_name::get_table(table_name, db_n, table_n);

    MDL_ticket *exclusiv_mdl = nullptr;
    if (dd::acquire_exclusive_table_mdl(thd, db_n.c_str(), table_n.c_str(),
                                        false, &exclusiv_mdl)) {
      return (DB_ERROR);
    }
  }
  return (DB_SUCCESS);
}

/** Lock all FTS index AUX tables (for dropping table)
@param[in]      thd     thread locking the AUX table
@param[in]      fts     fts instance
@return DB_SUCCESS or error code */
[[nodiscard]] static dberr_t fts_lock_all_index_tables(THD *thd, fts_t *fts) {
  dberr_t error = DB_SUCCESS;

  for (ulint i = 0; fts->indexes != nullptr && i < ib_vector_size(fts->indexes);
       ++i) {
    dberr_t err;
    dict_index_t *index;

    index = static_cast<dict_index_t *>(ib_vector_getp(fts->indexes, i));

    err = fts_lock_index_tables(thd, index);

    if (err != DB_SUCCESS) {
      error = err;
    }
  }

  return (error);
}

/** Lock all FTS AUX tables (for dropping table)
@param[in]      thd     thread locking the AUX table
@param[in]      table   table has the fts index
@return DB_SUCCESS or error code */
dberr_t fts_lock_all_aux_tables(THD *thd, dict_table_t *table) {
  dberr_t error;
  fts_table_t fts_table;

  FTS_INIT_FTS_TABLE(&fts_table, nullptr, FTS_COMMON_TABLE, table);

  error = fts_lock_common_tables(thd, &fts_table);

  if (error == DB_SUCCESS) {
    error = fts_lock_all_index_tables(thd, table->fts);
  }

  return (error);
}

/** Extract only the required flags from table->flags2 for FTS Aux
tables.
@param[in]      flags2  Table flags2
@return extracted flags2 for FTS aux tables */
static inline uint32_t fts_get_table_flags2_for_aux_tables(uint32_t flags2) {
  /* Extract the file_per_table flag, temporary file flag and encryption flag
  from the main FTS table flags2 */
  return ((flags2 & DICT_TF2_USE_FILE_PER_TABLE) |
          (flags2 & DICT_TF2_ENCRYPTION_FILE_PER_TABLE) |
          (flags2 & DICT_TF2_TEMPORARY) | DICT_TF2_AUX);
}

/** Create dict_table_t object for FTS Aux tables.
@param[in]      aux_table_name  FTS Aux table name
@param[in]      table           table object of FTS Index
@param[in]      n_cols          number of columns for FTS Aux table
@return table object for FTS Aux table */
static dict_table_t *fts_create_in_mem_aux_table(const char *aux_table_name,
                                                 const dict_table_t *table,
                                                 ulint n_cols) {
  dict_table_t *new_table = dict_mem_table_create(
      aux_table_name, table->space, n_cols, 0, 0, table->flags,
      fts_get_table_flags2_for_aux_tables(table->flags2));

  if (DICT_TF_HAS_SHARED_SPACE(table->flags)) {
    ut_ad(table->space == fil_space_get_id_by_name(table->tablespace()));
    new_table->tablespace = mem_heap_strdup(new_table->heap, table->tablespace);
  }

  if (DICT_TF_HAS_DATA_DIR(table->flags)) {
    ut_ad(table->data_dir_path != nullptr);
    new_table->data_dir_path =
        mem_heap_strdup(new_table->heap, table->data_dir_path);
  }

  return (new_table);
}

/** Function to create on FTS common table.
@param[in,out]  trx             InnoDB transaction
@param[in]      table           Table that has FTS Index
@param[in]      fts_table_name  FTS AUX table name
@param[in]      fts_suffix      FTS AUX table suffix
@param[in]      heap            heap
@return table object if created, else NULL */
static dict_table_t *fts_create_one_common_table(trx_t *trx,
                                                 const dict_table_t *table,
                                                 const char *fts_table_name,
                                                 const char *fts_suffix,
                                                 mem_heap_t *heap) {
  dict_table_t *new_table = nullptr;
  dberr_t error;
  bool is_config = fts_suffix == FTS_SUFFIX_CONFIG;

  if (!is_config) {
    new_table = fts_create_in_mem_aux_table(fts_table_name, table,
                                            FTS_DELETED_TABLE_NUM_COLS);

    dict_mem_table_add_col(new_table, heap, "doc_id", DATA_INT, DATA_UNSIGNED,
                           FTS_DELETED_TABLE_COL_LEN, true);
  } else {
    /* Config table has different schema. */
    new_table = fts_create_in_mem_aux_table(fts_table_name, table,
                                            FTS_CONFIG_TABLE_NUM_COLS);

    dict_mem_table_add_col(new_table, heap, "key", DATA_VARCHAR, 0,
                           FTS_CONFIG_TABLE_KEY_COL_LEN, true);

    dict_mem_table_add_col(new_table, heap, "value", DATA_VARCHAR,
                           DATA_NOT_NULL, FTS_CONFIG_TABLE_VALUE_COL_LEN, true);
  }

  error = row_create_table_for_mysql(new_table, nullptr, nullptr, trx, nullptr);

  if (error == DB_SUCCESS) {
    dict_index_t *index = dict_mem_index_create(
        fts_table_name, "FTS_COMMON_TABLE_IND", new_table->space,
        DICT_UNIQUE | DICT_CLUSTERED, 1);

    if (!is_config) {
      index->add_field("doc_id", 0, true);
    } else {
      index->add_field("key", 0, true);
    }

    /* We save and restore trx->dict_operation because
    row_create_index_for_mysql() changes the operation to
    TRX_DICT_OP_TABLE. */
    trx_dict_op_t op = trx_get_dict_operation(trx);

    error = row_create_index_for_mysql(index, trx, nullptr, nullptr);

    trx->dict_operation = op;
  }

  if (error != DB_SUCCESS) {
    trx->error_state = error;
    new_table = nullptr;
    ib::warn(ER_IB_MSG_465)
        << "Failed to create FTS common table " << fts_table_name;
  }

  return (new_table);
}

/** Check if common tables already exist
@param[in]      table   table with fts index
@return true on success, false on failure */
bool fts_check_common_tables_exist(const dict_table_t *table) {
  fts_table_t fts_table;
  char fts_name[MAX_FULL_NAME_LEN];

  /* TODO: set a new flag for the situation table has hidden
  FTS_DOC_ID but no FTS indexes. */
  FTS_INIT_FTS_TABLE(&fts_table, nullptr, FTS_COMMON_TABLE, table);
  fts_table.suffix = FTS_SUFFIX_CONFIG;
  fts_get_table_name(&fts_table, fts_name);

  dict_table_t *config_table;
  THD *thd = current_thd;
  MDL_ticket *mdl = reinterpret_cast<MDL_ticket *>(-1);

  /* Check that the table exists in our data dictionary */
  config_table = dd_table_open_on_name(
      thd, &mdl, fts_name, false,
      static_cast<dict_err_ignore_t>(DICT_ERR_IGNORE_INDEX_ROOT |
                                     DICT_ERR_IGNORE_CORRUPT));

  bool exist = false;
  if (config_table != nullptr) {
    dd_table_close(config_table, thd, &mdl, false);
    exist = true;
  }

  return (exist);
}

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
dberr_t fts_create_common_tables(trx_t *trx, const dict_table_t *table,
                                 const char *name, bool skip_doc_id_index) {
  dberr_t error;
  fts_table_t fts_table;
  char full_name[sizeof(fts_common_tables) / sizeof(char *)][MAX_FULL_NAME_LEN];
  dict_index_t *index = nullptr;
  trx_dict_op_t op;

  ut_ad(!dict_sys_mutex_own());
  ut_ad(!fts_check_common_tables_exist(table));

  mem_heap_t *heap = mem_heap_create(1024, UT_LOCATION_HERE);

  FTS_INIT_FTS_TABLE(&fts_table, nullptr, FTS_COMMON_TABLE, table);

  /* Create the FTS tables that are common to an FTS index. */
  for (ulint i = 0; fts_common_tables[i] != nullptr; ++i) {
    fts_table.suffix = fts_common_tables[i];
    fts_get_table_name(&fts_table, full_name[i]);
    dict_table_t *common_table = fts_create_one_common_table(
        trx, table, full_name[i], fts_table.suffix, heap);

    if (common_table == nullptr) {
      error = DB_ERROR;
      goto func_exit;
    }

    DBUG_EXECUTE_IF(
        "ib_fts_aux_table_error",
        /* Return error after creating FTS_AUX_CONFIG table. */
        if (i == 4) {
          error = DB_ERROR;
          goto func_exit;
        });
  }

  /* Write the default settings to the config table. */
  error = fts_init_config_table(&fts_table);

  if (error != DB_SUCCESS || skip_doc_id_index) {
    goto func_exit;
  }

  index = dict_mem_index_create(name, FTS_DOC_ID_INDEX_NAME, table->space,
                                DICT_UNIQUE, 1);
  index->add_field(FTS_DOC_ID_COL_NAME, 0, true);

  op = trx_get_dict_operation(trx);

  error = row_create_index_for_mysql(index, trx, nullptr, nullptr);

  trx->dict_operation = op;

func_exit:
  mem_heap_free(heap);

  return (error);
}

/** Creates one FTS auxiliary index table for an FTS index.
@param[in,out]  trx             transaction
@param[in]      index           the index instance
@param[in]      fts_table       fts_table structure
@param[in]      heap            memory heap
@return DB_SUCCESS or error code */
static dict_table_t *fts_create_one_index_table(trx_t *trx,
                                                const dict_index_t *index,
                                                fts_table_t *fts_table,
                                                mem_heap_t *heap) {
  dict_field_t *field;
  dict_table_t *new_table = nullptr;
  char table_name[MAX_FULL_NAME_LEN];
  dberr_t error;
  CHARSET_INFO *charset;

  ut_ad(index->type & DICT_FTS);

  fts_get_table_name(fts_table, table_name);

  new_table = fts_create_in_mem_aux_table(table_name, fts_table->table,
                                          FTS_AUX_INDEX_TABLE_NUM_COLS);

  field = index->get_field(0);
  charset = fts_get_charset(field->col->prtype);

  dict_mem_table_add_col(
      new_table, heap, "word",
      charset == &my_charset_latin1 ? DATA_VARCHAR : DATA_VARMYSQL,
      field->col->prtype, FTS_INDEX_WORD_LEN, true);

  dict_mem_table_add_col(new_table, heap, "first_doc_id", DATA_INT,
                         DATA_NOT_NULL | DATA_UNSIGNED,
                         FTS_INDEX_FIRST_DOC_ID_LEN, true);

  dict_mem_table_add_col(new_table, heap, "last_doc_id", DATA_INT,
                         DATA_NOT_NULL | DATA_UNSIGNED,
                         FTS_INDEX_LAST_DOC_ID_LEN, true);

  dict_mem_table_add_col(new_table, heap, "doc_count", DATA_INT,
                         DATA_NOT_NULL | DATA_UNSIGNED, FTS_INDEX_DOC_COUNT_LEN,
                         true);

  /* The precise type calculation is as follows:
  least significant byte: MySQL type code (not applicable for sys cols)
  second least : DATA_NOT_NULL | DATA_BINARY_TYPE
  third least  : the MySQL charset-collation code (DATA_MTYPE_MAX) */

  dict_mem_table_add_col(new_table, heap, "ilist", DATA_BLOB,
                         (DATA_MTYPE_MAX << 16) | DATA_UNSIGNED | DATA_NOT_NULL,
                         FTS_INDEX_ILIST_LEN, true);

  error = row_create_table_for_mysql(new_table, nullptr, nullptr, trx, nullptr);

  if (error == DB_SUCCESS) {
    dict_index_t *index = dict_mem_index_create(
        table_name, "FTS_INDEX_TABLE_IND", new_table->space,
        DICT_UNIQUE | DICT_CLUSTERED, 2);
    index->add_field("word", 0, true);
    index->add_field("first_doc_id", 0, true);

    trx_dict_op_t op = trx_get_dict_operation(trx);

    error = row_create_index_for_mysql(index, trx, nullptr, nullptr);

    trx->dict_operation = op;
  }

  if (error != DB_SUCCESS) {
    trx->error_state = error;
    new_table = nullptr;
    ib::warn(ER_IB_MSG_466)
        << "Failed to create FTS index table " << table_name;
  }

  return (new_table);
}

/** Freeze all auiliary tables to be not evictable if exist, with dict_mutex
held
@param[in]      table           InnoDB table object */
void fts_freeze_aux_tables(const dict_table_t *table) {
  fts_table_t fts_table;
  char table_name[MAX_FULL_NAME_LEN];

  FTS_INIT_FTS_TABLE(&fts_table, nullptr, FTS_COMMON_TABLE, table);

  for (ulint i = 0; fts_common_tables[i] != nullptr; ++i) {
    fts_table.suffix = fts_common_tables[i];
    fts_get_table_name(&fts_table, table_name);

    dict_table_t *common;
    common = dd_table_open_on_name_in_mem(table_name, true);
    if (common != nullptr && common->can_be_evicted) {
      dict_table_prevent_eviction(common);
    }

    if (common != nullptr) {
      dd_table_close(common, nullptr, nullptr, true);
    }
  }

  fts_t *fts = table->fts;
  if (fts == nullptr) {
    return;
  }

  for (ulint i = 0; fts->indexes != nullptr && i < ib_vector_size(fts->indexes);
       ++i) {
    dict_index_t *index;
    index = static_cast<dict_index_t *>(ib_vector_getp(fts->indexes, i));

    FTS_INIT_INDEX_TABLE(&fts_table, nullptr, FTS_INDEX_TABLE, index);

    for (ulint j = 0; j < FTS_NUM_AUX_INDEX; ++j) {
      fts_table.suffix = fts_get_suffix(j);
      fts_get_table_name(&fts_table, table_name);

      dict_table_t *index_table;
      index_table = dd_table_open_on_name_in_mem(table_name, true);
      if (index_table != nullptr && index_table->can_be_evicted) {
        dict_table_prevent_eviction(index_table);
      }

      if (index_table != nullptr) {
        dd_table_close(index_table, nullptr, nullptr, true);
      }
    }
  }
}

/** Allow all the auxiliary tables of specified base table to be evictable
if they exist, if not exist just ignore
@param[in]      table           InnoDB table object
@param[in]      dict_locked     True if we have dict_sys mutex */
void fts_detach_aux_tables(const dict_table_t *table, bool dict_locked) {
  fts_table_t fts_table;
  char table_name[MAX_FULL_NAME_LEN];

  if (!dict_locked) {
    dict_sys_mutex_enter();
  }

  FTS_INIT_FTS_TABLE(&fts_table, nullptr, FTS_COMMON_TABLE, table);

  for (ulint i = 0; fts_common_tables[i] != nullptr; ++i) {
    fts_table.suffix = fts_common_tables[i];
    fts_get_table_name(&fts_table, table_name);

    dict_table_t *common;
    common = dd_table_open_on_name_in_mem(table_name, true);
    if (common != nullptr && !common->can_be_evicted) {
      dict_table_allow_eviction(common);
    }

    if (common != nullptr) {
      dd_table_close(common, nullptr, nullptr, true);
    }
  }

  fts_t *fts = table->fts;
  if (fts == nullptr) {
    if (!dict_locked) {
      dict_sys_mutex_exit();
    }

    return;
  }

  for (ulint i = 0; fts->indexes != nullptr && i < ib_vector_size(fts->indexes);
       ++i) {
    dict_index_t *index;
    index = static_cast<dict_index_t *>(ib_vector_getp(fts->indexes, i));

    FTS_INIT_INDEX_TABLE(&fts_table, nullptr, FTS_INDEX_TABLE, index);

    for (ulint j = 0; j < FTS_NUM_AUX_INDEX; ++j) {
      fts_table.suffix = fts_get_suffix(j);
      fts_get_table_name(&fts_table, table_name);

      dict_table_t *index_table;
      index_table = dd_table_open_on_name_in_mem(table_name, true);
      if (index_table != nullptr && !index_table->can_be_evicted) {
        dict_table_allow_eviction(index_table);
      }

      if (index_table != nullptr) {
        dd_table_close(index_table, nullptr, nullptr, true);
      }
    }
  }

  if (!dict_locked) {
    dict_sys_mutex_exit();
  }
}

/** Update DD system table for auxiliary common tables for an FTS index.
@param[in]      table           dict table instance
@return true on success, false on failure */
bool fts_create_common_dd_tables(const dict_table_t *table) {
  fts_table_t fts_table;
  bool ret = true;

  FTS_INIT_FTS_TABLE(&fts_table, nullptr, FTS_COMMON_TABLE, table);

  /* Create the FTS tables that are common to an FTS index. */
  for (ulint i = 0; fts_common_tables[i] != nullptr && ret; ++i) {
    char table_name[MAX_FULL_NAME_LEN];

    fts_table.suffix = fts_common_tables[i];
    fts_get_table_name(&fts_table, table_name);

    dict_table_t *common_table;
    common_table = dd_table_open_on_name_in_mem(table_name, false);
    ut_ad(common_table != nullptr);

    bool is_config = fts_table.suffix == FTS_SUFFIX_CONFIG;
    ret = dd_create_fts_common_table(table, common_table, is_config);

    dd_table_close(common_table, nullptr, nullptr, false);
  }

  return (ret);
}

/** Update DD system table for auxiliary index tables for an FTS index.
@param[in]      index           the index instance
@return DB_SUCCESS or error code */
static dberr_t fts_create_one_index_dd_tables(const dict_index_t *index) {
  ulint i;
  fts_table_t fts_table;
  dberr_t error = DB_SUCCESS;
  char *parent_name = index->table->name.m_name;

  fts_table.type = FTS_INDEX_TABLE;
  fts_table.index_id = index->id;
  fts_table.table_id = index->table->id;
  fts_table.parent = parent_name;
  fts_table.table = index->table;

  for (i = 0; i < FTS_NUM_AUX_INDEX && error == DB_SUCCESS; ++i) {
    dict_table_t *new_table;
    char table_name[MAX_FULL_NAME_LEN];
    CHARSET_INFO *charset;
    dict_field_t *field;

    ut_ad(index->type & DICT_FTS);

    field = index->get_field(0);
    charset = fts_get_charset(field->col->prtype);

    fts_table.suffix = fts_get_suffix(i);
    fts_get_table_name(&fts_table, table_name);

    new_table = dd_table_open_on_name_in_mem(table_name, false);
    ut_ad(new_table != nullptr);

    if (!dd_create_fts_index_table(fts_table.table, new_table, charset)) {
      ib::warn(ER_IB_MSG_467)
          << "Failed to create FTS index dd table " << table_name;
      error = DB_FAIL;
    }

    dd_table_close(new_table, nullptr, nullptr, false);
  }

  return (error);
}

/** Check if a table has FTS index needs to have its auxiliary index
tables' metadata updated in DD
@param[in,out]  table           table to check
@return DB_SUCCESS or error code */
dberr_t fts_create_index_dd_tables(dict_table_t *table) {
  dberr_t error = DB_SUCCESS;

  for (dict_index_t *index = table->first_index();
       index != nullptr && error == DB_SUCCESS; index = index->next()) {
    if ((index->type & DICT_FTS) && index->fill_dd) {
      error = fts_create_one_index_dd_tables(index);
      index->fill_dd = false;
    }

    ut_ad(!index->fill_dd);
  }

  return (error);
}

/** Create auxiliary index tables for an FTS index.
@param[in,out]  trx             transaction
@param[in]      index           the index instance
@param[in]      table_name      table name
@param[in]      table_id        the table id
@return DB_SUCCESS or error code */
dberr_t fts_create_index_tables_low(trx_t *trx, dict_index_t *index,
                                    const char *table_name,
                                    table_id_t table_id) {
  ulint i;
  fts_table_t fts_table;
  dberr_t error = DB_SUCCESS;
  mem_heap_t *heap = mem_heap_create(1024, UT_LOCATION_HERE);

  fts_table.type = FTS_INDEX_TABLE;
  fts_table.index_id = index->id;
  fts_table.table_id = table_id;
  fts_table.parent = table_name;
  fts_table.table = index->table;

  for (i = 0; i < FTS_NUM_AUX_INDEX && error == DB_SUCCESS; ++i) {
    dict_table_t *new_table;

    /* Create the FTS auxiliary tables that are specific
    to an FTS index. We need to preserve the table_id %s
    which fts_parse_sql() will fill in for us. */
    fts_table.suffix = fts_get_suffix(i);

    new_table = fts_create_one_index_table(trx, index, &fts_table, heap);

    if (new_table == nullptr) {
      error = DB_FAIL;
      break;
    }

    DBUG_EXECUTE_IF(
        "ib_fts_index_table_error",
        /* Return error after creating FTS_INDEX_5
        aux table. */
        if (i == 4) {
          error = DB_FAIL;
          break;
        });
  }

  if (error == DB_SUCCESS) {
    index->fill_dd = true;
  }

  mem_heap_free(heap);

  return (error);
}

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
dberr_t fts_create_index_tables(trx_t *trx, dict_index_t *index) {
  dberr_t err;
  dict_table_t *table;

  ut_ad(!dict_sys_mutex_own());

  table = dd_table_open_on_name_in_mem(index->table_name, false);
  ut_a(table != nullptr);
  ut_d(dict_sys_mutex_enter());
  ut_ad(table->get_ref_count() > 1);
  ut_d(dict_sys_mutex_exit());

  err = fts_create_index_tables_low(trx, index, table->name.m_name, table->id);

  dd_table_close(table, nullptr, nullptr, false);

  return (err);
}
#if 0
/******************************************************************//**
Return string representation of state. */
static
const char*
fts_get_state_str(
                                /* out: string representation of state */
    fts_row_state zstate) /*!< in: state */
{
        switch (state) {
        case FTS_INSERT:
                return("INSERT");

        case FTS_MODIFY:
                return("MODIFY");

        case FTS_DELETE:
                return("DELETE");

        case FTS_NOTHING:
                return("NOTHING");

        case FTS_INVALID:
                return("INVALID");

        default:
                return("UNKNOWN");
        }
}
#endif

/** Calculate the new state of a row given the existing state and a new event.
 @return new state of row */
static fts_row_state fts_trx_row_get_new_state(
    fts_row_state old_state, /*!< in: existing state of row */
    fts_row_state event)     /*!< in: new event */
{
  /* The rules for transforming states:

  I = inserted
  M = modified
  D = deleted
  N = nothing

  M+D -> D:

  If the row existed before the transaction started and it is modified
  during the transaction, followed by a deletion of the row, only the
  deletion will be signaled.

  M+ -> M:

  If the row existed before the transaction started and it is modified
  more than once during the transaction, only the last modification
  will be signaled.

  IM*D -> N:

  If a new row is added during the transaction (and possibly modified
  after its initial insertion) but it is deleted before the end of the
  transaction, nothing will be signaled.

  IM* -> I:

  If a new row is added during the transaction and modified after its
  initial insertion, only the addition will be signaled.

  M*DI -> M:

  If the row existed before the transaction started and it is deleted,
  then re-inserted, only a modification will be signaled. Note that
  this case is only possible if the table is using the row's primary
  key for FTS row ids, since those can be re-inserted by the user,
  which is not true for InnoDB generated row ids.

  It is easily seen that the above rules decompose such that we do not
  need to store the row's entire history of events. Instead, we can
  store just one state for the row and update that when new events
  arrive. Then we can implement the above rules as a two-dimensional
  look-up table, and get checking of invalid combinations "for free"
  in the process. */

  /* The lookup table for transforming states. old_state is the
  Y-axis, event is the X-axis. */
  static const fts_row_state table[4][4] = {
      /*    I            M            D            N */
      /* I */ {FTS_INVALID, FTS_INSERT, FTS_NOTHING, FTS_INVALID},
      /* M */ {FTS_INVALID, FTS_MODIFY, FTS_DELETE, FTS_INVALID},
      /* D */ {FTS_MODIFY, FTS_INVALID, FTS_INVALID, FTS_INVALID},
      /* N */ {FTS_INVALID, FTS_INVALID, FTS_INVALID, FTS_INVALID}};

  fts_row_state result;

  ut_a(old_state < FTS_INVALID);
  ut_a(event < FTS_INVALID);

  result = table[(int)old_state][(int)event];
  ut_a(result != FTS_INVALID);

  return (result);
}

/** Create a savepoint instance.
 @return savepoint instance */
static fts_savepoint_t *fts_savepoint_create(
    ib_vector_t *savepoints, /*!< out: InnoDB transaction */
    const char *name,        /*!< in: savepoint name */
    mem_heap_t *heap)        /*!< in: heap */
{
  fts_savepoint_t *savepoint;

  savepoint =
      static_cast<fts_savepoint_t *>(ib_vector_push(savepoints, nullptr));

  memset(savepoint, 0x0, sizeof(*savepoint));

  if (name) {
    savepoint->name = mem_heap_strdup(heap, name);
  }

  savepoint->tables = rbt_create(sizeof(fts_trx_table_t *), fts_trx_table_cmp);

  return (savepoint);
}

/** Create an FTS trx.
@param[in,out]  trx     InnoDB Transaction
@return FTS transaction. */
fts_trx_t *fts_trx_create(trx_t *trx) {
  fts_trx_t *ftt;
  ib_alloc_t *heap_alloc;
  mem_heap_t *heap = mem_heap_create(1024, UT_LOCATION_HERE);

  ut_a(trx->fts_trx == nullptr);

  ftt = static_cast<fts_trx_t *>(mem_heap_alloc(heap, sizeof(fts_trx_t)));
  ftt->trx = trx;
  ftt->heap = heap;

  heap_alloc = ib_heap_allocator_create(heap);

  ftt->savepoints = static_cast<ib_vector_t *>(
      ib_vector_create(heap_alloc, sizeof(fts_savepoint_t), 4));

  ftt->last_stmt = static_cast<ib_vector_t *>(
      ib_vector_create(heap_alloc, sizeof(fts_savepoint_t), 4));

  /* Default instance has no name and no heap. */
  fts_savepoint_create(ftt->savepoints, nullptr, nullptr);
  fts_savepoint_create(ftt->last_stmt, nullptr, nullptr);

  /* Copy savepoints that already set before. */
  for (auto savep : trx->trx_savepoints) {
    fts_savepoint_take(ftt, savep->name);
  }

  return (ftt);
}

/** Create an FTS trx table.
 @return FTS trx table */
static fts_trx_table_t *fts_trx_table_create(
    fts_trx_t *fts_trx,  /*!< in: FTS trx */
    dict_table_t *table) /*!< in: table */
{
  fts_trx_table_t *ftt;

  ftt = static_cast<fts_trx_table_t *>(
      mem_heap_alloc(fts_trx->heap, sizeof(*ftt)));

  if (ftt != nullptr) memset(ftt, 0x0, sizeof(*ftt));

  ftt->table = table;
  ftt->fts_trx = fts_trx;

  ftt->rows = rbt_create(sizeof(fts_trx_row_t), fts_trx_row_doc_id_cmp);

  return (ftt);
}

/** Clone an FTS trx table.
 @return FTS trx table */
static fts_trx_table_t *fts_trx_table_clone(
    const fts_trx_table_t *ftt_src) /*!< in: FTS trx */
{
  fts_trx_table_t *ftt;

  ftt = static_cast<fts_trx_table_t *>(
      mem_heap_alloc(ftt_src->fts_trx->heap, sizeof(*ftt)));

  memset(ftt, 0x0, sizeof(*ftt));

  ftt->table = ftt_src->table;
  ftt->fts_trx = ftt_src->fts_trx;

  ftt->rows = rbt_create(sizeof(fts_trx_row_t), fts_trx_row_doc_id_cmp);

  /* Copy the rb tree values to the new savepoint. */
  rbt_merge_uniq(ftt->rows, ftt_src->rows);

  /* These are only added on commit. At this stage we only have
  the updated row state. */
  ut_a(ftt_src->added_doc_ids == nullptr);

  return (ftt);
}

/** Initialize the FTS trx instance.
 @return FTS trx instance */
static fts_trx_table_t *fts_trx_init(
    trx_t *trx,              /*!< in: transaction */
    dict_table_t *table,     /*!< in: FTS table instance */
    ib_vector_t *savepoints) /*!< in: Savepoints */
{
  fts_trx_table_t *ftt;
  ib_rbt_bound_t parent;
  ib_rbt_t *tables;
  fts_savepoint_t *savepoint;

  savepoint = static_cast<fts_savepoint_t *>(ib_vector_last(savepoints));

  tables = savepoint->tables;
  rbt_search_cmp(tables, &parent, &table->id, fts_trx_table_id_cmp, nullptr);

  if (parent.result == 0) {
    fts_trx_table_t **fttp;

    fttp = rbt_value(fts_trx_table_t *, parent.last);
    ftt = *fttp;
  } else {
    ftt = fts_trx_table_create(trx->fts_trx, table);
    rbt_add_node(tables, &parent, &ftt);
  }

  ut_a(ftt->table == table);

  return (ftt);
}

/** Notify the FTS system about an operation on an FTS-indexed table. */
static void fts_trx_table_add_op(
    fts_trx_table_t *ftt,     /*!< in: FTS trx table */
    doc_id_t doc_id,          /*!< in: doc id */
    fts_row_state state,      /*!< in: state of the row */
    ib_vector_t *fts_indexes) /*!< in: FTS indexes affected */
{
  ib_rbt_t *rows;
  ib_rbt_bound_t parent;

  rows = ftt->rows;
  rbt_search(rows, &parent, &doc_id);

  /* Row id found, update state, and if new state is FTS_NOTHING,
  we delete the row from our tree. */
  if (parent.result == 0) {
    fts_trx_row_t *row = rbt_value(fts_trx_row_t, parent.last);

    row->state = fts_trx_row_get_new_state(row->state, state);

    if (row->state == FTS_NOTHING) {
      if (row->fts_indexes) {
        ib_vector_free(row->fts_indexes);
      }

      ut::free(rbt_remove_node(rows, parent.last));
      row = nullptr;
    } else if (row->fts_indexes != nullptr) {
      ib_vector_free(row->fts_indexes);
      row->fts_indexes = fts_indexes;
    }

  } else { /* Row-id not found, create a new one. */
    fts_trx_row_t row;

    row.doc_id = doc_id;
    row.state = state;
    row.fts_indexes = fts_indexes;

    rbt_add_node(rows, &parent, &row);
  }
}

/** Notify the FTS system about an operation on an FTS-indexed table.
@param[in] trx Innodb transaction
@param[in] table Table
@param[in] doc_id Doc id
@param[in] state State of the row
@param[in] fts_indexes Fts indexes affected (null=all) */
void fts_trx_add_op(trx_t *trx, dict_table_t *table, doc_id_t doc_id,
                    fts_row_state state, ib_vector_t *fts_indexes) {
  fts_trx_table_t *tran_ftt;
  fts_trx_table_t *stmt_ftt;

  if (!trx->fts_trx) {
    trx->fts_trx = fts_trx_create(trx);
  }

  tran_ftt = fts_trx_init(trx, table, trx->fts_trx->savepoints);
  stmt_ftt = fts_trx_init(trx, table, trx->fts_trx->last_stmt);

  fts_trx_table_add_op(tran_ftt, doc_id, state, fts_indexes);
  fts_trx_table_add_op(stmt_ftt, doc_id, state, fts_indexes);
}

/** Fetch callback that converts a textual document id to a binary value and
 stores it in the given place.
 @return always returns NULL */
static bool fts_fetch_store_doc_id(void *row,      /*!< in: sel_node_t* */
                                   void *user_arg) /*!< in: doc_id_t* to store
                                                   doc_id in */
{
  int n_parsed;
  sel_node_t *node = static_cast<sel_node_t *>(row);
  doc_id_t *doc_id = static_cast<doc_id_t *>(user_arg);
  dfield_t *dfield = que_node_get_val(node->select_list);
  dtype_t *type = dfield_get_type(dfield);
  ulint len = dfield_get_len(dfield);

  char buf[32];

  ut_a(dtype_get_mtype(type) == DATA_VARCHAR);
  ut_a(len > 0 && len < sizeof(buf));

  memcpy(buf, dfield_get_data(dfield), len);
  buf[len] = '\0';

  n_parsed = sscanf(buf, FTS_DOC_ID_FORMAT, doc_id);
  ut_a(n_parsed == 1);

  return false;
}

#ifdef FTS_CACHE_SIZE_DEBUG
/** Get the max cache size in bytes. If there is an error reading the
 value we simply print an error message here and return the default
 value to the caller.
 @return max cache size in bytes */
static ulint fts_get_max_cache_size(
    trx_t *trx,             /*!< in: transaction */
    fts_table_t *fts_table) /*!< in: table instance */
{
  dberr_t error;
  fts_string_t value;
  ulint cache_size_in_mb;

  /* Set to the default value. */
  cache_size_in_mb = FTS_CACHE_SIZE_LOWER_LIMIT_IN_MB;

  /* We set the length of value to the max bytes it can hold. This
  information is used by the callback that reads the value. */
  value.f_n_char = 0;
  value.f_len = FTS_MAX_CONFIG_VALUE_LEN;
  value.f_str = ut::malloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, value.f_len + 1);

  error =
      fts_config_get_value(trx, fts_table, FTS_MAX_CACHE_SIZE_IN_MB, &value);

  if (error == DB_SUCCESS) {
    value.f_str[value.f_len] = 0;
    cache_size_in_mb = strtoul((char *)value.f_str, NULL, 10);

    if (cache_size_in_mb > FTS_CACHE_SIZE_UPPER_LIMIT_IN_MB) {
      ib::warn(ER_IB_MSG_468)
          << "FTS max cache size (" << cache_size_in_mb
          << ") out of range."
             " Minimum value is "
          << FTS_CACHE_SIZE_LOWER_LIMIT_IN_MB << "MB and the maximum value is "
          << FTS_CACHE_SIZE_UPPER_LIMIT_IN_MB
          << "MB, setting cache size to upper limit";

      cache_size_in_mb = FTS_CACHE_SIZE_UPPER_LIMIT_IN_MB;

    } else if (cache_size_in_mb < FTS_CACHE_SIZE_LOWER_LIMIT_IN_MB) {
      ib::warn(ER_IB_MSG_469)
          << "FTS max cache size (" << cache_size_in_mb
          << ") out of range."
             " Minimum value is "
          << FTS_CACHE_SIZE_LOWER_LIMIT_IN_MB << "MB and the maximum value is"
          << FTS_CACHE_SIZE_UPPER_LIMIT_IN_MB
          << "MB, setting cache size to lower limit";

      cache_size_in_mb = FTS_CACHE_SIZE_LOWER_LIMIT_IN_MB;
    }
  } else {
    ib::error(ER_IB_MSG_470) << "(" << ut_strerr(error)
                             << ") reading max"
                                " cache config value from config table";
  }

  ut::free(value.f_str);

  return (cache_size_in_mb * 1024 * 1024);
}
#endif

/** Update the next and last Doc ID in the CONFIG table to be the input
 "doc_id" value (+ 1). We would do so after each FTS index build or
 table truncate */
void fts_update_next_doc_id(
    trx_t *trx,                /*!< in/out: transaction */
    const dict_table_t *table, /*!< in: table */
    const char *table_name,    /*!< in: table name, or NULL */
    doc_id_t doc_id)           /*!< in: DOC ID to set */
{
  table->fts->cache->synced_doc_id = doc_id;
  table->fts->cache->next_doc_id = doc_id + 1;

  table->fts->cache->first_doc_id = table->fts->cache->next_doc_id;

  fts_update_sync_doc_id(table, table_name, table->fts->cache->synced_doc_id,
                         trx);
}

/** Get the next available document id.
 @return DB_SUCCESS if OK */
dberr_t fts_get_next_doc_id(const dict_table_t *table, /*!< in: table */
                            doc_id_t *doc_id) /*!< out: new document id */
{
  fts_cache_t *cache = table->fts->cache;

  /* If the Doc ID system has not yet been initialized, we
  will consult the CONFIG table and user table to re-establish
  the initial value of the Doc ID */
  if (cache->first_doc_id == FTS_NULL_DOC_ID) {
    fts_init_doc_id(table);
  }

  if (!DICT_TF2_FLAG_IS_SET(table, DICT_TF2_FTS_HAS_DOC_ID)) {
    *doc_id = FTS_NULL_DOC_ID;
    return (DB_SUCCESS);
  }

  mutex_enter(&cache->doc_id_lock);
  *doc_id = ++cache->next_doc_id;
  mutex_exit(&cache->doc_id_lock);

  return (DB_SUCCESS);
}

/** This function fetch the Doc ID from CONFIG table, and compare with
 the Doc ID supplied. And store the larger one to the CONFIG table.
 @return DB_SUCCESS if OK */
static dberr_t fts_cmp_set_sync_doc_id(
    const dict_table_t *table, /*!< in: table */
    doc_id_t doc_id_cmp,       /*!< in: Doc ID to compare */
    bool read_only,            /*!< in: true if read the
                                synced_doc_id only */
    doc_id_t *doc_id)          /*!< out: larger document id
                               after comparing "doc_id_cmp"
                               to the one stored in CONFIG
                               table */
{
  trx_t *trx;
  pars_info_t *info;
  dberr_t error;
  fts_table_t fts_table;
  que_t *graph = nullptr;
  fts_cache_t *cache = table->fts->cache;
  char table_name[MAX_FULL_NAME_LEN];
retry:
  ut_a(table->fts->doc_col != ULINT_UNDEFINED);

  fts_table.suffix = FTS_SUFFIX_CONFIG;
  fts_table.table_id = table->id;
  fts_table.type = FTS_COMMON_TABLE;
  fts_table.table = table;

  fts_table.parent = table->name.m_name;

  trx = trx_allocate_for_background();

  trx->op_info = "update the next FTS document id";

  info = pars_info_create();

  pars_info_bind_function(info, "my_func", fts_fetch_store_doc_id, doc_id);

  fts_get_table_name(&fts_table, table_name);
  pars_info_bind_id(info, true, "config_table", table_name);

  graph = fts_parse_sql(&fts_table, info,
                        "DECLARE FUNCTION my_func;\n"
                        "DECLARE CURSOR c IS SELECT value FROM $config_table"
                        " WHERE key = 'synced_doc_id' FOR UPDATE;\n"
                        "BEGIN\n"
                        ""
                        "OPEN c;\n"
                        "WHILE 1 = 1 LOOP\n"
                        "  FETCH c INTO my_func();\n"
                        "  IF c % NOTFOUND THEN\n"
                        "    EXIT;\n"
                        "  END IF;\n"
                        "END LOOP;\n"
                        "CLOSE c;");

  *doc_id = 0;

  error = fts_eval_sql(trx, graph);

  que_graph_free(graph);

  // FIXME: We need to retry deadlock errors
  if (error != DB_SUCCESS) {
    goto func_exit;
  }

  if (read_only) {
    goto func_exit;
  }

  if (doc_id_cmp == 0 && *doc_id) {
    cache->synced_doc_id = *doc_id - 1;
  } else {
    cache->synced_doc_id = std::max(doc_id_cmp, *doc_id);
  }

  mutex_enter(&cache->doc_id_lock);
  /* For each sync operation, we will add next_doc_id by 1,
  so to mark a sync operation */
  if (cache->next_doc_id < cache->synced_doc_id + 1) {
    cache->next_doc_id = cache->synced_doc_id + 1;
  }
  mutex_exit(&cache->doc_id_lock);

  if (doc_id_cmp > *doc_id) {
    error = fts_update_sync_doc_id(table, table->name.m_name,
                                   cache->synced_doc_id, trx);
  }

  *doc_id = cache->next_doc_id;

func_exit:

  if (error == DB_SUCCESS) {
    fts_sql_commit(trx);
  } else {
    *doc_id = 0;

    ib::error(ER_IB_MSG_471) << "(" << ut_strerr(error)
                             << ") while getting"
                                " next doc id.";
    fts_sql_rollback(trx);

    if (error == DB_DEADLOCK) {
      std::this_thread::sleep_for(
          std::chrono::milliseconds(FTS_DEADLOCK_RETRY_WAIT_MS));
      goto retry;
    }
  }

  trx_free_for_background(trx);

  return (error);
}

/** Update the last document id. This function could create a new
 transaction to update the last document id.
 @return DB_SUCCESS if OK */
static dberr_t fts_update_sync_doc_id(
    const dict_table_t *table, /*!< in: table */
    const char *table_name,    /*!< in: table name, or NULL */
    doc_id_t doc_id,           /*!< in: last document id */
    trx_t *trx)                /*!< in: update trx, or NULL */
{
  byte id[FTS_MAX_ID_LEN];
  pars_info_t *info;
  fts_table_t fts_table;
  ulint id_len;
  que_t *graph = nullptr;
  dberr_t error;
  bool local_trx = false;
  fts_cache_t *cache = table->fts->cache;
  char fts_name[MAX_FULL_NAME_LEN];

  fts_table.suffix = FTS_SUFFIX_CONFIG;
  fts_table.table_id = table->id;
  fts_table.type = FTS_COMMON_TABLE;
  fts_table.table = table;
  if (table_name) {
    fts_table.parent = table_name;
  } else {
    fts_table.parent = table->name.m_name;
  }

  if (!trx) {
    trx = trx_allocate_for_background();

    trx->op_info = "setting last FTS document id";
    local_trx = true;
  }

  info = pars_info_create();

  id_len = snprintf((char *)id, sizeof(id), FTS_DOC_ID_FORMAT, doc_id + 1);

  pars_info_bind_varchar_literal(info, "doc_id", id, id_len);

  fts_get_table_name(&fts_table, fts_name);
  pars_info_bind_id(info, true, "table_name", fts_name);

  graph = fts_parse_sql(&fts_table, info,
                        "BEGIN"
                        " UPDATE $table_name SET value = :doc_id"
                        " WHERE key = 'synced_doc_id';");

  error = fts_eval_sql(trx, graph);

  que_graph_free(graph);

  if (local_trx) {
    if (error == DB_SUCCESS) {
      fts_sql_commit(trx);
      cache->synced_doc_id = doc_id;
    } else {
      ib::error(ER_IB_MSG_472) << "(" << ut_strerr(error)
                               << ") while"
                                  " updating last doc id.";

      fts_sql_rollback(trx);
    }
    trx_free_for_background(trx);
  }

  return (error);
}

/** Create a new fts_doc_ids_t.
 @return new fts_doc_ids_t */
fts_doc_ids_t *fts_doc_ids_create(void) {
  fts_doc_ids_t *fts_doc_ids;
  mem_heap_t *heap = mem_heap_create(512, UT_LOCATION_HERE);

  fts_doc_ids =
      static_cast<fts_doc_ids_t *>(mem_heap_alloc(heap, sizeof(*fts_doc_ids)));

  fts_doc_ids->self_heap = ib_heap_allocator_create(heap);

  fts_doc_ids->doc_ids = static_cast<ib_vector_t *>(
      ib_vector_create(fts_doc_ids->self_heap, sizeof(fts_update_t), 32));

  return (fts_doc_ids);
}

/** Free a fts_doc_ids_t. */
void fts_doc_ids_free(fts_doc_ids_t *fts_doc_ids) {
  mem_heap_t *heap = static_cast<mem_heap_t *>(fts_doc_ids->self_heap->arg);

  memset(fts_doc_ids, 0, sizeof(*fts_doc_ids));

  mem_heap_free(heap);
}

/** Do commit-phase steps necessary for the insertion of a new row.
@param[in]      ftt     FTS transaction table
@param[in]      row     row to be inserted in index
*/
static void fts_add(fts_trx_table_t *ftt, fts_trx_row_t *row) {
  dict_table_t *table = ftt->table;
  doc_id_t doc_id = row->doc_id;

  ut_a(row->state == FTS_INSERT || row->state == FTS_MODIFY);

  fts_add_doc_by_id(ftt, doc_id, row->fts_indexes);

  mutex_enter(&table->fts->cache->deleted_lock);
  ++table->fts->cache->added;
  mutex_exit(&table->fts->cache->deleted_lock);

  if (!DICT_TF2_FLAG_IS_SET(table, DICT_TF2_FTS_HAS_DOC_ID) &&
      doc_id >= table->fts->cache->next_doc_id) {
    table->fts->cache->next_doc_id = doc_id + 1;
  }
}

/** Do commit-phase steps necessary for the deletion of a row.
 @return DB_SUCCESS or error code */
[[nodiscard]] static dberr_t fts_delete(
    fts_trx_table_t *ftt, /*!< in: FTS trx table */
    fts_trx_row_t *row)   /*!< in: row */
{
  que_t *graph;
  fts_table_t fts_table;
  dberr_t error = DB_SUCCESS;
  doc_id_t write_doc_id;
  dict_table_t *table = ftt->table;
  doc_id_t doc_id = row->doc_id;
  trx_t *trx = ftt->fts_trx->trx;
  pars_info_t *info = pars_info_create();
  fts_cache_t *cache = table->fts->cache;

  /* we do not index Documents whose Doc ID value is 0 */
  if (doc_id == FTS_NULL_DOC_ID) {
    ut_ad(!DICT_TF2_FLAG_IS_SET(table, DICT_TF2_FTS_HAS_DOC_ID));
    return (error);
  }

  ut_a(row->state == FTS_DELETE || row->state == FTS_MODIFY);

  FTS_INIT_FTS_TABLE(&fts_table, FTS_SUFFIX_DELETED, FTS_COMMON_TABLE, table);

  /* Convert to "storage" byte order. */
  fts_write_doc_id((byte *)&write_doc_id, doc_id);
  fts_bind_doc_id(info, "doc_id", &write_doc_id);

  /* It is possible we update a record that has not yet been sync-ed
  into cache from last crash (delete Doc will not initialize the
  sync). Avoid any added counter accounting until the FTS cache
  is re-established and sync-ed */
  if (table->fts->fts_status & ADDED_TABLE_SYNCED &&
      doc_id > cache->synced_doc_id) {
    mutex_enter(&table->fts->cache->deleted_lock);

    /* The Doc ID could belong to those left in
    ADDED table from last crash. So need to check
    if it is less than first_doc_id when we initialize
    the Doc ID system after reboot */
    if (doc_id >= table->fts->cache->first_doc_id &&
        table->fts->cache->added > 0) {
      --table->fts->cache->added;
    }

    mutex_exit(&table->fts->cache->deleted_lock);

    /* Only if the row was really deleted. */
    ut_a(row->state == FTS_DELETE || row->state == FTS_MODIFY);
  }

  /* Note the deleted document for OPTIMIZE to purge. */
  if (error == DB_SUCCESS) {
    char table_name[MAX_FULL_NAME_LEN];

    trx->op_info = "adding doc id to FTS DELETED";

    info->graph_owns_us = true;

    fts_table.suffix = FTS_SUFFIX_DELETED;

    fts_get_table_name(&fts_table, table_name);
    pars_info_bind_id(info, true, "deleted", table_name);

    graph = fts_parse_sql(&fts_table, info,
                          "BEGIN INSERT INTO $deleted VALUES (:doc_id);");

    error = fts_eval_sql(trx, graph);

    fts_que_graph_free(graph);
  } else {
    pars_info_free(info);
  }

  /* Increment the total deleted count, this is used to calculate the
  number of documents indexed. */
  if (error == DB_SUCCESS) {
    mutex_enter(&table->fts->cache->deleted_lock);

    ++table->fts->cache->deleted;

    mutex_exit(&table->fts->cache->deleted_lock);
  }

  return (error);
}

/** Do commit-phase steps necessary for the modification of a row.
 @return DB_SUCCESS or error code */
[[nodiscard]] static dberr_t fts_modify(
    fts_trx_table_t *ftt, /*!< in: FTS trx table */
    fts_trx_row_t *row)   /*!< in: row */
{
  dberr_t error;

  ut_a(row->state == FTS_MODIFY);

  error = fts_delete(ftt, row);

  if (error == DB_SUCCESS) {
    fts_add(ftt, row);
  }

  return (error);
}

dberr_t fts_create_doc_id(dict_table_t *table, dtuple_t *row,
                          mem_heap_t *heap) {
  doc_id_t doc_id;
  dberr_t error = DB_SUCCESS;

  ut_a(table->fts->doc_col != ULINT_UNDEFINED);

  if (!DICT_TF2_FLAG_IS_SET(table, DICT_TF2_FTS_HAS_DOC_ID)) {
    if (table->fts->cache->first_doc_id == FTS_NULL_DOC_ID) {
      error = fts_get_next_doc_id(table, &doc_id);
    }
    return (error);
  }

  error = fts_get_next_doc_id(table, &doc_id);

  if (error == DB_SUCCESS) {
    dfield_t *dfield;
    doc_id_t *write_doc_id;

    ut_a(doc_id > 0);

    dfield = dtuple_get_nth_field(row, table->fts->doc_col);
    write_doc_id =
        static_cast<doc_id_t *>(mem_heap_alloc(heap, sizeof(*write_doc_id)));

    ut_a(doc_id != FTS_NULL_DOC_ID);
    ut_a(sizeof(doc_id) == dfield->type.len);
    fts_write_doc_id((byte *)write_doc_id, doc_id);

    dfield_set_data(dfield, write_doc_id, sizeof(*write_doc_id));
  }

  return (error);
}

/** The given transaction is about to be committed; do whatever is necessary
 from the FTS system's POV.
 @return DB_SUCCESS or error code */
[[nodiscard]] static dberr_t fts_commit_table(
    fts_trx_table_t *ftt) /*!< in: FTS table to commit*/
{
  const ib_rbt_node_t *node;
  ib_rbt_t *rows;
  dberr_t error = DB_SUCCESS;
  fts_cache_t *cache = ftt->table->fts->cache;
  trx_t *trx = trx_allocate_for_background();

  rows = ftt->rows;

  ftt->fts_trx->trx = trx;

  if (cache->get_docs == nullptr) {
    rw_lock_x_lock(&cache->init_lock, UT_LOCATION_HERE);
    if (cache->get_docs == nullptr) {
      cache->get_docs = fts_get_docs_create(cache);
    }
    rw_lock_x_unlock(&cache->init_lock);
  }

  for (node = rbt_first(rows); node != nullptr && error == DB_SUCCESS;
       node = rbt_next(rows, node)) {
    fts_trx_row_t *row = rbt_value(fts_trx_row_t, node);

    switch (row->state) {
      case FTS_INSERT:
        fts_add(ftt, row);
        break;

      case FTS_MODIFY:
        error = fts_modify(ftt, row);
        break;

      case FTS_DELETE:
        error = fts_delete(ftt, row);
        break;

      default:
        ut_error;
    }
  }

  fts_sql_commit(trx);

  trx_free_for_background(trx);

  return (error);
}

/** The given transaction is about to be committed; do whatever is necessary
 from the FTS system's POV.
 @return DB_SUCCESS or error code */
dberr_t fts_commit(trx_t *trx) /*!< in: transaction */
{
  const ib_rbt_node_t *node;
  dberr_t error;
  ib_rbt_t *tables;
  fts_savepoint_t *savepoint;

  savepoint =
      static_cast<fts_savepoint_t *>(ib_vector_last(trx->fts_trx->savepoints));
  tables = savepoint->tables;

  for (node = rbt_first(tables), error = DB_SUCCESS;
       node != nullptr && error == DB_SUCCESS; node = rbt_next(tables, node)) {
    fts_trx_table_t **ftt;

    ftt = rbt_value(fts_trx_table_t *, node);

    error = fts_commit_table(*ftt);
  }

  return (error);
}

/** Initialize a document. */
void fts_doc_init(fts_doc_t *doc) /*!< in: doc to initialize */
{
  mem_heap_t *heap = mem_heap_create(32, UT_LOCATION_HERE);

  memset(doc, 0, sizeof(*doc));

  doc->self_heap = ib_heap_allocator_create(heap);
}

/** Free document. */
void fts_doc_free(fts_doc_t *doc) /*!< in: document */
{
  mem_heap_t *heap = static_cast<mem_heap_t *>(doc->self_heap->arg);

  if (doc->tokens) {
    rbt_free(doc->tokens);
  }

  ut_d(memset(doc, 0, sizeof(*doc)));

  mem_heap_free(heap);
}

/** Callback function for fetch that stores the text of an FTS document,
 converting each column to UTF-16.
 @return always false */
bool fts_query_expansion_fetch_doc(void *row,      /*!< in: sel_node_t* */
                                   void *user_arg) /*!< in: fts_doc_t* */
{
  que_node_t *exp;
  sel_node_t *node = static_cast<sel_node_t *>(row);
  fts_doc_t *result_doc = static_cast<fts_doc_t *>(user_arg);
  dfield_t *dfield;
  ulint len;
  ulint doc_len;
  fts_doc_t doc;
  CHARSET_INFO *doc_charset = nullptr;
  ulint field_no = 0;

  len = 0;

  fts_doc_init(&doc);
  doc.found = true;

  exp = node->select_list;
  doc_len = 0;

  doc_charset = result_doc->charset;

  /* Copy each indexed column content into doc->text.f_str */
  while (exp) {
    dfield = que_node_get_val(exp);
    len = dfield_get_len(dfield);

    /* NULL column */
    if (len == UNIV_SQL_NULL) {
      exp = que_node_get_next(exp);
      continue;
    }

    if (!doc_charset) {
      doc_charset = fts_get_charset(dfield->type.prtype);
    }

    doc.charset = doc_charset;
    doc.is_ngram = result_doc->is_ngram;

    if (dfield_is_ext(dfield)) {
      /* We ignore columns that are stored externally, this
      could result in too many words to search */
      exp = que_node_get_next(exp);
      continue;
    } else {
      doc.text.f_n_char = 0;

      doc.text.f_str = static_cast<byte *>(dfield_get_data(dfield));

      doc.text.f_len = len;
    }

    if (field_no == 0) {
      fts_tokenize_document(&doc, result_doc, result_doc->parser);
    } else {
      fts_tokenize_document_next(&doc, doc_len, result_doc, result_doc->parser);
    }

    exp = que_node_get_next(exp);

    doc_len += (exp) ? len + 1 : len;

    field_no++;
  }

  ut_ad(doc_charset);

  if (!result_doc->charset) {
    result_doc->charset = doc_charset;
  }

  fts_doc_free(&doc);

  return false;
}

/** fetch and tokenize the document. */
static void fts_fetch_doc_from_rec(
    fts_get_doc_t *get_doc,    /*!< in: FTS index's get_doc struct */
    dict_index_t *clust_index, /*!< in: cluster index */
    btr_pcur_t *pcur,          /*!< in: cursor whose position
                               has been stored */
    ulint *offsets,            /*!< in: offsets */
    fts_doc_t *doc)            /*!< out: fts doc to hold parsed
                               documents */
{
  dict_index_t *index;
  dict_table_t *table;
  const rec_t *clust_rec;
  ulint num_field;
  const dict_field_t *ifield;
  const dict_col_t *col;
  uint16_t clust_pos;
  ulint i;
  ulint doc_len = 0;
  ulint processed_doc = 0;
  st_mysql_ftparser *parser;

  if (!get_doc) {
    return;
  }

  index = get_doc->index_cache->index;
  table = get_doc->index_cache->index->table;
  parser = get_doc->index_cache->index->parser;

  clust_rec = pcur->get_rec();

  num_field = dict_index_get_n_fields(index);

  for (i = 0; i < num_field; i++) {
    ifield = index->get_field(i);
    col = ifield->col;
    clust_pos = static_cast<uint16_t>(dict_col_get_clust_pos(col, clust_index));

    if (!get_doc->index_cache->charset) {
      get_doc->index_cache->charset = fts_get_charset(ifield->col->prtype);
    }

    if (rec_offs_nth_extern(index, offsets, clust_pos)) {
      doc->text.f_str = lob::btr_rec_copy_externally_stored_field(
          nullptr, clust_index, clust_rec, offsets, dict_table_page_size(table),
          clust_pos, &doc->text.f_len, nullptr, false,
          static_cast<mem_heap_t *>(doc->self_heap->arg));
    } else {
      doc->text.f_str = const_cast<byte *>(rec_get_nth_field_instant(
          clust_rec, offsets, clust_pos, clust_index, &doc->text.f_len));
    }

    doc->found = true;
    doc->charset = get_doc->index_cache->charset;
    doc->is_ngram = index->is_ngram;

    /* Null Field */
    if (doc->text.f_len == UNIV_SQL_NULL || doc->text.f_len == 0) {
      continue;
    }

    if (processed_doc == 0) {
      fts_tokenize_document(doc, nullptr, parser);
    } else {
      fts_tokenize_document_next(doc, doc_len, nullptr, parser);
    }

    processed_doc++;
    doc_len += doc->text.f_len + 1;
  }
}

/** Fetch the data from tuple and tokenize the document.
@param[in]      get_doc FTS index's get_doc struct
@param[in]      tuple   tuple should be arranged in table schema order
@param[out]     doc     fts doc to hold parsed documents. */
static void fts_fetch_doc_from_tuple(fts_get_doc_t *get_doc,
                                     const dtuple_t *tuple, fts_doc_t *doc) {
  dict_index_t *index;
  st_mysql_ftparser *parser;
  ulint doc_len = 0;
  ulint processed_doc = 0;
  ulint num_field;

  if (get_doc == nullptr) {
    return;
  }

  index = get_doc->index_cache->index;
  parser = get_doc->index_cache->index->parser;
  num_field = dict_index_get_n_fields(index);

  for (ulint i = 0; i < num_field; i++) {
    const dict_field_t *ifield;
    const dict_col_t *col;
    ulint pos;
    dfield_t *field;

    ifield = index->get_field(i);
    col = ifield->col;
    pos = dict_col_get_no(col);
    field = dtuple_get_nth_field(tuple, pos);

    if (!get_doc->index_cache->charset) {
      get_doc->index_cache->charset = fts_get_charset(ifield->col->prtype);
    }

    ut_ad(!dfield_is_ext(field));

    doc->text.f_str = (byte *)dfield_get_data(field);
    doc->text.f_len = dfield_get_len(field);
    doc->found = true;
    doc->charset = get_doc->index_cache->charset;
    doc->is_ngram = index->is_ngram;

    /* field data is NULL. */
    if (doc->text.f_len == UNIV_SQL_NULL || doc->text.f_len == 0) {
      continue;
    }

    if (processed_doc == 0) {
      fts_tokenize_document(doc, nullptr, parser);
    } else {
      fts_tokenize_document_next(doc, doc_len, nullptr, parser);
    }

    processed_doc++;
    doc_len += doc->text.f_len + 1;
  }
}

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
                            const dtuple_t *tuple) {
  mtr_t mtr;
  fts_cache_t *cache = ftt->table->fts->cache;

  ut_ad(cache->get_docs);

  if (!(ftt->table->fts->fts_status & ADDED_TABLE_SYNCED)) {
    fts_init_index(ftt->table, false);
  }

  mtr_start(&mtr);

  ulint num_idx = ib_vector_size(cache->get_docs);

  for (ulint i = 0; i < num_idx; ++i) {
    fts_doc_t doc;
    dict_table_t *table;
    fts_get_doc_t *get_doc;

    get_doc = static_cast<fts_get_doc_t *>(ib_vector_get(cache->get_docs, i));
    table = get_doc->index_cache->index->table;

    fts_doc_init(&doc);
    fts_fetch_doc_from_tuple(get_doc, tuple, &doc);

    if (doc.found) {
      mtr_commit(&mtr);
      rw_lock_x_lock(&table->fts->cache->lock, UT_LOCATION_HERE);

      if (table->fts->cache->stopword_info.status & STOPWORD_NOT_INIT) {
        fts_load_stopword(table, nullptr, nullptr, nullptr, true, true);
      }

      fts_cache_add_doc(table->fts->cache, get_doc->index_cache, doc_id,
                        doc.tokens);

      rw_lock_x_unlock(&table->fts->cache->lock);

      if (cache->total_size > fts_max_cache_size / 5 || fts_need_sync) {
        fts_sync(cache->sync, true, false, false);
      }

      mtr_start(&mtr);
    }

    fts_doc_free(&doc);
  }

  mtr_commit(&mtr);
}

static ulint fts_add_doc_by_id(fts_trx_table_t *ftt, doc_id_t doc_id,
                               ib_vector_t *fts_indexes [[maybe_unused]]) {
  mtr_t mtr;
  mem_heap_t *heap;
  btr_pcur_t pcur;
  dict_table_t *table;
  dtuple_t *tuple;
  dfield_t *dfield;
  fts_get_doc_t *get_doc;
  doc_id_t temp_doc_id;
  dict_index_t *clust_index;
  dict_index_t *fts_id_index;
  fts_cache_t *cache = ftt->table->fts->cache;

  ut_ad(cache->get_docs);

  /* If Doc ID has been supplied by the user, then the table
  might not yet be sync-ed */

  if (!(ftt->table->fts->fts_status & ADDED_TABLE_SYNCED)) {
    fts_init_index(ftt->table, false);
  }

  /* Get the first FTS index's get_doc */
  get_doc = static_cast<fts_get_doc_t *>(ib_vector_get(cache->get_docs, 0));
  ut_ad(get_doc);

  table = get_doc->index_cache->index->table;

  heap = mem_heap_create(512, UT_LOCATION_HERE);

  clust_index = table->first_index();
  fts_id_index = table->fts_doc_id_index;

  /* Check whether the index on FTS_DOC_ID is cluster index */
  auto is_id_cluster = (clust_index == fts_id_index);

  mtr_start(&mtr);
  pcur.init();

  /* Search based on Doc ID. Here, we'll need to consider the case
  when there is no primary index on Doc ID */
  tuple = dtuple_create(heap, 1);
  dfield = dtuple_get_nth_field(tuple, 0);
  dfield->type.mtype = DATA_INT;
  dfield->type.prtype = DATA_NOT_NULL | DATA_UNSIGNED | DATA_BINARY_TYPE;

  mach_write_to_8((byte *)&temp_doc_id, doc_id);
  dfield_set_data(dfield, &temp_doc_id, sizeof(temp_doc_id));

  pcur.open_no_init(fts_id_index, tuple, PAGE_CUR_LE, BTR_SEARCH_LEAF, 0, &mtr,
                    UT_LOCATION_HERE);

  /* If we have a match, add the data to doc structure */
  if (pcur.get_low_match() == 1) {
    const rec_t *rec;
    btr_pcur_t *doc_pcur;
    const rec_t *clust_rec;
    btr_pcur_t clust_pcur;
    ulint *offsets = nullptr;
    ulint num_idx = ib_vector_size(cache->get_docs);

    rec = pcur.get_rec();

    /* Doc could be deleted */
    if (page_rec_is_infimum(rec) ||
        rec_get_deleted_flag(rec, dict_table_is_comp(table))) {
      goto func_exit;
    }

    if (is_id_cluster) {
      clust_rec = rec;
      doc_pcur = &pcur;
    } else {
      dtuple_t *clust_ref;
      ulint n_fields;

      clust_pcur.init();
      n_fields = dict_index_get_n_unique(clust_index);

      clust_ref = dtuple_create(heap, n_fields);
      dict_index_copy_types(clust_ref, clust_index, n_fields);

      row_build_row_ref_in_tuple(clust_ref, rec, fts_id_index, nullptr);

      clust_pcur.open_no_init(clust_index, clust_ref, PAGE_CUR_LE,
                              BTR_SEARCH_LEAF, 0, &mtr, UT_LOCATION_HERE);

      doc_pcur = &clust_pcur;
      clust_rec = clust_pcur.get_rec();
    }

    offsets = rec_get_offsets(clust_rec, clust_index, nullptr, ULINT_UNDEFINED,
                              UT_LOCATION_HERE, &heap);

    for (ulint i = 0; i < num_idx; ++i) {
      fts_doc_t doc;
      dict_table_t *table;
      fts_get_doc_t *get_doc;

      get_doc = static_cast<fts_get_doc_t *>(ib_vector_get(cache->get_docs, i));

      table = get_doc->index_cache->index->table;

      fts_doc_init(&doc);

      fts_fetch_doc_from_rec(get_doc, clust_index, doc_pcur, offsets, &doc);

      if (doc.found) {
        doc_pcur->store_position(&mtr);
        mtr_commit(&mtr);

        DEBUG_SYNC_C("fts_instrument_sync_cache_wait");
        rw_lock_x_lock(&table->fts->cache->lock, UT_LOCATION_HERE);

        if (table->fts->cache->stopword_info.status & STOPWORD_NOT_INIT) {
          fts_load_stopword(table, nullptr, nullptr, nullptr, true, true);
        }

        fts_cache_add_doc(table->fts->cache, get_doc->index_cache, doc_id,
                          doc.tokens);

        bool need_sync = false;
        if ((cache->total_size - cache->total_size_before_sync >
                 fts_max_cache_size / 10 ||
             fts_need_sync) &&
            !cache->sync->in_progress) {
          need_sync = true;
          cache->total_size_before_sync = cache->total_size;
        }

        rw_lock_x_unlock(&table->fts->cache->lock);

        DBUG_EXECUTE_IF("fts_instrument_sync_cache_wait", {
          ut_a(srv_fatal_semaphore_wait_extend.load() == 0);
          // we size smaller than permissible min value for this sys var
          const auto old_fts_max_cache_size = fts_max_cache_size;
          fts_max_cache_size = 100;
          fts_sync(cache->sync, true, true, false);
          fts_max_cache_size = old_fts_max_cache_size;
        });

        DBUG_EXECUTE_IF("fts_instrument_sync",
                        fts_optimize_request_sync_table(table);
                        os_event_wait(cache->sync->event););

        DBUG_EXECUTE_IF("fts_instrument_sync_debug",
                        fts_sync(cache->sync, true, true, false););

        DEBUG_SYNC_C("fts_instrument_sync_request");
        DBUG_EXECUTE_IF("fts_instrument_sync_request",
                        fts_optimize_request_sync_table(table););

        if (need_sync) {
          fts_optimize_request_sync_table(table);
        }

        mtr_start(&mtr);

        if (i < num_idx - 1) {
          [[maybe_unused]] auto success = doc_pcur->restore_position(
              BTR_SEARCH_LEAF, &mtr, UT_LOCATION_HERE);

          ut_ad(success);
        }
      }

      fts_doc_free(&doc);
    }

    if (!is_id_cluster) {
      doc_pcur->close();
    }
  }
func_exit:
  mtr_commit(&mtr);

  pcur.close();

  mem_heap_free(heap);
  return true;
}

/** Callback function to read a single ulint column.
 return always returns true */
static bool fts_read_ulint(void *row,      /*!< in: sel_node_t* */
                           void *user_arg) /*!< in: pointer to ulint */
{
  sel_node_t *sel_node = static_cast<sel_node_t *>(row);
  ulint *value = static_cast<ulint *>(user_arg);
  que_node_t *exp = sel_node->select_list;
  dfield_t *dfield = que_node_get_val(exp);
  void *data = dfield_get_data(dfield);

  *value =
      static_cast<ulint>(mach_read_from_4(static_cast<const byte *>(data)));

  return true;
}

/** Get maximum Doc ID in a table if index "FTS_DOC_ID_INDEX" exists
 @return max Doc ID or 0 if index "FTS_DOC_ID_INDEX" does not exist */
doc_id_t fts_get_max_doc_id(dict_table_t *table) /*!< in: user table */
{
  dict_index_t *index;
  dict_field_t *dfield [[maybe_unused]] = nullptr;
  doc_id_t doc_id = 0;
  mtr_t mtr;
  btr_pcur_t pcur;

  index = table->fts_doc_id_index;

  if (!index) {
    return (0);
  }

  dfield = index->get_field(0);

#if 0 /* This can fail when renaming a column to FTS_DOC_ID_COL_NAME. */
        ut_ad(innobase_strcasecmp(FTS_DOC_ID_COL_NAME, dfield->name) == 0);
#endif

  mtr_start(&mtr);

  /* fetch the largest indexes value */
  pcur.open_at_side(false, index, BTR_SEARCH_LEAF, true, 0, &mtr);

  if (!page_is_empty(pcur.get_page())) {
    const rec_t *rec = nullptr;
    ulint offsets_[REC_OFFS_NORMAL_SIZE];
    ulint *offsets = offsets_;
    mem_heap_t *heap = nullptr;
    ulint len;
    const void *data;

    rec_offs_init(offsets_);

    do {
      rec = pcur.get_rec();

      if (page_rec_is_user_rec(rec)) {
        break;
      }
    } while (pcur.move_to_prev(&mtr));

    if (!rec) {
      goto func_exit;
    }

    offsets = rec_get_offsets(rec, index, offsets, ULINT_UNDEFINED,
                              UT_LOCATION_HERE, &heap);

    data = rec_get_nth_field(nullptr, rec, offsets, 0, &len);

    doc_id =
        static_cast<doc_id_t>(fts_read_doc_id(static_cast<const byte *>(data)));
  }

func_exit:
  pcur.close();
  mtr_commit(&mtr);
  return (doc_id);
}

/** Fetch document with the given document id.
 @return DB_SUCCESS if OK else error */
dberr_t fts_doc_fetch_by_doc_id(
    fts_get_doc_t *get_doc,     /*!< in: state */
    doc_id_t doc_id,            /*!< in: id of document to
                                fetch */
    dict_index_t *index_to_use, /*!< in: caller supplied FTS index,
                                or NULL */
    ulint option,               /*!< in: search option, if it is
                                greater than doc_id or equal */
    fts_sql_callback callback,  /*!< in: callback to read */
    void *arg)                  /*!< in: callback arg */
{
  pars_info_t *info;
  dberr_t error;
  const char *select_str;
  doc_id_t write_doc_id;
  dict_index_t *index;
  trx_t *trx = trx_allocate_for_background();
  que_t *graph;

  trx->op_info = "fetching indexed FTS document";

  /* The FTS index can be supplied by caller directly with
  "index_to_use", otherwise, get it from "get_doc" */
  index = (index_to_use) ? index_to_use : get_doc->index_cache->index;

  if (get_doc && get_doc->get_document_graph) {
    info = get_doc->get_document_graph->info;
  } else {
    info = pars_info_create();
  }

  /* Convert to "storage" byte order. */
  fts_write_doc_id((byte *)&write_doc_id, doc_id);
  fts_bind_doc_id(info, "doc_id", &write_doc_id);
  pars_info_bind_function(info, "my_func", callback, arg);

  select_str = fts_get_select_columns_str(index, info, info->heap);
  pars_info_bind_id(info, true, "table_name", index->table_name);

  if (!get_doc || !get_doc->get_document_graph) {
    if (option == FTS_FETCH_DOC_BY_ID_EQUAL) {
      graph = fts_parse_sql(nullptr, info,
                            mem_heap_printf(info->heap,
                                            "DECLARE FUNCTION my_func;\n"
                                            "DECLARE CURSOR c IS"
                                            " SELECT %s FROM $table_name"
                                            " WHERE %s = :doc_id;\n"
                                            "BEGIN\n"
                                            ""
                                            "OPEN c;\n"
                                            "WHILE 1 = 1 LOOP\n"
                                            "  FETCH c INTO my_func();\n"
                                            "  IF c %% NOTFOUND THEN\n"
                                            "    EXIT;\n"
                                            "  END IF;\n"
                                            "END LOOP;\n"
                                            "CLOSE c;",
                                            select_str, FTS_DOC_ID_COL_NAME));
    } else {
      ut_ad(option == FTS_FETCH_DOC_BY_ID_LARGE);

      /* This is used for crash recovery of table with
      hidden DOC ID or FTS indexes. We will scan the table
      to re-processing user table rows whose DOC ID or
      FTS indexed documents have not been sync-ed to disc
      during recent crash.
      In the case that all fulltext indexes are dropped
      for a table, we will keep the "hidden" FTS_DOC_ID
      column, and this scan is to retrieve the largest
      DOC ID being used in the table to determine the
      appropriate next DOC ID.
      In the case of there exists fulltext index(es), this
      operation will re-tokenize any docs that have not
      been sync-ed to the disk, and re-prime the FTS
      cached */
      graph = fts_parse_sql(nullptr, info,
                            mem_heap_printf(info->heap,
                                            "DECLARE FUNCTION my_func;\n"
                                            "DECLARE CURSOR c IS"
                                            " SELECT %s, %s FROM $table_name"
                                            " WHERE %s > :doc_id;\n"
                                            "BEGIN\n"
                                            ""
                                            "OPEN c;\n"
                                            "WHILE 1 = 1 LOOP\n"
                                            "  FETCH c INTO my_func();\n"
                                            "  IF c %% NOTFOUND THEN\n"
                                            "    EXIT;\n"
                                            "  END IF;\n"
                                            "END LOOP;\n"
                                            "CLOSE c;",
                                            FTS_DOC_ID_COL_NAME, select_str,
                                            FTS_DOC_ID_COL_NAME));
    }
    if (get_doc) {
      get_doc->get_document_graph = graph;
    }
  } else {
    graph = get_doc->get_document_graph;
  }

  error = fts_eval_sql(trx, graph);

  if (error == DB_SUCCESS) {
    fts_sql_commit(trx);
  } else {
    fts_sql_rollback(trx);
  }

  trx_free_for_background(trx);

  if (!get_doc) {
    fts_que_graph_free(graph);
  }

  return (error);
}

/** Write out a single word's data as new entry/entries in the INDEX table.
 @return DB_SUCCESS if all OK. */
dberr_t fts_write_node(trx_t *trx,             /*!< in: transaction */
                       que_t **graph,          /*!< in: query graph */
                       fts_table_t *fts_table, /*!< in: aux table */
                       fts_string_t *word,     /*!< in: word in UTF-8 */
                       fts_node_t *node)       /*!< in: node columns */
{
  pars_info_t *info;
  dberr_t error;
  uint32_t doc_count;
  doc_id_t last_doc_id;
  doc_id_t first_doc_id;
  char table_name[MAX_FULL_NAME_LEN];

  ut_a(node->ilist != nullptr);

  if (*graph) {
    info = (*graph)->info;
  } else {
    info = pars_info_create();

    fts_get_table_name(fts_table, table_name);
    pars_info_bind_id(info, true, "index_table_name", table_name);
  }

  pars_info_bind_varchar_literal(info, "token", word->f_str, word->f_len);

  /* Convert to "storage" byte order. */
  fts_write_doc_id((byte *)&first_doc_id, node->first_doc_id);
  fts_bind_doc_id(info, "first_doc_id", &first_doc_id);

  /* Convert to "storage" byte order. */
  fts_write_doc_id((byte *)&last_doc_id, node->last_doc_id);
  fts_bind_doc_id(info, "last_doc_id", &last_doc_id);

  ut_a(node->last_doc_id >= node->first_doc_id);

  /* Convert to "storage" byte order. */
  mach_write_to_4((byte *)&doc_count, node->doc_count);
  pars_info_bind_int4_literal(info, "doc_count", (const uint32_t *)&doc_count);

  /* Set copy_name to false since it's a static. */
  pars_info_bind_literal(info, "ilist", node->ilist, node->ilist_size,
                         DATA_BLOB, DATA_BINARY_TYPE);

  if (!*graph) {
    *graph = fts_parse_sql(fts_table, info,
                           "BEGIN\n"
                           "INSERT INTO $index_table_name VALUES"
                           " (:token, :first_doc_id,"
                           "  :last_doc_id, :doc_count, :ilist);");
  }

  const auto start_time = std::chrono::steady_clock::now();
  error = fts_eval_sql(trx, *graph);
  elapsed_time += std::chrono::steady_clock::now() - start_time;
  ++n_nodes;

  return (error);
}

/** Add rows to the DELETED_CACHE table.
 @return DB_SUCCESS if all went well else error code*/
[[nodiscard]] static dberr_t fts_sync_add_deleted_cache(
    fts_sync_t *sync,     /*!< in: sync state */
    ib_vector_t *doc_ids) /*!< in: doc ids to add */
{
  ulint i;
  pars_info_t *info;
  que_t *graph;
  fts_table_t fts_table;
  char table_name[MAX_FULL_NAME_LEN];
  doc_id_t dummy = 0;
  dberr_t error = DB_SUCCESS;
  ulint n_elems = ib_vector_size(doc_ids);

  ut_a(ib_vector_size(doc_ids) > 0);

  ib_vector_sort(doc_ids, fts_update_doc_id_cmp);

  info = pars_info_create();

  fts_bind_doc_id(info, "doc_id", &dummy);

  FTS_INIT_FTS_TABLE(&fts_table, FTS_SUFFIX_DELETED_CACHE, FTS_COMMON_TABLE,
                     sync->table);

  fts_get_table_name(&fts_table, table_name);
  pars_info_bind_id(info, true, "table_name", table_name);

  graph = fts_parse_sql(&fts_table, info,
                        "BEGIN INSERT INTO $table_name VALUES (:doc_id);");

  for (i = 0; i < n_elems && error == DB_SUCCESS; ++i) {
    fts_update_t *update;
    doc_id_t write_doc_id;

    update = static_cast<fts_update_t *>(ib_vector_get(doc_ids, i));

    /* Convert to "storage" byte order. */
    fts_write_doc_id((byte *)&write_doc_id, update->doc_id);
    fts_bind_doc_id(info, "doc_id", &write_doc_id);

    error = fts_eval_sql(sync->trx, graph);
  }

  fts_que_graph_free(graph);

  return (error);
}

/** Write the words and ilist to disk.
@param[in,out]  trx             transaction
@param[in]      index_cache     index cache
@param[in]      unlock_cache    whether unlock cache when write node
@param[in]      sync_start_time Holds the timestamp of start of sync
                                for deducing the length of sync time
@return DB_SUCCESS if all went well else error code */
[[nodiscard]] static MY_ATTRIBUTE((nonnull)) dberr_t fts_sync_write_words(
    trx_t *trx, fts_index_cache_t *index_cache, bool unlock_cache,
    std::chrono::steady_clock::time_point sync_start_time) {
  fts_table_t fts_table;
  ulint n_nodes = 0;
  ulint n_words = 0;
  const ib_rbt_node_t *rbt_node;
  dberr_t error = DB_SUCCESS;
  bool print_error = false;
  dict_table_t *table = index_cache->index->table;
  const float cutoff = 0.98f;
  auto lock_threshold = get_srv_fatal_semaphore_wait_threshold() * cutoff;

  bool timeout_extended = false;

  FTS_INIT_INDEX_TABLE(&fts_table, nullptr, FTS_INDEX_TABLE,
                       index_cache->index);

  n_words = rbt_size(index_cache->words);

  /* We iterate over the entire tree, even if there is an error,
  since we want to free the memory used during caching. */
  for (rbt_node = rbt_first(index_cache->words); rbt_node;
       rbt_node = rbt_next(index_cache->words, rbt_node)) {
    ulint i;
    ulint selected;
    fts_tokenizer_word_t *word;

    word = rbt_value(fts_tokenizer_word_t, rbt_node);

    selected = fts_select_index(index_cache->charset, word->text.f_str,
                                word->text.f_len);

    fts_table.suffix = fts_get_suffix(selected);

    /* We iterate over all the nodes even if there was an error */
    for (i = 0; i < ib_vector_size(word->nodes); ++i) {
      fts_node_t *fts_node =
          static_cast<fts_node_t *>(ib_vector_get(word->nodes, i));

      if (fts_node->synced) {
        continue;
      } else {
        fts_node->synced = true;
      }

      /*FIXME: we need to handle the error properly. */
      if (error == DB_SUCCESS) {
        DBUG_EXECUTE_IF("fts_instrument_sync_write",
                        std::this_thread::sleep_for(std::chrono::seconds(10)););
        if (!unlock_cache) {
          const auto cache_lock_time =
              std::chrono::steady_clock::now() - sync_start_time;
          if (cache_lock_time > lock_threshold) {
            if (!timeout_extended) {
              srv_fatal_semaphore_wait_extend.fetch_add(1);
              timeout_extended = true;
              lock_threshold += std::chrono::hours{2};
            } else {
              unlock_cache = true;
              srv_fatal_semaphore_wait_extend.fetch_sub(1);
              timeout_extended = false;
            }
          }
        }

        if (unlock_cache) {
          rw_lock_x_unlock(&table->fts->cache->lock);
        }

        error = fts_write_node(trx, &index_cache->ins_graph[selected],
                               &fts_table, &word->text, fts_node);

        DBUG_EXECUTE_IF("fts_instrument_sync_write",
                        std::this_thread::sleep_for(std::chrono::seconds(10)););

        DEBUG_SYNC_C("fts_write_node");
        DBUG_EXECUTE_IF("fts_write_node_crash", DBUG_SUICIDE(););

        DBUG_EXECUTE_IF("fts_instrument_sync_sleep",
                        std::this_thread::sleep_for(std::chrono::seconds(1)););

        if (unlock_cache) {
          rw_lock_x_lock(&table->fts->cache->lock, UT_LOCATION_HERE);
        }
      }
    }

    n_nodes += ib_vector_size(word->nodes);

    if (error != DB_SUCCESS && !print_error) {
      ib::error(ER_IB_MSG_473) << "(" << ut_strerr(error)
                               << ") writing"
                                  " word node to FTS auxiliary index table.";
      print_error = true;
    }
  }

  if (fts_enable_diag_print) {
    printf("Avg number of nodes: %lf\n",
           (double)n_nodes / (double)(n_words > 1 ? n_words : 1));
  }

  if (timeout_extended) {
    srv_fatal_semaphore_wait_extend.fetch_sub(1);
  }

  return (error);
}

/** Begin Sync, create transaction, acquire locks, etc. */
static void fts_sync_begin(fts_sync_t *sync) /*!< in: sync state */
{
  fts_cache_t *cache = sync->table->fts->cache;

  n_nodes = 0;
  elapsed_time = std::chrono::seconds::zero();

  sync->start_time = std::chrono::steady_clock::now();

  sync->trx = trx_allocate_for_background();

  if (fts_enable_diag_print) {
    ib::info(ER_IB_MSG_474)
        << "FTS SYNC for table " << sync->table->name
        << ", deleted count: " << ib_vector_size(cache->deleted_doc_ids)
        << " size: " << cache->total_size << " bytes";
  }
}

/** Run SYNC on the table, i.e., write out data from the index specific
 cache to the FTS aux INDEX table and FTS aux doc id stats table.
 @return DB_SUCCESS if all OK */
[[nodiscard]] static dberr_t fts_sync_index(
    fts_sync_t *sync,               /*!< in: sync state */
    fts_index_cache_t *index_cache) /*!< in: index cache */
{
  trx_t *trx = sync->trx;

  trx->op_info = "doing SYNC index";

  if (fts_enable_diag_print) {
    ib::info(ER_IB_MSG_475) << "SYNC words: " << rbt_size(index_cache->words);
  }

  ut_ad(rbt_validate(index_cache->words));

  return (fts_sync_write_words(trx, index_cache, sync->unlock_cache,
                               sync->start_time));
}

/** Check if index cache has been synced completely
@param[in,out]  index_cache     index cache
@return true if index is synced, otherwise false. */
static bool fts_sync_index_check(fts_index_cache_t *index_cache) {
  const ib_rbt_node_t *rbt_node;

  for (rbt_node = rbt_first(index_cache->words); rbt_node != nullptr;
       rbt_node = rbt_next(index_cache->words, rbt_node)) {
    fts_tokenizer_word_t *word;
    word = rbt_value(fts_tokenizer_word_t, rbt_node);

    fts_node_t *fts_node;
    fts_node = static_cast<fts_node_t *>(ib_vector_last(word->nodes));

    if (!fts_node->synced) {
      return (false);
    }
  }

  return (true);
}

/** Reset synced flag in index cache when rollback
@param[in,out]  index_cache     index cache */
static void fts_sync_index_reset(fts_index_cache_t *index_cache) {
  const ib_rbt_node_t *rbt_node;

  for (rbt_node = rbt_first(index_cache->words); rbt_node != nullptr;
       rbt_node = rbt_next(index_cache->words, rbt_node)) {
    fts_tokenizer_word_t *word;
    word = rbt_value(fts_tokenizer_word_t, rbt_node);

    fts_node_t *fts_node;
    fts_node = static_cast<fts_node_t *>(ib_vector_last(word->nodes));

    fts_node->synced = false;
  }
}

/** Commit the SYNC, change state of processed doc ids etc.
@param[in,out]  sync    sync state
@return DB_SUCCESS if all OK */
[[nodiscard]] static dberr_t fts_sync_commit(fts_sync_t *sync) {
  dberr_t error;
  trx_t *trx = sync->trx;
  fts_cache_t *cache = sync->table->fts->cache;
  doc_id_t last_doc_id;

  trx->op_info = "doing SYNC commit";

  /* After each Sync, update the CONFIG table about the max doc id
  we just sync-ed to index table */
  error = fts_cmp_set_sync_doc_id(sync->table, sync->max_doc_id, false,
                                  &last_doc_id);

  /* Get the list of deleted documents that are either in the
  cache or were headed there but were deleted before the add
  thread got to them. */

  if (error == DB_SUCCESS && ib_vector_size(cache->deleted_doc_ids) > 0) {
    error = fts_sync_add_deleted_cache(sync, cache->deleted_doc_ids);
  }

  /* We need to do this within the deleted lock since fts_delete() can
  attempt to add a deleted doc id to the cache deleted id array. */
  fts_cache_clear(cache);
  DEBUG_SYNC_C("fts_deleted_doc_ids_clear");
  fts_cache_init(cache);
  rw_lock_x_unlock(&cache->lock);

  if (error == DB_SUCCESS) {
    fts_sql_commit(trx);

  } else if (error != DB_SUCCESS) {
    fts_sql_rollback(trx);

    ib::error(ER_IB_MSG_476) << "(" << ut_strerr(error) << ") during SYNC.";
  }

  if (fts_enable_diag_print && elapsed_time != std::chrono::seconds::zero()) {
    ib::info(ER_IB_MSG_477)
        << "SYNC for table " << sync->table->name << ": SYNC time: "
        << std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::steady_clock::now() - sync->start_time)
               .count()
        << " secs: elapsed "
        << n_nodes / std::chrono::duration_cast<std::chrono::duration<double>>(
                         elapsed_time)
                         .count()
        << " ins/sec";
  }

  /* Avoid assertion in trx_free(). */
  trx->dict_operation_lock_mode = 0;
  trx_free_for_background(trx);

  return (error);
}

/** Rollback a sync operation */
static void fts_sync_rollback(fts_sync_t *sync) /*!< in: sync state */
{
  trx_t *trx = sync->trx;
  fts_cache_t *cache = sync->table->fts->cache;

  for (ulint i = 0; i < ib_vector_size(cache->indexes); ++i) {
    ulint j;
    fts_index_cache_t *index_cache;

    index_cache =
        static_cast<fts_index_cache_t *>(ib_vector_get(cache->indexes, i));

    /* Reset synced flag so nodes will not be skipped
    in the next sync, see fts_sync_write_words(). */
    fts_sync_index_reset(index_cache);

    for (j = 0; fts_index_selector[j].value; ++j) {
      if (index_cache->ins_graph[j] != nullptr) {
        que_graph_free(index_cache->ins_graph[j]);

        index_cache->ins_graph[j] = nullptr;
      }

      if (index_cache->sel_graph[j] != nullptr) {
        que_graph_free(index_cache->sel_graph[j]);

        index_cache->sel_graph[j] = nullptr;
      }
    }
  }

  rw_lock_x_unlock(&cache->lock);

  fts_sql_rollback(trx);

  /* Avoid assertion in trx_free(). */
  trx->dict_operation_lock_mode = 0;
  trx_free_for_background(trx);
}

/** Run SYNC on the table, i.e., write out data from the cache to the
FTS auxiliary INDEX table and clear the cache at the end.
@param[in,out]  sync            sync state
@param[in]      unlock_cache    whether unlock cache lock when write node
@param[in]      wait            whether wait when a sync is in progress
@param[in]      has_dict        whether has dict operation lock
@return DB_SUCCESS if all OK */
static dberr_t fts_sync(fts_sync_t *sync, bool unlock_cache, bool wait,
                        bool has_dict) {
  ulint i;
  dberr_t error = DB_SUCCESS;
  fts_cache_t *cache = sync->table->fts->cache;

  rw_lock_x_lock(&cache->lock, UT_LOCATION_HERE);

  /* Check if cache is being synced.
  Note: we release cache lock in fts_sync_write_words() to
  avoid long wait for the lock by other threads. */
  while (sync->in_progress) {
    rw_lock_x_unlock(&cache->lock);

    if (wait) {
      os_event_wait(sync->event);
    } else {
      return (DB_SUCCESS);
    }

    rw_lock_x_lock(&cache->lock, UT_LOCATION_HERE);
  }
  sync->unlock_cache = unlock_cache;
  sync->in_progress = true;

  DEBUG_SYNC_C("fts_sync_begin");
  fts_sync_begin(sync);

  /* When sync in background, we hold dict operation lock
  to prevent DDL like DROP INDEX, etc. */
  if (has_dict) {
    sync->trx->dict_operation_lock_mode = RW_S_LATCH;
  }

begin_sync:
  if (cache->total_size > fts_max_cache_size) {
    /* Avoid the case: sync never finish when
    insert/update keeps coming. */
    ut_ad(sync->unlock_cache);
    sync->unlock_cache = false;
  }

  DEBUG_SYNC_C("fts_instrument_sync1");
  for (i = 0; i < ib_vector_size(cache->indexes); ++i) {
    fts_index_cache_t *index_cache;

    index_cache =
        static_cast<fts_index_cache_t *>(ib_vector_get(cache->indexes, i));

    error = fts_sync_index(sync, index_cache);

    if (error != DB_SUCCESS && !sync->interrupted) {
      goto end_sync;
    }
  }

  DBUG_EXECUTE_IF("fts_instrument_sync_interrupted", sync->interrupted = true;
                  error = DB_INTERRUPTED; goto end_sync;);

  /* Make sure all the caches are synced. */
  for (i = 0; i < ib_vector_size(cache->indexes); ++i) {
    fts_index_cache_t *index_cache;

    index_cache =
        static_cast<fts_index_cache_t *>(ib_vector_get(cache->indexes, i));

    if (index_cache->index->to_be_dropped ||
        fts_sync_index_check(index_cache)) {
      continue;
    }

    goto begin_sync;
  }

end_sync:
  if (error == DB_SUCCESS && !sync->interrupted) {
    error = fts_sync_commit(sync);
  } else {
    fts_sync_rollback(sync);
  }

  rw_lock_x_lock(&cache->lock, UT_LOCATION_HERE);
  sync->interrupted = false;
  sync->in_progress = false;
  os_event_set(sync->event);
  rw_lock_x_unlock(&cache->lock);

  /* We need to check whether an optimize is required, for that
  we make copies of the two variables that control the trigger. These
  variables can change behind our back and we don't want to hold the
  lock for longer than is needed. */
  mutex_enter(&cache->deleted_lock);

  cache->added = 0;
  cache->deleted = 0;

  mutex_exit(&cache->deleted_lock);

  return (error);
}

/** Run SYNC on the table, i.e., write out data from the cache to the
FTS auxiliary INDEX table and clear the cache at the end.
@param[in,out]  table           fts table
@param[in]      unlock_cache    whether unlock cache when write node
@param[in]      wait            whether wait for existing sync to finish
@param[in]      has_dict        whether has dict operation lock
@return DB_SUCCESS on success, error code on failure. */
dberr_t fts_sync_table(dict_table_t *table, bool unlock_cache, bool wait,
                       bool has_dict) {
  dberr_t err = DB_SUCCESS;

  ut_ad(table->fts);

  if (!dict_table_is_discarded(table) && table->fts->cache &&
      !table->is_corrupted()) {
    err = fts_sync(table->fts->cache->sync, unlock_cache, wait, has_dict);
  }

  return (err);
}

/** Check fts token
1. for ngram token, check whether the token contains any words in stopwords
2. for non-ngram token, check if it's stopword or less than fts_min_token_size
or greater than fts_max_token_size.
@param[in]      token           token string
@param[in]      stopwords       stopwords rb tree
@param[in]      is_ngram        is ngram parser
@param[in]      cs              token charset
@retval true    if it is not stopword and length in range
@retval false   if it is stopword or length not in range */
bool fts_check_token(const fts_string_t *token, const ib_rbt_t *stopwords,
                     bool is_ngram, const CHARSET_INFO *cs) {
  ut_ad(cs != nullptr || stopwords == nullptr);

  if (!is_ngram) {
    ib_rbt_bound_t parent;

    if (token->f_n_char < fts_min_token_size ||
        token->f_n_char > fts_max_token_size ||
        (stopwords != nullptr && rbt_search(stopwords, &parent, token) == 0)) {
      return (false);
    } else {
      return (true);
    }
  }

  /* Check token for ngram. */
  DBUG_EXECUTE_IF("fts_instrument_ignore_ngram_check", return (true););

  /* We ignore fts_min_token_size when ngram */
  ut_ad(token->f_n_char > 0 && token->f_n_char <= fts_max_token_size);

  if (stopwords == nullptr) {
    return (true);
  }

  /*Ngram checks whether the token contains any words in stopwords.
  We can't simply use CONTAIN to search in stopwords, because it's
  built on COMPARE. So we need to tokenize the token into words
  from unigram to f_n_char, and check them separately. */
  for (ulint ngram_token_size = 1; ngram_token_size <= token->f_n_char;
       ngram_token_size++) {
    const char *start;
    const char *next;
    const char *end;
    ulint char_len;
    ulint n_chars;

    start = reinterpret_cast<char *>(token->f_str);
    next = start;
    end = start + token->f_len;
    n_chars = 0;

    while (next < end) {
      char_len = my_mbcharlen_ptr(cs, next, end);

      if (next + char_len > end || char_len == 0) {
        break;
      } else {
        /* Skip SPACE */
        if (char_len == 1 && *next == ' ') {
          start = next + 1;
          next = start;
          n_chars = 0;

          continue;
        }

        next += char_len;
        n_chars++;
      }

      if (n_chars == ngram_token_size) {
        fts_string_t ngram_token;
        ngram_token.f_str = reinterpret_cast<byte *>(const_cast<char *>(start));
        ngram_token.f_len = next - start;
        ngram_token.f_n_char = ngram_token_size;

        ib_rbt_bound_t parent;
        if (rbt_search(stopwords, &parent, &ngram_token) == 0) {
          return (false);
        }

        /* Move a char forward */
        start += my_mbcharlen_ptr(cs, start, end);
        n_chars = ngram_token_size - 1;
      }
    }
  }

  return (true);
}

/** Add the token and its start position to the token's list of positions.
@param[in,out]  result_doc      result doc rb tree
@param[in]      str             token string
@param[in]      position        token position */
static void fts_add_token(fts_doc_t *result_doc, fts_string_t str,
                          ulint position) {
  /* Ignore string whose character number is less than
  "fts_min_token_size" or more than "fts_max_token_size" */

  if (fts_check_token(&str, nullptr, result_doc->is_ngram,
                      result_doc->charset)) {
    mem_heap_t *heap;
    fts_string_t t_str;
    fts_token_t *token;
    ib_rbt_bound_t parent;
    ulint newlen;

    heap = static_cast<mem_heap_t *>(result_doc->self_heap->arg);

    t_str.f_n_char = str.f_n_char;

    t_str.f_len = str.f_len * result_doc->charset->casedn_multiply + 1;

    t_str.f_str = static_cast<byte *>(mem_heap_alloc(heap, t_str.f_len));

    /* For binary collations, a case sensitive search is
    performed. Hence don't convert to lower case. */
    if (my_binary_compare(result_doc->charset)) {
      memcpy(t_str.f_str, str.f_str, str.f_len);
      t_str.f_str[str.f_len] = 0;
      newlen = str.f_len;
    } else {
      newlen =
          innobase_fts_casedn_str(result_doc->charset, (char *)str.f_str,
                                  str.f_len, (char *)t_str.f_str, t_str.f_len);
    }

    t_str.f_len = newlen;
    t_str.f_str[newlen] = 0;

    /* Add the word to the document statistics. If the word
    hasn't been seen before we create a new entry for it. */
    if (rbt_search(result_doc->tokens, &parent, &t_str) != 0) {
      fts_token_t new_token;

      new_token.text.f_len = newlen;
      new_token.text.f_str = t_str.f_str;
      new_token.text.f_n_char = t_str.f_n_char;

      new_token.positions =
          ib_vector_create(result_doc->self_heap, sizeof(ulint), 32);

      parent.last = rbt_add_node(result_doc->tokens, &parent, &new_token);

      ut_ad(rbt_validate(result_doc->tokens));
    }

    token = rbt_value(fts_token_t, parent.last);
    ib_vector_push(token->positions, &position);
  }
}

/** Process next token from document starting at the given position, i.e., add
the token's start position to the token's list of positions.
@param[in,out]  doc             document to tokenize
@param[out]     result          if provided, save result here
@param[in]      start_pos       start position in text
@param[in]      add_pos         add this position to all tokens from this
                                tokenization
@return number of characters handled in this call */
static ulint fts_process_token(fts_doc_t *doc, fts_doc_t *result,
                               ulint start_pos, ulint add_pos) {
  ulint ret;
  fts_string_t str;
  ulint position;
  fts_doc_t *result_doc;
  byte buf[FTS_MAX_WORD_LEN + 1];

  str.f_str = buf;

  /* Determine where to save the result. */
  result_doc = (result != nullptr) ? result : doc;

  /* The length of a string in characters is set here only. */

  ret = innobase_mysql_fts_get_token(doc->charset, doc->text.f_str + start_pos,
                                     doc->text.f_str + doc->text.f_len, &str);

  position = start_pos + ret - str.f_len + add_pos;

  fts_add_token(result_doc, str, position);

  return (ret);
}

/** Get token char size by charset
 @return token size */
ulint fts_get_token_size(const CHARSET_INFO *cs, /*!< in: Character set */
                         const char *token,      /*!< in: token */
                         ulint len)              /*!< in: token length */
{
  char *start;
  char *end;
  ulint size = 0;

  /* const_cast is for reinterpret_cast below, or it will fail. */
  start = const_cast<char *>(token);
  end = start + len;
  while (start < end) {
    int ctype;
    int mbl;

    mbl = cs->cset->ctype(cs, &ctype, reinterpret_cast<uchar *>(start),
                          reinterpret_cast<uchar *>(end));

    size++;

    start += mbl > 0 ? mbl : (mbl < 0 ? -mbl : 1);
  }

  return (size);
}

/** FTS plugin parser 'myql_parser' callback function for document tokenize.
 Refer to 'MYSQL_FTPARSER_PARAM' for more detail.
 @return always returns 0 */
int fts_tokenize_document_internal(
    MYSQL_FTPARSER_PARAM *param, /*!< in: parser parameter */
    char *doc,                   /*!< in/out: document */
    int len)                     /*!< in: document length */
{
  fts_string_t str;
  byte buf[FTS_MAX_WORD_LEN + 1];
  MYSQL_FTPARSER_BOOLEAN_INFO bool_info = {FT_TOKEN_WORD, 0,      0, 0, 0, 0,
                                           ' ',           nullptr};

  ut_ad(len >= 0);

  str.f_str = buf;

  for (ulint i = 0, inc = 0; i < static_cast<ulint>(len); i += inc) {
    inc =
        innobase_mysql_fts_get_token(const_cast<CHARSET_INFO *>(param->cs),
                                     reinterpret_cast<byte *>(doc) + i,
                                     reinterpret_cast<byte *>(doc) + len, &str);

    if (str.f_len > 0) {
      bool_info.position = static_cast<int>(i + inc - str.f_len);
      ut_ad(bool_info.position >= 0);

      /* Stop when add word fails */
      if (param->mysql_add_word(param, reinterpret_cast<char *>(str.f_str),
                                static_cast<int>(str.f_len), &bool_info)) {
        break;
      }
    }
  }

  return (0);
}

/** FTS plugin parser 'myql_add_word' callback function for document tokenize.
 Refer to 'MYSQL_FTPARSER_PARAM' for more detail.
 @return always returns 0 */
static int fts_tokenize_add_word_for_parser(
    MYSQL_FTPARSER_PARAM *param,               /* in: parser parameter */
    char *word,                                /* in: token word */
    int word_len,                              /* in: word len */
    MYSQL_FTPARSER_BOOLEAN_INFO *boolean_info) /* in: word boolean info */
{
  fts_string_t str;
  fts_tokenize_param_t *fts_param;
  fts_doc_t *result_doc;
  ulint position;

  fts_param = static_cast<fts_tokenize_param_t *>(param->mysql_ftparam);
  result_doc = fts_param->result_doc;
  ut_ad(result_doc != nullptr);

  str.f_str = reinterpret_cast<byte *>(word);
  str.f_len = word_len;
  str.f_n_char =
      fts_get_token_size(const_cast<CHARSET_INFO *>(param->cs), word, word_len);

  ut_ad(boolean_info->position >= 0);
  position = boolean_info->position + fts_param->add_pos;

  fts_add_token(result_doc, str, position);

  return (0);
}

/** Parse a document using an external / user supplied parser */
static void fts_tokenize_by_parser(
    fts_doc_t *doc,                  /* in/out: document to tokenize */
    st_mysql_ftparser *parser,       /* in: plugin fts parser */
    fts_tokenize_param_t *fts_param) /* in: fts tokenize param */
{
  MYSQL_FTPARSER_PARAM param;

  ut_a(parser);

  /* Set parameters for param */
  param.mysql_parse = fts_tokenize_document_internal;
  param.mysql_add_word = fts_tokenize_add_word_for_parser;
  param.mysql_ftparam = fts_param;
  param.cs = doc->charset;
  param.doc = reinterpret_cast<char *>(doc->text.f_str);
  param.length = static_cast<int>(doc->text.f_len);
  param.mode = MYSQL_FTPARSER_SIMPLE_MODE;

  PARSER_INIT(parser, &param);
  parser->parse(&param);
  PARSER_DEINIT(parser, &param);
}

/** Tokenize a document.
@param[in,out]  doc     document to tokenize
@param[out]     result  tokenization result
@param[in]      parser  pluggable parser */
static void fts_tokenize_document(fts_doc_t *doc, fts_doc_t *result,
                                  st_mysql_ftparser *parser) {
  ut_a(!doc->tokens);
  ut_a(doc->charset);

  doc->tokens = rbt_create_arg_cmp(sizeof(fts_token_t), innobase_fts_text_cmp,
                                   doc->charset);

  if (parser != nullptr) {
    fts_tokenize_param_t fts_param;

    fts_param.result_doc = (result != nullptr) ? result : doc;
    fts_param.add_pos = 0;

    fts_tokenize_by_parser(doc, parser, &fts_param);
  } else {
    ulint inc;

    for (ulint i = 0; i < doc->text.f_len; i += inc) {
      inc = fts_process_token(doc, result, i, 0);
      ut_a(inc > 0);
    }
  }
}

/** Continue to tokenize a document.
@param[in,out]  doc     document to tokenize
@param[in]      add_pos add this position to all tokens from this tokenization
@param[out]     result  tokenization result
@param[in]      parser  pluggable parser */
static void fts_tokenize_document_next(fts_doc_t *doc, ulint add_pos,
                                       fts_doc_t *result,
                                       st_mysql_ftparser *parser) {
  ut_a(doc->tokens);

  if (parser) {
    fts_tokenize_param_t fts_param;

    fts_param.result_doc = (result != nullptr) ? result : doc;
    fts_param.add_pos = add_pos;

    fts_tokenize_by_parser(doc, parser, &fts_param);
  } else {
    ulint inc;

    for (ulint i = 0; i < doc->text.f_len; i += inc) {
      inc = fts_process_token(doc, result, i, add_pos);
      ut_a(inc > 0);
    }
  }
}

/** Create the vector of fts_get_doc_t instances.
@param[in,out]  cache   fts cache
@return vector of fts_get_doc_t instances */
static ib_vector_t *fts_get_docs_create(fts_cache_t *cache) {
  ib_vector_t *get_docs;

  ut_ad(rw_lock_own(&cache->init_lock, RW_LOCK_X));

  /* We need one instance of fts_get_doc_t per index. */
  get_docs = ib_vector_create(cache->self_heap, sizeof(fts_get_doc_t), 4);

  /* Create the get_doc instance, we need one of these
  per FTS index. */
  for (ulint i = 0; i < ib_vector_size(cache->indexes); ++i) {
    dict_index_t **index;
    fts_get_doc_t *get_doc;

    index = static_cast<dict_index_t **>(ib_vector_get(cache->indexes, i));

    get_doc = static_cast<fts_get_doc_t *>(ib_vector_push(get_docs, nullptr));

    memset(get_doc, 0x0, sizeof(*get_doc));

    get_doc->index_cache = fts_get_index_cache(cache, *index);
    get_doc->cache = cache;

    /* Must find the index cache. */
    ut_a(get_doc->index_cache != nullptr);
  }

  return (get_docs);
}

/********************************************************************
Release any resources held by the fts_get_doc_t instances. */
static void fts_get_docs_clear(
    ib_vector_t *get_docs) /*!< in: Doc retrieval vector */
{
  ulint i;

  /* Release the get doc graphs if any. */
  for (i = 0; i < ib_vector_size(get_docs); ++i) {
    fts_get_doc_t *get_doc =
        static_cast<fts_get_doc_t *>(ib_vector_get(get_docs, i));

    if (get_doc->get_document_graph != nullptr) {
      ut_a(get_doc->index_cache);

      fts_que_graph_free(get_doc->get_document_graph);
      get_doc->get_document_graph = nullptr;
    }
  }
}

/** Get the initial Doc ID by consulting the CONFIG table
 @return initial Doc ID */
doc_id_t fts_init_doc_id(const dict_table_t *table) /*!< in: table */
{
  doc_id_t max_doc_id = 0;

  rw_lock_x_lock(&table->fts->cache->lock, UT_LOCATION_HERE);

  /* Return if the table is already initialized for DOC ID */
  if (table->fts->cache->first_doc_id != FTS_NULL_DOC_ID) {
    rw_lock_x_unlock(&table->fts->cache->lock);
    return (0);
  }

  DEBUG_SYNC_C("fts_initialize_doc_id");

  /* Then compare this value with the ID value stored in the CONFIG
  table. The larger one will be our new initial Doc ID */
  fts_cmp_set_sync_doc_id(table, 0, false, &max_doc_id);

  /* If DICT_TF2_FTS_ADD_DOC_ID is set, we are in the process of
  creating index (and add doc id column. No need to recovery
  documents */
  if (!DICT_TF2_FLAG_IS_SET(table, DICT_TF2_FTS_ADD_DOC_ID)) {
    fts_init_index((dict_table_t *)table, true);
  }

  table->fts->fts_status |= ADDED_TABLE_SYNCED;

  table->fts->cache->first_doc_id = max_doc_id;

  rw_lock_x_unlock(&table->fts->cache->lock);

  ut_ad(max_doc_id > 0);

  return (max_doc_id);
}

#ifdef FTS_MULT_INDEX
/** Check if the index is in the affected set.
 @return true if index is updated */
static bool fts_is_index_updated(
    const ib_vector_t *fts_indexes, /*!< in: affected FTS indexes */
    const fts_get_doc_t *get_doc)   /*!< in: info for reading
                                    document */
{
  ulint i;
  dict_index_t *index = get_doc->index_cache->index;

  for (i = 0; i < ib_vector_size(fts_indexes); ++i) {
    const dict_index_t *updated_fts_index;

    updated_fts_index =
        static_cast<const dict_index_t *>(ib_vector_getp_const(fts_indexes, i));

    ut_a(updated_fts_index != NULL);

    if (updated_fts_index == index) {
      return true;
    }
  }

  return false;
}
#endif

/** Fetch COUNT(*) from specified table.
 @return the number of rows in the table */
ulint fts_get_rows_count(fts_table_t *fts_table) /*!< in: fts table to read */
{
  trx_t *trx;
  pars_info_t *info;
  que_t *graph;
  dberr_t error;
  ulint count = 0;
  char table_name[MAX_FULL_NAME_LEN];

  trx = trx_allocate_for_background();

  trx->op_info = "fetching FT table rows count";

  info = pars_info_create();

  pars_info_bind_function(info, "my_func", fts_read_ulint, &count);

  fts_get_table_name(fts_table, table_name);
  pars_info_bind_id(info, true, "table_name", table_name);

  graph = fts_parse_sql(fts_table, info,
                        "DECLARE FUNCTION my_func;\n"
                        "DECLARE CURSOR c IS"
                        " SELECT COUNT(*)"
                        " FROM $table_name;\n"
                        "BEGIN\n"
                        "\n"
                        "OPEN c;\n"
                        "WHILE 1 = 1 LOOP\n"
                        "  FETCH c INTO my_func();\n"
                        "  IF c % NOTFOUND THEN\n"
                        "    EXIT;\n"
                        "  END IF;\n"
                        "END LOOP;\n"
                        "CLOSE c;");

  for (;;) {
    error = fts_eval_sql(trx, graph);

    if (error == DB_SUCCESS) {
      fts_sql_commit(trx);

      break; /* Exit the loop. */
    } else {
      fts_sql_rollback(trx);

      if (error == DB_LOCK_WAIT_TIMEOUT) {
        ib::warn(ER_IB_MSG_478) << "lock wait timeout reading"
                                   " FTS table. Retrying!";

        trx->error_state = DB_SUCCESS;
      } else {
        ib::error(ER_IB_MSG_479)
            << "(" << ut_strerr(error) << ") while reading FTS table.";

        break; /* Exit the loop. */
      }
    }
  }

  fts_que_graph_free(graph);

  trx_free_for_background(trx);

  return (count);
}

#ifdef FTS_CACHE_SIZE_DEBUG
/** Read the max cache size parameter from the config table. */
static void fts_update_max_cache_size(fts_sync_t *sync) /*!< in: sync state */
{
  trx_t *trx;
  fts_table_t fts_table;

  trx = trx_allocate_for_background();

  FTS_INIT_FTS_TABLE(&fts_table, FTS_SUFFIX_CONFIG, FTS_COMMON_TABLE,
                     sync->table);

  /* The size returned is in bytes. */
  sync->max_cache_size = fts_get_max_cache_size(trx, &fts_table);

  fts_sql_commit(trx);

  trx_free_for_background(trx);
}
#endif /* FTS_CACHE_SIZE_DEBUG */

/** Free the modified rows of a table. */
static inline void fts_trx_table_rows_free(
    ib_rbt_t *rows) /*!< in: rbt of rows to free */
{
  const ib_rbt_node_t *node;

  for (node = rbt_first(rows); node; node = rbt_first(rows)) {
    fts_trx_row_t *row;

    row = rbt_value(fts_trx_row_t, node);

    if (row->fts_indexes != nullptr) {
      /* This vector shouldn't be using the
      heap allocator.  */
      ut_a(row->fts_indexes->allocator->arg == nullptr);

      ib_vector_free(row->fts_indexes);
      row->fts_indexes = nullptr;
    }

    ut::free(rbt_remove_node(rows, node));
  }

  ut_a(rbt_empty(rows));
  rbt_free(rows);
}

/** Free an FTS savepoint instance. */
static inline void fts_savepoint_free(
    fts_savepoint_t *savepoint) /*!< in: savepoint instance */
{
  const ib_rbt_node_t *node;
  ib_rbt_t *tables = savepoint->tables;

  /* Nothing to free! */
  if (tables == nullptr) {
    return;
  }

  for (node = rbt_first(tables); node; node = rbt_first(tables)) {
    fts_trx_table_t *ftt;
    fts_trx_table_t **fttp;

    fttp = rbt_value(fts_trx_table_t *, node);
    ftt = *fttp;

    /* This can be NULL if a savepoint was released. */
    if (ftt->rows != nullptr) {
      fts_trx_table_rows_free(ftt->rows);
      ftt->rows = nullptr;
    }

    /* This can be NULL if a savepoint was released. */
    if (ftt->added_doc_ids != nullptr) {
      fts_doc_ids_free(ftt->added_doc_ids);
      ftt->added_doc_ids = nullptr;
    }

    /* The default savepoint name must be NULL. */
    if (ftt->docs_added_graph) {
      fts_que_graph_free(ftt->docs_added_graph);
    }

    /* NOTE: We are responsible for free'ing the node */
    ut::free(rbt_remove_node(tables, node));
  }

  ut_a(rbt_empty(tables));
  rbt_free(tables);
  savepoint->tables = nullptr;
}

/** Free an FTS trx. */
void fts_trx_free(fts_trx_t *fts_trx) /* in, own: FTS trx */
{
  ulint i;

  for (i = 0; i < ib_vector_size(fts_trx->savepoints); ++i) {
    fts_savepoint_t *savepoint;

    savepoint =
        static_cast<fts_savepoint_t *>(ib_vector_get(fts_trx->savepoints, i));

    /* The default savepoint name must be NULL. */
    if (i == 0) {
      ut_a(savepoint->name == nullptr);
    }

    fts_savepoint_free(savepoint);
  }

  for (i = 0; i < ib_vector_size(fts_trx->last_stmt); ++i) {
    fts_savepoint_t *savepoint;

    savepoint =
        static_cast<fts_savepoint_t *>(ib_vector_get(fts_trx->last_stmt, i));

    /* The default savepoint name must be NULL. */
    if (i == 0) {
      ut_a(savepoint->name == nullptr);
    }

    fts_savepoint_free(savepoint);
  }

  if (fts_trx->heap) {
    mem_heap_free(fts_trx->heap);
  }
}

/** Extract the doc id from the FTS hidden column.
 @return doc id that was extracted from rec */
doc_id_t fts_get_doc_id_from_row(dict_table_t *table, /*!< in: table */
                                 dtuple_t *row) /*!< in: row whose FTS doc id we
                                                want to extract.*/
{
  dfield_t *field;
  doc_id_t doc_id = 0;

  ut_a(table->fts->doc_col != ULINT_UNDEFINED);

  field = dtuple_get_nth_field(row, table->fts->doc_col);

  ut_a(dfield_get_len(field) == sizeof(doc_id));
  ut_a(dfield_get_type(field)->mtype == DATA_INT);

  doc_id = fts_read_doc_id(static_cast<const byte *>(dfield_get_data(field)));

  return (doc_id);
}

/** Extract the doc id from the record that belongs to index.
@param[in]      table   table
@param[in]      rec     record contains FTS_DOC_ID
@param[in]      index   index of rec
@param[in]      heap    heap memory
@return doc id that was extracted from rec */
doc_id_t fts_get_doc_id_from_rec(dict_table_t *table, const rec_t *rec,
                                 const dict_index_t *index, mem_heap_t *heap) {
  ulint len;
  const byte *data;
  ulint col_no;
  doc_id_t doc_id = 0;
  ulint offsets_[REC_OFFS_NORMAL_SIZE];
  ulint *offsets = offsets_;
  mem_heap_t *my_heap = heap;

  ut_a(table->fts->doc_col != ULINT_UNDEFINED);

  rec_offs_init(offsets_);

  offsets = rec_get_offsets(rec, index, offsets, ULINT_UNDEFINED,
                            UT_LOCATION_HERE, &my_heap);

  col_no = index->get_col_pos(table->fts->doc_col);

  ut_ad(col_no != ULINT_UNDEFINED);

  data = rec_get_nth_field(nullptr, rec, offsets, col_no, &len);

  ut_a(len == 8);
  static_assert(8 == sizeof(doc_id));
  doc_id = static_cast<doc_id_t>(mach_read_from_8(data));

  if (my_heap && !heap) {
    mem_heap_free(my_heap);
  }

  return (doc_id);
}

/** Search the index specific cache for a particular FTS index.
 @return the index specific cache else NULL */
fts_index_cache_t *fts_find_index_cache(
    const fts_cache_t *cache,  /*!< in: cache to search */
    const dict_index_t *index) /*!< in: index to search for */
{
  /* We cast away the const because our internal function, takes
  non-const cache arg and returns a non-const pointer. */
  return (static_cast<fts_index_cache_t *>(
      fts_get_index_cache((fts_cache_t *)cache, index)));
}

/** Search cache for word.
 @return the word node vector if found else NULL */
const ib_vector_t *fts_cache_find_word(
    const fts_index_cache_t *index_cache, /*!< in: cache to search */
    const fts_string_t *text)             /*!< in: word to search for */
{
  ib_rbt_bound_t parent;
  const ib_vector_t *nodes = nullptr;
#ifdef UNIV_DEBUG
  dict_table_t *table = index_cache->index->table;
  fts_cache_t *cache = table->fts->cache;

  ut_ad(rw_lock_own(&cache->lock, RW_LOCK_X));
#endif /* UNIV_DEBUG */

  /* Lookup the word in the rb tree */
  if (rbt_search(index_cache->words, &parent, text) == 0) {
    const fts_tokenizer_word_t *word;

    word = rbt_value(fts_tokenizer_word_t, parent.last);

    nodes = word->nodes;
  }

  return (nodes);
}

/** Append deleted doc ids to vector. */
void fts_cache_append_deleted_doc_ids(
    const fts_cache_t *cache, /*!< in: cache to use */
    ib_vector_t *vector)      /*!< in: append to this vector */
{
  mutex_enter(const_cast<ib_mutex_t *>(&cache->deleted_lock));

  if (cache->deleted_doc_ids == nullptr) {
    mutex_exit((ib_mutex_t *)&cache->deleted_lock);
    return;
  }

  for (ulint i = 0; i < ib_vector_size(cache->deleted_doc_ids); ++i) {
    fts_update_t *update;

    update =
        static_cast<fts_update_t *>(ib_vector_get(cache->deleted_doc_ids, i));

    ib_vector_push(vector, &update->doc_id);
  }

  mutex_exit((ib_mutex_t *)&cache->deleted_lock);
}

bool fts_wait_for_background_thread_to_start(
    dict_table_t *table, std::chrono::microseconds max_wait) {
  ulint count = 0;
  bool done = false;

  ut_a(max_wait == std::chrono::seconds::zero() ||
       max_wait >= FTS_MAX_BACKGROUND_THREAD_WAIT);

  for (;;) {
    fts_t *fts = table->fts;

    mutex_enter(&fts->bg_threads_mutex);

    if (fts->fts_status & BG_THREAD_READY) {
      done = true;
    }

    mutex_exit(&fts->bg_threads_mutex);

    if (!done) {
      std::this_thread::sleep_for(FTS_MAX_BACKGROUND_THREAD_WAIT);

      if (max_wait > std::chrono::seconds::zero()) {
        max_wait -= FTS_MAX_BACKGROUND_THREAD_WAIT;

        /* We ignore the residual value. */
        if (max_wait < FTS_MAX_BACKGROUND_THREAD_WAIT) {
          break;
        }
      }

      ++count;
    } else {
      break;
    }

    if (count >= FTS_BACKGROUND_THREAD_WAIT_COUNT) {
      ib::error(ER_IB_MSG_480) << "The background thread for the FTS"
                                  " table "
                               << table->name << " refuses to start";

      count = 0;
    }
  }

  return (done);
}

/** Add the FTS document id hidden column.
@param[in,out] table Table with FTS index
@param[in] heap Temporary memory heap, or NULL
*/
void fts_add_doc_id_column(dict_table_t *table, mem_heap_t *heap) {
  dict_mem_table_add_col(
      table, heap, FTS_DOC_ID_COL_NAME, DATA_INT,
      dtype_form_prtype(
          DATA_NOT_NULL | DATA_UNSIGNED | DATA_BINARY_TYPE | DATA_FTS_DOC_ID,
          0),
      sizeof(doc_id_t), false);
  DICT_TF2_FLAG_SET(table, DICT_TF2_FTS_HAS_DOC_ID);
}

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
                           doc_id_t *next_doc_id) {
  doc_id_t doc_id;
  dberr_t error = DB_SUCCESS;

  if (*next_doc_id) {
    doc_id = *next_doc_id;
  } else {
    /* Get the new document id that will be added. */
    error = fts_get_next_doc_id(table, &doc_id);
  }

  if (error == DB_SUCCESS) {
    dict_index_t *clust_index;
    dict_col_t *col = table->get_col(table->fts->doc_col);

    ufield->exp = nullptr;

    ufield->new_val.len = sizeof(doc_id);

    clust_index = table->first_index();

    ufield->field_no = dict_col_get_clust_pos(col, clust_index);
    col->copy_type(dfield_get_type(&ufield->new_val));

    /* It is possible we update record that has
    not yet be sync-ed from last crash. */

    /* Convert to storage byte order. */
    ut_a(doc_id != FTS_NULL_DOC_ID);
    fts_write_doc_id((byte *)next_doc_id, doc_id);

    ufield->new_val.data = next_doc_id;
    ufield->new_val.ext = 0;
  }

  return (doc_id);
}

/** fts_t constructor.
@param[in]      table   table with FTS indexes
@param[in,out]  heap    memory heap where 'this' is stored */
fts_t::fts_t(dict_table_t *table, mem_heap_t *heap)
    : bg_threads(0),
      fts_status(0),
      add_wq(nullptr),
      cache(nullptr),
      doc_col(ULINT_UNDEFINED),
      fts_heap(heap) {
  ut_a(table->fts == nullptr);

  mutex_create(LATCH_ID_FTS_BG_THREADS, &bg_threads_mutex);

  ib_alloc_t *heap_alloc = ib_heap_allocator_create(fts_heap);

  indexes = ib_vector_create(heap_alloc, sizeof(dict_index_t *), 4);

  dict_table_get_all_fts_indexes(table, indexes);
}

/** fts_t destructor. */
fts_t::~fts_t() {
  mutex_free(&bg_threads_mutex);

  ut_ad(add_wq == nullptr);

  if (cache != nullptr) {
    fts_cache_clear(cache);
    fts_cache_destroy(cache);
    cache = nullptr;
  }

  /* There is no need to call ib_vector_free() on this->indexes
  because it is stored in this->fts_heap. */
}

/** Create an instance of fts_t.
 @return instance of fts_t */
fts_t *fts_create(dict_table_t *table) /*!< in/out: table with FTS indexes */
{
  fts_t *fts;
  mem_heap_t *heap;

  heap = mem_heap_create(512, UT_LOCATION_HERE);

  fts = static_cast<fts_t *>(mem_heap_alloc(heap, sizeof(*fts)));

  new (fts) fts_t(table, heap);

  return (fts);
}

/** Free the FTS resources. */
void fts_free(dict_table_t *table) /*!< in/out: table with FTS indexes */
{
  fts_t *fts = table->fts;

  fts->~fts_t();

  mem_heap_free(fts->fts_heap);

  table->fts = nullptr;
}

#if 0  // TODO: Enable this in WL#6608
/*********************************************************************//**
Signal FTS threads to initiate shutdown. */
void
fts_start_shutdown(
        dict_table_t*   table,          /*!< in: table with FTS indexes */
        fts_t*          fts)            /*!< in: fts instance that needs
                                        to be informed about shutdown */
{
        mutex_enter(&fts->bg_threads_mutex);

        fts->fts_status |= BG_THREAD_STOP;

        mutex_exit(&fts->bg_threads_mutex);

}

/*********************************************************************//**
Wait for FTS threads to shutdown. */
void
fts_shutdown(
        dict_table_t*   table,          /*!< in: table with FTS indexes */
        fts_t*          fts)            /*!< in: fts instance to shutdown */
{
        mutex_enter(&fts->bg_threads_mutex);

        ut_a(fts->fts_status & BG_THREAD_STOP);

        dict_table_wait_for_bg_threads_to_exit(table, std::chrono::milliseconds{20});

        mutex_exit(&fts->bg_threads_mutex);
}
#endif

/** Take a FTS savepoint. */
static inline void fts_savepoint_copy(
    const fts_savepoint_t *src, /*!< in: source savepoint */
    fts_savepoint_t *dst)       /*!< out: destination savepoint */
{
  const ib_rbt_node_t *node;
  const ib_rbt_t *tables;

  tables = src->tables;

  for (node = rbt_first(tables); node; node = rbt_next(tables, node)) {
    fts_trx_table_t *ftt_dst;
    const fts_trx_table_t **ftt_src;

    ftt_src = rbt_value(const fts_trx_table_t *, node);

    ftt_dst = fts_trx_table_clone(*ftt_src);

    rbt_insert(dst->tables, &ftt_dst, &ftt_dst);
  }
}

void fts_savepoint_take(fts_trx_t *fts_trx, const char *name) {
  mem_heap_t *heap;
  fts_savepoint_t *savepoint;
  fts_savepoint_t *last_savepoint;

  ut_a(name != nullptr);

  heap = fts_trx->heap;

  /* The implied savepoint must exist. */
  ut_a(ib_vector_size(fts_trx->savepoints) > 0);

  last_savepoint =
      static_cast<fts_savepoint_t *>(ib_vector_last(fts_trx->savepoints));
  savepoint = fts_savepoint_create(fts_trx->savepoints, name, heap);

  if (last_savepoint->tables != nullptr) {
    fts_savepoint_copy(last_savepoint, savepoint);
  }
}

/** Lookup a savepoint instance by name.
 @return ULINT_UNDEFINED if not found */
static inline ulint fts_savepoint_lookup(
    ib_vector_t *savepoints, /*!< in: savepoints */
    const char *name)        /*!< in: savepoint name */
{
  ulint i;

  ut_a(ib_vector_size(savepoints) > 0);

  for (i = 1; i < ib_vector_size(savepoints); ++i) {
    fts_savepoint_t *savepoint;

    savepoint = static_cast<fts_savepoint_t *>(ib_vector_get(savepoints, i));

    if (strcmp(name, savepoint->name) == 0) {
      return (i);
    }
  }

  return (ULINT_UNDEFINED);
}

/** Release the savepoint data identified by name. All savepoints created
 after the named savepoint are kept. */
void fts_savepoint_release(trx_t *trx,       /*!< in: transaction */
                           const char *name) /*!< in: savepoint name */
{
  ut_a(name != nullptr);

  ib_vector_t *savepoints = trx->fts_trx->savepoints;

  ut_a(ib_vector_size(savepoints) > 0);

  ulint i = fts_savepoint_lookup(savepoints, name);
  if (i != ULINT_UNDEFINED) {
    ut_a(i >= 1);

    fts_savepoint_t *savepoint;
    savepoint = static_cast<fts_savepoint_t *>(ib_vector_get(savepoints, i));

    if (i == ib_vector_size(savepoints) - 1) {
      /* If the savepoint is the last, we save its
      tables to the  previous savepoint. */
      fts_savepoint_t *prev_savepoint;
      prev_savepoint =
          static_cast<fts_savepoint_t *>(ib_vector_get(savepoints, i - 1));

      ib_rbt_t *tables = savepoint->tables;
      savepoint->tables = prev_savepoint->tables;
      prev_savepoint->tables = tables;
    }

    fts_savepoint_free(savepoint);
    ib_vector_remove(savepoints, *(void **)savepoint);

    /* Make sure we don't delete the implied savepoint. */
    ut_a(ib_vector_size(savepoints) > 0);
  }
}

/** Refresh last statement savepoint. */
void fts_savepoint_laststmt_refresh(trx_t *trx) /*!< in: transaction */
{
  fts_trx_t *fts_trx;
  fts_savepoint_t *savepoint;

  fts_trx = trx->fts_trx;

  savepoint = static_cast<fts_savepoint_t *>(ib_vector_pop(fts_trx->last_stmt));
  fts_savepoint_free(savepoint);

  ut_ad(ib_vector_is_empty(fts_trx->last_stmt));
  savepoint = fts_savepoint_create(fts_trx->last_stmt, nullptr, nullptr);
}

/********************************************************************
Undo the Doc ID add/delete operations in last stmt */
static void fts_undo_last_stmt(
    fts_trx_table_t *s_ftt, /*!< in: Transaction FTS table */
    fts_trx_table_t *l_ftt) /*!< in: last stmt FTS table */
{
  ib_rbt_t *s_rows;
  ib_rbt_t *l_rows;
  const ib_rbt_node_t *node;

  l_rows = l_ftt->rows;
  s_rows = s_ftt->rows;

  for (node = rbt_first(l_rows); node; node = rbt_next(l_rows, node)) {
    fts_trx_row_t *l_row = rbt_value(fts_trx_row_t, node);
    ib_rbt_bound_t parent;

    rbt_search(s_rows, &parent, &(l_row->doc_id));

    if (parent.result == 0) {
      fts_trx_row_t *s_row = rbt_value(fts_trx_row_t, parent.last);

      switch (l_row->state) {
        case FTS_INSERT:
          ut::free(rbt_remove_node(s_rows, parent.last));
          break;

        case FTS_DELETE:
          if (s_row->state == FTS_NOTHING) {
            s_row->state = FTS_INSERT;
          } else if (s_row->state == FTS_DELETE) {
            ut::free(rbt_remove_node(s_rows, parent.last));
          }
          break;

        /* FIXME: Check if FTS_MODIFY need to be addressed */
        case FTS_MODIFY:
        case FTS_NOTHING:
          break;
        default:
          ut_error;
      }
    }
  }
}

/** Rollback to savepoint identified by name. */
void fts_savepoint_rollback_last_stmt(trx_t *trx) /*!< in: transaction */
{
  ib_vector_t *savepoints;
  fts_savepoint_t *savepoint;
  fts_savepoint_t *last_stmt;
  fts_trx_t *fts_trx;
  ib_rbt_bound_t parent;
  const ib_rbt_node_t *node;
  ib_rbt_t *l_tables;
  ib_rbt_t *s_tables;

  fts_trx = trx->fts_trx;
  savepoints = fts_trx->savepoints;

  savepoint = static_cast<fts_savepoint_t *>(ib_vector_last(savepoints));
  last_stmt =
      static_cast<fts_savepoint_t *>(ib_vector_last(fts_trx->last_stmt));

  l_tables = last_stmt->tables;
  s_tables = savepoint->tables;

  for (node = rbt_first(l_tables); node; node = rbt_next(l_tables, node)) {
    fts_trx_table_t **l_ftt;

    l_ftt = rbt_value(fts_trx_table_t *, node);

    rbt_search_cmp(s_tables, &parent, &(*l_ftt)->table->id,
                   fts_trx_table_id_cmp, nullptr);

    if (parent.result == 0) {
      fts_trx_table_t **s_ftt;

      s_ftt = rbt_value(fts_trx_table_t *, parent.last);

      fts_undo_last_stmt(*s_ftt, *l_ftt);
    }
  }
}

/** Rollback to savepoint identified by name. */
void fts_savepoint_rollback(trx_t *trx,       /*!< in: transaction */
                            const char *name) /*!< in: savepoint name */
{
  ulint i;
  ib_vector_t *savepoints;

  ut_a(name != nullptr);

  savepoints = trx->fts_trx->savepoints;

  /* We pop all savepoints from the the top of the stack up to
  and including the instance that was found. */
  i = fts_savepoint_lookup(savepoints, name);

  if (i != ULINT_UNDEFINED) {
    fts_savepoint_t *savepoint;

    ut_a(i > 0);

    while (ib_vector_size(savepoints) > i) {
      fts_savepoint_t *savepoint;

      savepoint = static_cast<fts_savepoint_t *>(ib_vector_pop(savepoints));

      if (savepoint->name != nullptr) {
        /* Since name was allocated on the heap, the
        memory will be released when the transaction
        completes. */
        savepoint->name = nullptr;

        fts_savepoint_free(savepoint);
      }
    }

    /* Pop all a elements from the top of the stack that may
    have been released. We have to be careful that we don't
    delete the implied savepoint. */

    for (savepoint = static_cast<fts_savepoint_t *>(ib_vector_last(savepoints));
         ib_vector_size(savepoints) > 1 && savepoint->name == nullptr;
         savepoint =
             static_cast<fts_savepoint_t *>(ib_vector_last(savepoints))) {
      ib_vector_pop(savepoints);
    }

    /* Make sure we don't delete the implied savepoint. */
    ut_a(ib_vector_size(savepoints) > 0);

    /* Restore the savepoint. */
    fts_savepoint_take(trx->fts_trx, name);
  }
}

/** Check if a table is an FTS auxiliary table name.
@param[out]     table   FTS table info
@param[in]      name    Table name
@param[in]      len     Length of table name
@return true if the name matches an auxiliary table name pattern */
bool fts_is_aux_table_name(fts_aux_table_t *table, const char *name,
                           ulint len) {
  const char *ptr;
  char *end;
  char my_name[MAX_FULL_NAME_LEN + 1];

  ut_ad(len <= MAX_FULL_NAME_LEN);
  ut_memcpy(my_name, name, len);
  my_name[len] = 0;
  end = my_name + len;

  ptr = static_cast<const char *>(memchr(my_name, '/', len));

  if (ptr != nullptr) {
    /* We will start the match after the '/' */
    ++ptr;
    len = end - ptr;
  }

  /* All auxiliary tables are prefixed with "FTS_" and the name
  length will be at the very least greater than 20 bytes. */
  if (ptr != nullptr && len > 20 &&
      (strncmp(ptr, FTS_PREFIX, 4) == 0 ||
       strncmp(ptr, FTS_PREFIX_5_7, 4) == 0)) {
    ulint i;

    /* Skip the prefix. */
    ptr += 4;
    len -= 4;

    /* Try and read the table id. */
    if (!fts_read_object_id(&table->parent_id, ptr)) {
      return (false);
    }

    /* Skip the table id. */
    ptr = static_cast<const char *>(memchr(ptr, '_', len));

    if (ptr == nullptr) {
      return (false);
    }

    /* Skip the underscore. */
    ++ptr;
    ut_a(end >= ptr);
    len = end - ptr;

    /* It's not enough to be a FTS auxiliary table name */
    if (len == 0) {
      return (false);
    }

    /* First search the common table suffix array. */
    for (i = 0; fts_common_tables[i] != nullptr; ++i) {
      if ((len == strlen(fts_common_tables[i])) &&
          (strncmp(ptr, fts_common_tables[i], len) == 0)) {
        table->type = FTS_COMMON_TABLE;
        return (true);
      }

      if ((len == strlen(fts_common_tables_5_7[i])) &&
          (strncmp(ptr, fts_common_tables_5_7[i], len) == 0)) {
        table->type = FTS_COMMON_TABLE;
        return (true);
      }
    }

    /* Could be obsolete common tables. */
    if ((len == strlen("ADDED")) &&
        (native_strncasecmp(ptr, "ADDED", len) == 0)) {
      table->type = FTS_OBSOLETED_TABLE;
      return (true);
    }

    if ((len == strlen("STOPWORDS")) &&
        (native_strncasecmp(ptr, "STOPWORDS", len) == 0)) {
      table->type = FTS_OBSOLETED_TABLE;
      return (true);
    }

    /* Try and read the index id. */
    if (!fts_read_object_id(&table->index_id, ptr)) {
      return (false);
    }

    /* Skip the index id. */
    ptr = static_cast<const char *>(memchr(ptr, '_', len));

    if (ptr == nullptr) {
      return (false);
    }

    /* Skip the underscore. */
    ++ptr;
    ut_a(end >= ptr);
    len = end - ptr;

    /* It's not enough to be a FTS auxiliary table name */
    if (len == 0) {
      return (false);
    }

    /* Search the FT index specific array. */
    for (i = 0; i < FTS_NUM_AUX_INDEX; ++i) {
      if ((len == strlen(fts_get_suffix(i))) &&
          (strncmp(ptr, fts_get_suffix(i), len) == 0)) {
        table->type = FTS_INDEX_TABLE;
        return (true);
      }
      if ((len == strlen(fts_get_suffix_5_7(i))) &&
          (strncmp(ptr, fts_get_suffix_5_7(i), len) == 0)) {
        table->type = FTS_INDEX_TABLE;
        return (true);
      }
    }

    /* Other FT index specific table(s). */
    if ((len == strlen("DOC_ID")) &&
        (native_strncasecmp(ptr, "DOC_ID", len) == 0)) {
      table->type = FTS_OBSOLETED_TABLE;
      return (true);
    }
  }

  return (false);
}

/** Check whether user supplied stopword table is of the right format.
 Caller is responsible to hold dictionary locks.
 @return the stopword column charset if qualifies */
CHARSET_INFO *fts_valid_stopword_table(
    const char *stopword_table_name) /*!< in: Stopword table
                                     name */
{
  dict_table_t *table;
  dict_col_t *col = nullptr;

  if (!stopword_table_name) {
    return (nullptr);
  }

  MDL_ticket *mdl = nullptr;
  THD *thd = current_thd;
  table = dd_table_open_on_name(thd, &mdl, stopword_table_name, false,
                                DICT_ERR_IGNORE_NONE);

  if (!table) {
    ib::error(ER_IB_MSG_481)
        << "User stopword table " << stopword_table_name << " does not exist.";

    return (nullptr);
  } else {
    const char *col_name;

    dd_table_close(table, thd, &mdl, false);

    col_name = table->get_col_name(0);

    if (ut_strcmp(col_name, "value")) {
      ib::error(ER_IB_MSG_482) << "Invalid column name for stopword"
                                  " table "
                               << stopword_table_name
                               << ". Its"
                                  " first column must be named as 'value'.";

      return (nullptr);
    }

    col = table->get_col(0);

    if (col->mtype != DATA_VARCHAR && col->mtype != DATA_VARMYSQL) {
      ib::error(ER_IB_MSG_483) << "Invalid column type for stopword"
                                  " table "
                               << stopword_table_name
                               << ". Its"
                                  " first column must be of varchar type";

      return (nullptr);
    }
  }

  ut_ad(col);

  return (fts_get_charset(col->prtype));
}

/** This function loads the stopword into the FTS cache. It also
 records/fetches stopword configuration to/from FTS configure
 table, depending on whether we are creating or reloading the
 FTS.
 @return true if load operation is successful */
bool fts_load_stopword(
    const dict_table_t *table,          /*!< in: Table with FTS */
    trx_t *trx,                         /*!< in: Transactions */
    const char *global_stopword_table,  /*!< in: Global stopword table
                                        name */
    const char *session_stopword_table, /*!< in: Session stopword table
                                        name */
    bool stopword_is_on,                /*!< in: Whether stopword
                                         option is turned on/off */
    bool reload)                        /*!< in: Whether it is
                                         for reloading FTS table */
{
  fts_table_t fts_table;
  fts_string_t str;
  dberr_t error = DB_SUCCESS;
  ulint use_stopword;
  fts_cache_t *cache;
  const char *stopword_to_use = nullptr;
  bool new_trx = false;
  byte str_buffer[MAX_FULL_NAME_LEN + 1];

  FTS_INIT_FTS_TABLE(&fts_table, FTS_SUFFIX_CONFIG, FTS_COMMON_TABLE, table);

  cache = table->fts->cache;

  if (!reload && !(cache->stopword_info.status & STOPWORD_NOT_INIT)) {
    return true;
  }

  if (!trx) {
    trx = trx_allocate_for_background();
    trx->op_info = "upload FTS stopword";
    new_trx = true;
  }

  /* First check whether stopword filtering is turned off */
  if (reload) {
    error =
        fts_config_get_ulint(trx, &fts_table, FTS_USE_STOPWORD, &use_stopword);
  } else {
    use_stopword = (ulint)stopword_is_on;

    error =
        fts_config_set_ulint(trx, &fts_table, FTS_USE_STOPWORD, use_stopword);
  }

  if (error != DB_SUCCESS) {
    goto cleanup;
  }

  /* If stopword is turned off, no need to continue to load the
  stopword into cache, but still need to do initialization */
  if (!use_stopword) {
    cache->stopword_info.status = STOPWORD_OFF;
    goto cleanup;
  }

  if (reload) {
    /* Fetch the stopword table name from FTS config
    table */
    str.f_n_char = 0;
    str.f_str = str_buffer;
    str.f_len = sizeof(str_buffer) - 1;

    error =
        fts_config_get_value(trx, &fts_table, FTS_STOPWORD_TABLE_NAME, &str);

    if (error != DB_SUCCESS) {
      goto cleanup;
    }

    if (strlen((char *)str.f_str) > 0) {
      stopword_to_use = (const char *)str.f_str;
    }
  } else {
    stopword_to_use = (session_stopword_table) ? session_stopword_table
                                               : global_stopword_table;
  }

  if (stopword_to_use &&
      fts_load_user_stopword(stopword_to_use, &cache->stopword_info)) {
    /* Save the stopword table name to the configure
    table */
    if (!reload) {
      str.f_n_char = 0;
      str.f_str = (byte *)stopword_to_use;
      str.f_len = ut_strlen(stopword_to_use);

      error =
          fts_config_set_value(trx, &fts_table, FTS_STOPWORD_TABLE_NAME, &str);
    }
  } else {
    /* Load system default stopword list */
    fts_load_default_stopword(&cache->stopword_info);
  }

cleanup:
  if (new_trx) {
    if (error == DB_SUCCESS) {
      fts_sql_commit(trx);
    } else {
      fts_sql_rollback(trx);
    }

    trx_free_for_background(trx);
  }

  if (!cache->stopword_info.cached_stopword) {
    cache->stopword_info.cached_stopword =
        rbt_create_arg_cmp(sizeof(fts_tokenizer_word_t), innobase_fts_text_cmp,
                           &my_charset_latin1);
  }

  return (error == DB_SUCCESS);
}

/** Callback function when we initialize the FTS at the start up
 time. It recovers the maximum Doc IDs presented in the current table.
 @return: always returns true */
static bool fts_init_get_doc_id(void *row,      /*!< in: sel_node_t* */
                                void *user_arg) /*!< in: fts cache */
{
  doc_id_t doc_id = FTS_NULL_DOC_ID;
  sel_node_t *node = static_cast<sel_node_t *>(row);
  que_node_t *exp = node->select_list;
  fts_cache_t *cache = static_cast<fts_cache_t *>(user_arg);

  ut_ad(ib_vector_is_empty(cache->get_docs));

  /* Copy each indexed column content into doc->text.f_str */
  if (exp) {
    dfield_t *dfield = que_node_get_val(exp);
    dtype_t *type = dfield_get_type(dfield);
    void *data = dfield_get_data(dfield);

    ut_a(dtype_get_mtype(type) == DATA_INT);

    doc_id = static_cast<doc_id_t>(
        mach_read_from_8(static_cast<const byte *>(data)));

    if (doc_id >= cache->next_doc_id) {
      cache->next_doc_id = doc_id + 1;
    }
  }

  return true;
}

/** Callback function when we initialize the FTS at the start up
 time. It recovers Doc IDs that have not sync-ed to the auxiliary
 table, and require to bring them back into FTS index.
 @return: always returns true */
static bool fts_init_recover_doc(void *row,      /*!< in: sel_node_t* */
                                 void *user_arg) /*!< in: fts cache */
{
  fts_doc_t doc;
  ulint doc_len = 0;
  ulint field_no = 0;
  fts_get_doc_t *get_doc = static_cast<fts_get_doc_t *>(user_arg);
  doc_id_t doc_id = FTS_NULL_DOC_ID;
  sel_node_t *node = static_cast<sel_node_t *>(row);
  que_node_t *exp = node->select_list;
  fts_cache_t *cache = get_doc->cache;
  st_mysql_ftparser *parser = get_doc->index_cache->index->parser;

  fts_doc_init(&doc);
  doc.found = true;

  ut_ad(cache);

  /* Copy each indexed column content into doc->text.f_str */
  while (exp) {
    dfield_t *dfield = que_node_get_val(exp);
    ulint len = dfield_get_len(dfield);

    if (field_no == 0) {
      dtype_t *type = dfield_get_type(dfield);
      void *data = dfield_get_data(dfield);

      ut_a(dtype_get_mtype(type) == DATA_INT);

      doc_id = static_cast<doc_id_t>(
          mach_read_from_8(static_cast<const byte *>(data)));

      field_no++;
      exp = que_node_get_next(exp);
      continue;
    }

    if (len == UNIV_SQL_NULL) {
      exp = que_node_get_next(exp);
      continue;
    }

    ut_ad(get_doc);

    if (!get_doc->index_cache->charset) {
      get_doc->index_cache->charset = fts_get_charset(dfield->type.prtype);
    }

    doc.charset = get_doc->index_cache->charset;
    doc.is_ngram = get_doc->index_cache->index->is_ngram;

    if (dfield_is_ext(dfield)) {
      dict_table_t *table = cache->sync->table;

      /** When a nullptr is passed for trx, it means we will
      fetch the latest LOB (and no MVCC will be done). */
      doc.text.f_str = lob::btr_copy_externally_stored_field(
          nullptr, get_doc->index_cache->index, &doc.text.f_len, nullptr,
          static_cast<byte *>(dfield_get_data(dfield)),
          dict_table_page_size(table), len, false,
          static_cast<mem_heap_t *>(doc.self_heap->arg));
    } else {
      doc.text.f_str = static_cast<byte *>(dfield_get_data(dfield));

      doc.text.f_len = len;
    }

    if (field_no == 1) {
      fts_tokenize_document(&doc, nullptr, parser);
    } else {
      fts_tokenize_document_next(&doc, doc_len, nullptr, parser);
    }

    exp = que_node_get_next(exp);

    doc_len += (exp) ? len + 1 : len;

    field_no++;
  }

  fts_cache_add_doc(cache, get_doc->index_cache, doc_id, doc.tokens);

  fts_doc_free(&doc);

  cache->added++;

  if (doc_id >= cache->next_doc_id) {
    cache->next_doc_id = doc_id + 1;
  }

  return true;
}

/** This function brings FTS index in sync when FTS index is first
 used. There are documents that have not yet sync-ed to auxiliary
 tables from last server abnormally shutdown, we will need to bring
 such document into FTS cache before any further operations
 @return true if all OK */
bool fts_init_index(dict_table_t *table, /*!< in: Table with FTS */
                    bool has_cache_lock) /*!< in: Whether we already have
                                          cache lock */
{
  dict_index_t *index;
  doc_id_t start_doc;
  fts_get_doc_t *get_doc = nullptr;
  fts_cache_t *cache = table->fts->cache;
  bool need_init = false;

  ut_ad(!dict_sys_mutex_own());

  /* First check cache->get_docs is initialized */
  if (!has_cache_lock) {
    rw_lock_x_lock(&cache->lock, UT_LOCATION_HERE);
  }

  rw_lock_x_lock(&cache->init_lock, UT_LOCATION_HERE);
  if (cache->get_docs == nullptr) {
    cache->get_docs = fts_get_docs_create(cache);
  }
  rw_lock_x_unlock(&cache->init_lock);

  if (table->fts->fts_status & ADDED_TABLE_SYNCED) {
    goto func_exit;
  }

  need_init = true;

  start_doc = cache->synced_doc_id;

  if (!start_doc) {
    fts_cmp_set_sync_doc_id(table, 0, true, &start_doc);
    cache->synced_doc_id = start_doc;
  }

  /* No FTS index, this is the case when previous FTS index
  dropped, and we re-initialize the Doc ID system for subsequent
  insertion */
  if (ib_vector_is_empty(cache->get_docs)) {
    index = table->fts_doc_id_index;

    ut_a(index);

    fts_doc_fetch_by_doc_id(nullptr, start_doc, index,
                            FTS_FETCH_DOC_BY_ID_LARGE, fts_init_get_doc_id,
                            cache);
  } else {
    if (table->fts->cache->stopword_info.status & STOPWORD_NOT_INIT) {
      fts_load_stopword(table, nullptr, nullptr, nullptr, true, true);
    }

    for (ulint i = 0; i < ib_vector_size(cache->get_docs); ++i) {
      get_doc = static_cast<fts_get_doc_t *>(ib_vector_get(cache->get_docs, i));

      index = get_doc->index_cache->index;

      fts_doc_fetch_by_doc_id(nullptr, start_doc, index,
                              FTS_FETCH_DOC_BY_ID_LARGE, fts_init_recover_doc,
                              get_doc);
    }
  }

  table->fts->fts_status |= ADDED_TABLE_SYNCED;

  fts_get_docs_clear(cache->get_docs);

func_exit:
  if (!has_cache_lock) {
    rw_lock_x_unlock(&cache->lock);
  }

  if (need_init) {
    dict_sys_mutex_enter();
    /* Register the table with the optimize thread. */
    fts_optimize_add_table(table);
    dict_sys_mutex_exit();
  }

  return true;
}

/** Rename old FTS common and aux tables with the new table_id
@param[in]      old_name        old name of FTS AUX table
@param[in]      new_name        new name of FTS AUX table
@return new fts table if success, else nullptr on failure */
static dict_table_t *fts_upgrade_rename_aux_table_low(const char *old_name,
                                                      const char *new_name) {
  dict_sys_mutex_enter();

  dict_table_t *old_aux_table =
      dict_table_open_on_name(old_name, true, false, DICT_ERR_IGNORE_NONE);

  ut_ad(old_aux_table != nullptr);
  dict_table_close(old_aux_table, true, false);
  dberr_t err = dict_table_rename_in_cache(old_aux_table, new_name, false);
  if (err != DB_SUCCESS) {
    dict_sys_mutex_exit();
    return (nullptr);
  }

  dict_table_t *new_aux_table =
      dict_table_open_on_name(new_name, true, false, DICT_ERR_IGNORE_NONE);
  ut_ad(new_aux_table != nullptr);
  dict_sys_mutex_exit();

  return (new_aux_table);
}

/** Rename old FTS common and aux tables with the new table_id
@param[in]      old_name        old name of FTS AUX table
@param[in]      new_name        new name of FTS AUX table
@param[in]      rollback        if true, do the rename back
                                else mark original AUX tables
                                evictable */
static void fts_upgrade_rename_aux_table(const char *old_name,
                                         const char *new_name, bool rollback) {
  dict_table_t *new_table = nullptr;

  if (rollback) {
    new_table = fts_upgrade_rename_aux_table_low(old_name, new_name);

  } else {
    new_table =
        dict_table_open_on_name(old_name, false, false, DICT_ERR_IGNORE_NONE);
  }

  if (new_table == nullptr) {
    return;
  }

  dict_sys_mutex_enter();
  dict_table_allow_eviction(new_table);
  dict_table_close(new_table, true, false);
  dict_sys_mutex_exit();
}

/** During upgrade, tables are moved by DICT_MAX_DD_TABLES
offset, remove this offset to get 5.7 fts aux table names
@param[in]      table_id        8.0 table id */
inline table_id_t fts_upgrade_get_5_7_table_id(table_id_t table_id) {
  return (table_id - DICT_MAX_DD_TABLES);
}

/** Upgrade FTS AUX Tables. The FTS common and aux tables are
renamed because they have table_id in their name. We move table_ids
by DICT_MAX_DD_TABLES offset. Aux tables are registered into DD
after rename.
@param[in]      table           InnoDB table object
@return DB_SUCCESS or error code */
dberr_t fts_upgrade_aux_tables(dict_table_t *table) {
  fts_table_t fts_old_table;

  ut_ad(srv_is_upgrade_mode);

  FTS_INIT_FTS_TABLE(&fts_old_table, nullptr, FTS_COMMON_TABLE, table);
  fts_table_t fts_new_table = fts_old_table;

  fts_old_table.table_id = fts_upgrade_get_5_7_table_id(fts_old_table.table_id);

  /* Rename common auxiliary tables */
  for (ulint i = 0; fts_common_tables_5_7[i] != nullptr; ++i) {
    fts_old_table.suffix = fts_common_tables_5_7[i];

    bool is_config = fts_old_table.suffix == FTS_SUFFIX_CONFIG_5_7;
    char old_name[MAX_FULL_NAME_LEN];
    char new_name[MAX_FULL_NAME_LEN];

    fts_get_table_name_5_7(&fts_old_table, old_name);

    DBUG_EXECUTE_IF("dd_upgrade", ib::info(ER_IB_MSG_484)
                                      << "Old fts table name is " << old_name;);

    fts_new_table.suffix = fts_common_tables[i];
    fts_get_table_name(&fts_new_table, new_name);

    DBUG_EXECUTE_IF("dd_upgrade", ib::info(ER_IB_MSG_485)
                                      << "New fts table name is " << new_name;);

    dict_table_t *new_table =
        fts_upgrade_rename_aux_table_low(old_name, new_name);

    if (new_table == nullptr) {
      return (DB_ERROR);
    }

    dict_sys_mutex_enter();
    dict_table_prevent_eviction(new_table);
    dict_sys_mutex_exit();

    if (!dd_create_fts_common_table(table, new_table, is_config)) {
      dict_table_close(new_table, false, false);
      return (DB_FAIL);
    }
    dict_table_close(new_table, false, false);
  }

  fts_t *fts = table->fts;

  /* Rename index specific auxiliary tables */
  for (ulint i = 0; fts->indexes != nullptr && i < ib_vector_size(fts->indexes);
       ++i) {
    dict_index_t *index;

    index = static_cast<dict_index_t *>(ib_vector_getp(fts->indexes, i));

    FTS_INIT_INDEX_TABLE(&fts_old_table, nullptr, FTS_INDEX_TABLE, index);
    fts_new_table = fts_old_table;

    fts_old_table.table_id =
        fts_upgrade_get_5_7_table_id(fts_old_table.table_id);

    for (ulint j = 0; j < FTS_NUM_AUX_INDEX; ++j) {
      fts_old_table.suffix = fts_get_suffix_5_7(j);

      char old_name[MAX_FULL_NAME_LEN];
      char new_name[MAX_FULL_NAME_LEN];

      fts_get_table_name_5_7(&fts_old_table, old_name);

      fts_new_table.suffix = fts_get_suffix(j);
      fts_get_table_name(&fts_new_table, new_name);

      dict_table_t *new_table =
          fts_upgrade_rename_aux_table_low(old_name, new_name);

      if (new_table == nullptr) {
        return (DB_ERROR);
      }

      dict_sys_mutex_enter();
      dict_table_prevent_eviction(new_table);
      dict_sys_mutex_exit();

      CHARSET_INFO *charset = fts_get_charset(index->get_field(0)->col->prtype);

      if (!dd_create_fts_index_table(table, new_table, charset)) {
        dict_table_close(new_table, false, false);
        return (DB_FAIL);
      }
      dict_table_close(new_table, false, false);
    }
  }

  return (DB_SUCCESS);
}

/** Rename FTS AUX tablespace name from 8.0 format to 5.7 format.
This will be done on upgrade failure
@param[in]      table           parent table
@param[in]      rollback        rollback the rename from 8.0 to 5.7
                                if true, rename to 5.7 format
                                if false, mark the table as evictable
@return DB_SUCCESS on success, DB_ERROR on error */
dberr_t fts_upgrade_rename(const dict_table_t *table, bool rollback) {
  fts_table_t fts_old_table;

  ut_ad(srv_is_upgrade_mode);

  FTS_INIT_FTS_TABLE(&fts_old_table, nullptr, FTS_COMMON_TABLE, table);

  fts_table_t fts_new_table = fts_old_table;

  fts_new_table.table_id = fts_upgrade_get_5_7_table_id(fts_new_table.table_id);

  /* Rename common auxiliary tables */
  for (ulint i = 0; fts_common_tables[i] != nullptr; ++i) {
    fts_old_table.suffix = fts_common_tables[i];

    char old_name[MAX_FULL_NAME_LEN];
    char new_name[MAX_FULL_NAME_LEN];

    fts_get_table_name(&fts_old_table, old_name);

    fts_new_table.suffix = fts_common_tables_5_7[i];
    fts_get_table_name_5_7(&fts_new_table, new_name);

    fts_upgrade_rename_aux_table(old_name, new_name, rollback);
  }

  fts_t *fts = table->fts;

  /* Rename index specific auxiliary tables */
  for (ulint i = 0; fts->indexes != nullptr && i < ib_vector_size(fts->indexes);
       ++i) {
    dict_index_t *index;

    index = static_cast<dict_index_t *>(ib_vector_getp(fts->indexes, i));

    FTS_INIT_INDEX_TABLE(&fts_old_table, nullptr, FTS_INDEX_TABLE, index);
    fts_new_table = fts_old_table;

    fts_new_table.table_id =
        fts_upgrade_get_5_7_table_id(fts_new_table.table_id);

    for (ulint j = 0; j < FTS_NUM_AUX_INDEX; ++j) {
      fts_old_table.suffix = fts_get_suffix(j);

      char old_name[MAX_FULL_NAME_LEN];
      char new_name[MAX_FULL_NAME_LEN];

      fts_get_table_name(&fts_old_table, old_name);

      fts_new_table.suffix = fts_get_suffix_5_7(j);
      fts_get_table_name_5_7(&fts_new_table, new_name);

      fts_upgrade_rename_aux_table(old_name, new_name, rollback);
    }
  }
  return (DB_SUCCESS);
}
