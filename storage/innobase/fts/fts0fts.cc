/*****************************************************************************

Copyright (c) 2011, 2016, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

*****************************************************************************/

/**************************************************//**
@file fts/fts0fts.cc
Full Text Search interface
***********************************************************************/

#include "ha_prototypes.h"

#include "trx0roll.h"
#include "row0mysql.h"
#include "row0upd.h"
#include "dict0types.h"
#include "row0sel.h"
#include "fts0fts.h"
#include "fts0priv.h"
#include "fts0types.h"
#include "fts0types.ic"
#include "fts0vlc.ic"
#include "fts0plugin.h"
#include "dict0priv.h"
#include "dict0stats.h"
#include "btr0pcur.h"
#include "sync0sync.h"
#include "ut0new.h"

static const ulint FTS_MAX_ID_LEN = 32;

/** Column name from the FTS config table */
#define FTS_MAX_CACHE_SIZE_IN_MB	"cache_size_in_mb"

/** Verify if a aux table name is a obsolete table
by looking up the key word in the obsolete table names */
#define FTS_IS_OBSOLETE_AUX_TABLE(table_name)			\
	(strstr((table_name), "DOC_ID") != NULL			\
	 || strstr((table_name), "ADDED") != NULL		\
	 || strstr((table_name), "STOPWORDS") != NULL)

/** This is maximum FTS cache for each table and would be
a configurable variable */
ulong	fts_max_cache_size;

/** Whether the total memory used for FTS cache is exhausted, and we will
need a sync to free some memory */
bool	fts_need_sync = false;

/** Variable specifying the total memory allocated for FTS cache */
ulong	fts_max_total_cache_size;

/** This is FTS result cache limit for each query and would be
a configurable variable */
ulong	fts_result_cache_limit;

/** Variable specifying the maximum FTS max token size */
ulong	fts_max_token_size;

/** Variable specifying the minimum FTS max token size */
ulong	fts_min_token_size;


// FIXME: testing
ib_time_t elapsed_time = 0;
ulint n_nodes = 0;

#ifdef FTS_CACHE_SIZE_DEBUG
/** The cache size permissible lower limit (1K) */
static const ulint FTS_CACHE_SIZE_LOWER_LIMIT_IN_MB = 1;

/** The cache size permissible upper limit (1G) */
static const ulint FTS_CACHE_SIZE_UPPER_LIMIT_IN_MB = 1024;
#endif

/** Time to sleep after DEADLOCK error before retrying operation. */
static const ulint FTS_DEADLOCK_RETRY_WAIT = 100000;

/** variable to record innodb_fts_internal_tbl_name for information
schema table INNODB_FTS_INSERTED etc. */
char* fts_internal_tbl_name		= NULL;
char* fts_internal_tbl_name2		= NULL;

/** InnoDB default stopword list:
There are different versions of stopwords, the stop words listed
below comes from "Google Stopword" list. Reference:
http://meta.wikimedia.org/wiki/Stop_word_list/google_stop_word_list.
The final version of InnoDB default stopword list is still pending
for decision */
const char *fts_default_stopword[] =
{
	"a",
	"about",
	"an",
	"are",
	"as",
	"at",
	"be",
	"by",
	"com",
	"de",
	"en",
	"for",
	"from",
	"how",
	"i",
	"in",
	"is",
	"it",
	"la",
	"of",
	"on",
	"or",
	"that",
	"the",
	"this",
	"to",
	"was",
	"what",
	"when",
	"where",
	"who",
	"will",
	"with",
	"und",
	"the",
	"www",
	NULL
};

/** For storing table info when checking for orphaned tables. */
struct fts_aux_table_t {
	table_id_t	id;		/*!< Table id */
	table_id_t	parent_id;	/*!< Parent table id */
	table_id_t	index_id;	/*!< Table FT index id */
	char*		name;		/*!< Name of the table */
};

#ifdef FTS_DOC_STATS_DEBUG
/** Template for creating the FTS auxiliary index specific tables. This is
mainly designed for the statistics work in the future */
static const char* fts_create_index_tables_sql = {
	"BEGIN\n"
	""
	"CREATE TABLE $doc_id_table (\n"
	"   doc_id BIGINT UNSIGNED,\n"
	"   word_count INTEGER UNSIGNED NOT NULL\n"
	") COMPACT;\n"
	"CREATE UNIQUE CLUSTERED INDEX IND ON $doc_id_table(doc_id);\n"
};
#endif

/** FTS auxiliary table suffixes that are common to all FT indexes. */
const char* fts_common_tables[] = {
	"BEING_DELETED",
	"BEING_DELETED_CACHE",
	"CONFIG",
	"DELETED",
	"DELETED_CACHE",
	NULL
};

/** FTS auxiliary INDEX split intervals. */
const  fts_index_selector_t fts_index_selector[] = {
	{ 9, "INDEX_1" },
	{ 65, "INDEX_2" },
	{ 70, "INDEX_3" },
	{ 75, "INDEX_4" },
	{ 80, "INDEX_5" },
	{ 85, "INDEX_6" },
	{  0 , NULL	 }
};

/** Default config values for FTS indexes on a table. */
static const char* fts_config_table_insert_values_sql =
	"BEGIN\n"
	"\n"
	"INSERT INTO $config_table VALUES('"
		FTS_MAX_CACHE_SIZE_IN_MB "', '256');\n"
	""
	"INSERT INTO $config_table VALUES('"
		FTS_OPTIMIZE_LIMIT_IN_SECS  "', '180');\n"
	""
	"INSERT INTO $config_table VALUES ('"
		FTS_SYNCED_DOC_ID "', '0');\n"
	""
	"INSERT INTO $config_table VALUES ('"
		FTS_TOTAL_DELETED_COUNT "', '0');\n"
	"" /* Note: 0 == FTS_TABLE_STATE_RUNNING */
	"INSERT INTO $config_table VALUES ('"
		FTS_TABLE_STATE "', '0');\n";

/** FTS tokenize parmameter for plugin parser */
struct fts_tokenize_param_t {
	fts_doc_t*	result_doc;	/*!< Result doc for tokens */
	ulint		add_pos;	/*!< Added position for tokens */
};

/** Run SYNC on the table, i.e., write out data from the cache to the
FTS auxiliary INDEX table and clear the cache at the end.
@param[in,out]	sync		sync state
@param[in]	unlock_cache	whether unlock cache lock when write node
@param[in]	wait		whether wait when a sync is in progress
@param[in]      has_dict        whether has dict operation lock
@return DB_SUCCESS if all OK */
static
dberr_t
fts_sync(
	fts_sync_t*	sync,
	bool		unlock_cache,
	bool		wait,
	bool		has_dict);

/****************************************************************//**
Release all resources help by the words rb tree e.g., the node ilist. */
static
void
fts_words_free(
/*===========*/
	ib_rbt_t*	words)		/*!< in: rb tree of words */
	MY_ATTRIBUTE((nonnull));
#ifdef FTS_CACHE_SIZE_DEBUG
/****************************************************************//**
Read the max cache size parameter from the config table. */
static
void
fts_update_max_cache_size(
/*======================*/
	fts_sync_t*	sync);		/*!< in: sync state */
#endif

/*********************************************************************//**
This function fetches the document just inserted right before
we commit the transaction, and tokenize the inserted text data
and insert into FTS auxiliary table and its cache.
@return TRUE if successful */
static
ulint
fts_add_doc_by_id(
/*==============*/
	fts_trx_table_t*ftt,		/*!< in: FTS trx table */
	doc_id_t	doc_id,		/*!< in: doc id */
	ib_vector_t*	fts_indexes MY_ATTRIBUTE((unused)));
					/*!< in: affected fts indexes */
#ifdef FTS_DOC_STATS_DEBUG
/****************************************************************//**
Check whether a particular word (term) exists in the FTS index.
@return DB_SUCCESS if all went fine */
static
dberr_t
fts_is_word_in_index(
/*=================*/
	trx_t*		trx,		/*!< in: FTS query state */
	que_t**		graph,		/*!< out: Query graph */
	fts_table_t*	fts_table,	/*!< in: table instance */
	const fts_string_t* word,	/*!< in: the word to check */
	ibool*		found)		/*!< out: TRUE if exists */
	MY_ATTRIBUTE((nonnull, warn_unused_result));
#endif /* FTS_DOC_STATS_DEBUG */

/******************************************************************//**
Update the last document id. This function could create a new
transaction to update the last document id.
@return DB_SUCCESS if OK */
static
dberr_t
fts_update_sync_doc_id(
/*===================*/
	const dict_table_t*	table,		/*!< in: table */
	const char*		table_name,	/*!< in: table name, or NULL */
	doc_id_t		doc_id,		/*!< in: last document id */
	trx_t*			trx)		/*!< in: update trx, or NULL */
	MY_ATTRIBUTE((nonnull(1)));

/** Get a character set based on precise type.
@param prtype precise type
@return the corresponding character set */
UNIV_INLINE
CHARSET_INFO*
fts_get_charset(ulint prtype)
{
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

	uint cs_num = (uint) dtype_get_charset_coll(prtype);

	if (CHARSET_INFO* cs = get_charset(cs_num, MYF(MY_WME))) {
		return(cs);
	}

	ib::fatal() << "Unable to find charset-collation " << cs_num;
	return(NULL);
}

/****************************************************************//**
This function loads the default InnoDB stopword list */
static
void
fts_load_default_stopword(
/*======================*/
	fts_stopword_t*		stopword_info)	/*!< in: stopword info */
{
	fts_string_t		str;
	mem_heap_t*		heap;
	ib_alloc_t*		allocator;
	ib_rbt_t*		stop_words;

	allocator = stopword_info->heap;
	heap = static_cast<mem_heap_t*>(allocator->arg);

	if (!stopword_info->cached_stopword) {
		stopword_info->cached_stopword = rbt_create_arg_cmp(
			sizeof(fts_tokenizer_word_t), innobase_fts_text_cmp,
			&my_charset_latin1);
	}

	stop_words = stopword_info->cached_stopword;

	str.f_n_char = 0;

	for (ulint i = 0; fts_default_stopword[i]; ++i) {
		char*			word;
		fts_tokenizer_word_t	new_word;

		/* We are going to duplicate the value below. */
		word = const_cast<char*>(fts_default_stopword[i]);

		new_word.nodes = ib_vector_create(
			allocator, sizeof(fts_node_t), 4);

		str.f_len = ut_strlen(word);
		str.f_str = reinterpret_cast<byte*>(word);

		fts_string_dup(&new_word.text, &str, heap);

		rbt_insert(stop_words, &new_word, &new_word);
	}

	stopword_info->status = STOPWORD_FROM_DEFAULT;
}

/****************************************************************//**
Callback function to read a single stopword value.
@return Always return TRUE */
static
ibool
fts_read_stopword(
/*==============*/
	void*		row,		/*!< in: sel_node_t* */
	void*		user_arg)	/*!< in: pointer to ib_vector_t */
{
	ib_alloc_t*	allocator;
	fts_stopword_t*	stopword_info;
	sel_node_t*	sel_node;
	que_node_t*	exp;
	ib_rbt_t*	stop_words;
	dfield_t*	dfield;
	fts_string_t	str;
	mem_heap_t*	heap;
	ib_rbt_bound_t	parent;

	sel_node = static_cast<sel_node_t*>(row);
	stopword_info = static_cast<fts_stopword_t*>(user_arg);

	stop_words = stopword_info->cached_stopword;
	allocator =  static_cast<ib_alloc_t*>(stopword_info->heap);
	heap = static_cast<mem_heap_t*>(allocator->arg);

	exp = sel_node->select_list;

	/* We only need to read the first column */
	dfield = que_node_get_val(exp);

	str.f_n_char = 0;
	str.f_str = static_cast<byte*>(dfield_get_data(dfield));
	str.f_len = dfield_get_len(dfield);

	/* Only create new node if it is a value not already existed */
	if (str.f_len != UNIV_SQL_NULL
	    && rbt_search(stop_words, &parent, &str) != 0) {

		fts_tokenizer_word_t	new_word;

		new_word.nodes = ib_vector_create(
			allocator, sizeof(fts_node_t), 4);

		new_word.text.f_str = static_cast<byte*>(
			 mem_heap_alloc(heap, str.f_len + 1));

		memcpy(new_word.text.f_str, str.f_str, str.f_len);

		new_word.text.f_n_char = 0;
		new_word.text.f_len = str.f_len;
		new_word.text.f_str[str.f_len] = 0;

		rbt_insert(stop_words, &new_word, &new_word);
	}

	return(TRUE);
}

/******************************************************************//**
Load user defined stopword from designated user table
@return TRUE if load operation is successful */
static
ibool
fts_load_user_stopword(
/*===================*/
	fts_t*		fts,			/*!< in: FTS struct */
	const char*	stopword_table_name,	/*!< in: Stopword table
						name */
	fts_stopword_t*	stopword_info)		/*!< in: Stopword info */
{
	pars_info_t*	info;
	que_t*		graph;
	dberr_t		error = DB_SUCCESS;
	ibool		ret = TRUE;
	trx_t*		trx;
	ibool		has_lock = fts->fts_status & TABLE_DICT_LOCKED;

	trx = trx_allocate_for_background();
	trx->op_info = "Load user stopword table into FTS cache";

	if (!has_lock) {
		mutex_enter(&dict_sys->mutex);
	}

	/* Validate the user table existence and in the right
	format */
	stopword_info->charset = fts_valid_stopword_table(stopword_table_name);
	if (!stopword_info->charset) {
		ret = FALSE;
		goto cleanup;
	} else if (!stopword_info->cached_stopword) {
		/* Create the stopword RB tree with the stopword column
		charset. All comparison will use this charset */
		stopword_info->cached_stopword = rbt_create_arg_cmp(
			sizeof(fts_tokenizer_word_t), innobase_fts_text_cmp,
			stopword_info->charset);

	}

	info = pars_info_create();

	pars_info_bind_id(info, TRUE, "table_stopword", stopword_table_name);

	pars_info_bind_function(info, "my_func", fts_read_stopword,
				stopword_info);

	graph = fts_parse_sql_no_dict_lock(
		NULL,
		info,
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
				ib::warn() << "Lock wait timeout reading user"
					" stopword table. Retrying!";

				trx->error_state = DB_SUCCESS;
			} else {
				ib::error() << "Error '" << ut_strerr(error)
					<< "' while reading user stopword"
					" table.";
				ret = FALSE;
				break;
			}
		}
	}

	que_graph_free(graph);

cleanup:
	if (!has_lock) {
		mutex_exit(&dict_sys->mutex);
	}

	trx_free_for_background(trx);
	return(ret);
}

/******************************************************************//**
Initialize the index cache. */
static
void
fts_index_cache_init(
/*=================*/
	ib_alloc_t*		allocator,	/*!< in: the allocator to use */
	fts_index_cache_t*	index_cache)	/*!< in: index cache */
{
	ulint			i;

	ut_a(index_cache->words == NULL);

	index_cache->words = rbt_create_arg_cmp(
		sizeof(fts_tokenizer_word_t), innobase_fts_text_cmp,
		index_cache->charset);

	ut_a(index_cache->doc_stats == NULL);

	index_cache->doc_stats = ib_vector_create(
		allocator, sizeof(fts_doc_stats_t), 4);

	for (i = 0; i < FTS_NUM_AUX_INDEX; ++i) {
		ut_a(index_cache->ins_graph[i] == NULL);
		ut_a(index_cache->sel_graph[i] == NULL);
	}
}

/*********************************************************************//**
Initialize FTS cache. */
void
fts_cache_init(
/*===========*/
	fts_cache_t*	cache)		/*!< in: cache to initialize */
{
	ulint		i;

	/* Just to make sure */
	ut_a(cache->sync_heap->arg == NULL);

	cache->sync_heap->arg = mem_heap_create(1024);

	cache->total_size = 0;

	mutex_enter((ib_mutex_t*) &cache->deleted_lock);
	cache->deleted_doc_ids = ib_vector_create(
		cache->sync_heap, sizeof(fts_update_t), 4);
	mutex_exit((ib_mutex_t*) &cache->deleted_lock);

	/* Reset the cache data for all the FTS indexes. */
	for (i = 0; i < ib_vector_size(cache->indexes); ++i) {
		fts_index_cache_t*	index_cache;

		index_cache = static_cast<fts_index_cache_t*>(
			ib_vector_get(cache->indexes, i));

		fts_index_cache_init(cache->sync_heap, index_cache);
	}
}

/****************************************************************//**
Create a FTS cache. */
fts_cache_t*
fts_cache_create(
/*=============*/
	dict_table_t*	table)	/*!< in: table owns the FTS cache */
{
	mem_heap_t*	heap;
	fts_cache_t*	cache;

	heap = static_cast<mem_heap_t*>(mem_heap_create(512));

	cache = static_cast<fts_cache_t*>(
		mem_heap_zalloc(heap, sizeof(*cache)));

	cache->cache_heap = heap;

	rw_lock_create(fts_cache_rw_lock_key, &cache->lock, SYNC_FTS_CACHE);

	rw_lock_create(
		fts_cache_init_rw_lock_key, &cache->init_lock,
		SYNC_FTS_CACHE_INIT);

	mutex_create(LATCH_ID_FTS_DELETE, &cache->deleted_lock);

	mutex_create(LATCH_ID_FTS_OPTIMIZE, &cache->optimize_lock);

	mutex_create(LATCH_ID_FTS_DOC_ID, &cache->doc_id_lock);

	/* This is the heap used to create the cache itself. */
	cache->self_heap = ib_heap_allocator_create(heap);

	/* This is a transient heap, used for storing sync data. */
	cache->sync_heap = ib_heap_allocator_create(heap);
	cache->sync_heap->arg = NULL;

	cache->sync = static_cast<fts_sync_t*>(
		mem_heap_zalloc(heap, sizeof(fts_sync_t)));

	cache->sync->table = table;
	cache->sync->event = os_event_create(0);

	/* Create the index cache vector that will hold the inverted indexes. */
	cache->indexes = ib_vector_create(
		cache->self_heap, sizeof(fts_index_cache_t), 2);

	fts_cache_init(cache);

	cache->stopword_info.cached_stopword = NULL;
	cache->stopword_info.charset = NULL;

	cache->stopword_info.heap = cache->self_heap;

	cache->stopword_info.status = STOPWORD_NOT_INIT;

	return(cache);
}

/*******************************************************************//**
Add a newly create index into FTS cache */
void
fts_add_index(
/*==========*/
	dict_index_t*	index,		/*!< FTS index to be added */
	dict_table_t*	table)		/*!< table */
{
	fts_t*			fts = table->fts;
	fts_cache_t*		cache;
	fts_index_cache_t*	index_cache;

	ut_ad(fts);
	cache = table->fts->cache;

	rw_lock_x_lock(&cache->init_lock);

	ib_vector_push(fts->indexes, &index);

	index_cache = fts_find_index_cache(cache, index);

	if (!index_cache) {
		/* Add new index cache structure */
		index_cache = fts_cache_index_cache_create(table, index);
	}

	rw_lock_x_unlock(&cache->init_lock);
}

/*******************************************************************//**
recalibrate get_doc structure after index_cache in cache->indexes changed */
static
void
fts_reset_get_doc(
/*==============*/
	fts_cache_t*	cache)	/*!< in: FTS index cache */
{
	fts_get_doc_t*  get_doc;
	ulint		i;

	ut_ad(rw_lock_own(&cache->init_lock, RW_LOCK_X));

	ib_vector_reset(cache->get_docs);

	for (i = 0; i < ib_vector_size(cache->indexes); i++) {
		fts_index_cache_t*	ind_cache;

		ind_cache = static_cast<fts_index_cache_t*>(
			ib_vector_get(cache->indexes, i));

		get_doc = static_cast<fts_get_doc_t*>(
			ib_vector_push(cache->get_docs, NULL));

		memset(get_doc, 0x0, sizeof(*get_doc));

		get_doc->index_cache = ind_cache;
	}

	ut_ad(ib_vector_size(cache->get_docs)
	      == ib_vector_size(cache->indexes));
}

/*******************************************************************//**
Check an index is in the table->indexes list
@return TRUE if it exists */
static
ibool
fts_in_dict_index(
/*==============*/
	dict_table_t*	table,		/*!< in: Table */
	dict_index_t*	index_check)	/*!< in: index to be checked */
{
	dict_index_t*	index;

	for (index = dict_table_get_first_index(table);
	     index != NULL;
	     index = dict_table_get_next_index(index)) {

		if (index == index_check) {
			return(TRUE);
		}
	}

	return(FALSE);
}

/*******************************************************************//**
Check an index is in the fts->cache->indexes list
@return TRUE if it exists */
static
ibool
fts_in_index_cache(
/*===============*/
	dict_table_t*	table,	/*!< in: Table */
	dict_index_t*	index)	/*!< in: index to be checked */
{
	ulint	i;

	for (i = 0; i < ib_vector_size(table->fts->cache->indexes); i++) {
		fts_index_cache_t*      index_cache;

		index_cache = static_cast<fts_index_cache_t*>(
			ib_vector_get(table->fts->cache->indexes, i));

		if (index_cache->index == index) {
			return(TRUE);
		}
	}

	return(FALSE);
}

/*******************************************************************//**
Check indexes in the fts->indexes is also present in index cache and
table->indexes list
@return TRUE if all indexes match */
ibool
fts_check_cached_index(
/*===================*/
	dict_table_t*	table)	/*!< in: Table where indexes are dropped */
{
	ulint	i;

	if (!table->fts || !table->fts->cache) {
		return(TRUE);
	}

	ut_a(ib_vector_size(table->fts->indexes)
	      == ib_vector_size(table->fts->cache->indexes));

	for (i = 0; i < ib_vector_size(table->fts->indexes); i++) {
		dict_index_t*	index;

		index = static_cast<dict_index_t*>(
			ib_vector_getp(table->fts->indexes, i));

		if (!fts_in_index_cache(table, index)) {
			return(FALSE);
		}

		if (!fts_in_dict_index(table, index)) {
			return(FALSE);
		}
	}

	return(TRUE);
}

/*******************************************************************//**
Drop auxiliary tables related to an FTS index
@return DB_SUCCESS or error number */
dberr_t
fts_drop_index(
/*===========*/
	dict_table_t*	table,	/*!< in: Table where indexes are dropped */
	dict_index_t*	index,	/*!< in: Index to be dropped */
	trx_t*		trx)	/*!< in: Transaction for the drop */
{
	ib_vector_t*	indexes = table->fts->indexes;
	dberr_t		err = DB_SUCCESS;

	ut_a(indexes);

	if ((ib_vector_size(indexes) == 1
	    && (index == static_cast<dict_index_t*>(
			ib_vector_getp(table->fts->indexes, 0))))
	   || ib_vector_is_empty(indexes)) {
		doc_id_t	current_doc_id;
		doc_id_t	first_doc_id;

		/* If we are dropping the only FTS index of the table,
		remove it from optimize thread */
		fts_optimize_remove_table(table);

		DICT_TF2_FLAG_UNSET(table, DICT_TF2_FTS);

		/* If Doc ID column is not added internally by FTS index,
		we can drop all FTS auxiliary tables. Otherwise, we will
		need to keep some common table such as CONFIG table, so
		as to keep track of incrementing Doc IDs */
		if (!DICT_TF2_FLAG_IS_SET(
			table, DICT_TF2_FTS_HAS_DOC_ID)) {

			err = fts_drop_tables(trx, table);

			err = fts_drop_index_tables(trx, index);

			fts_free(table);

			return(err);
		}

		current_doc_id = table->fts->cache->next_doc_id;
		first_doc_id = table->fts->cache->first_doc_id;
		fts_cache_clear(table->fts->cache);
		fts_cache_destroy(table->fts->cache);
		table->fts->cache = fts_cache_create(table);
		table->fts->cache->next_doc_id = current_doc_id;
		table->fts->cache->first_doc_id = first_doc_id;
	} else {
		fts_cache_t*            cache = table->fts->cache;
		fts_index_cache_t*      index_cache;

		rw_lock_x_lock(&cache->init_lock);

		index_cache = fts_find_index_cache(cache, index);

		if (index_cache != NULL) {
			if (index_cache->words) {
				fts_words_free(index_cache->words);
				rbt_free(index_cache->words);
			}

			ib_vector_remove(cache->indexes, *(void**) index_cache);
		}

		if (cache->get_docs) {
			fts_reset_get_doc(cache);
		}

		rw_lock_x_unlock(&cache->init_lock);
	}

	err = fts_drop_index_tables(trx, index);

	ib_vector_remove(indexes, (const void*) index);

	return(err);
}

/****************************************************************//**
Free the query graph but check whether dict_sys->mutex is already
held */
void
fts_que_graph_free_check_lock(
/*==========================*/
	fts_table_t*		fts_table,	/*!< in: FTS table */
	const fts_index_cache_t*index_cache,	/*!< in: FTS index cache */
	que_t*			graph)		/*!< in: query graph */
{
	ibool	has_dict = FALSE;

	if (fts_table && fts_table->table) {
		ut_ad(fts_table->table->fts);

		has_dict = fts_table->table->fts->fts_status
			 & TABLE_DICT_LOCKED;
	} else if (index_cache) {
		ut_ad(index_cache->index->table->fts);

		has_dict = index_cache->index->table->fts->fts_status
			 & TABLE_DICT_LOCKED;
	}

	if (!has_dict) {
		mutex_enter(&dict_sys->mutex);
	}

	ut_ad(mutex_own(&dict_sys->mutex));

	que_graph_free(graph);

	if (!has_dict) {
		mutex_exit(&dict_sys->mutex);
	}
}

/****************************************************************//**
Create an FTS index cache. */
CHARSET_INFO*
fts_index_get_charset(
/*==================*/
	dict_index_t*		index)		/*!< in: FTS index */
{
	CHARSET_INFO*		charset = NULL;
	dict_field_t*		field;
	ulint			prtype;

	field = dict_index_get_nth_field(index, 0);
	prtype = field->col->prtype;

	charset = fts_get_charset(prtype);

#ifdef FTS_DEBUG
	/* Set up charset info for this index. Please note all
	field of the FTS index should have the same charset */
	for (i = 1; i < index->n_fields; i++) {
		CHARSET_INFO*   fld_charset;

		field = dict_index_get_nth_field(index, i);
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

	return(charset);

}
/****************************************************************//**
Create an FTS index cache.
@return Index Cache */
fts_index_cache_t*
fts_cache_index_cache_create(
/*=========================*/
	dict_table_t*		table,		/*!< in: table with FTS index */
	dict_index_t*		index)		/*!< in: FTS index */
{
	ulint			n_bytes;
	fts_index_cache_t*	index_cache;
	fts_cache_t*		cache = table->fts->cache;

	ut_a(cache != NULL);

	ut_ad(rw_lock_own(&cache->init_lock, RW_LOCK_X));

	/* Must not already exist in the cache vector. */
	ut_a(fts_find_index_cache(cache, index) == NULL);

	index_cache = static_cast<fts_index_cache_t*>(
		ib_vector_push(cache->indexes, NULL));

	memset(index_cache, 0x0, sizeof(*index_cache));

	index_cache->index = index;

	index_cache->charset = fts_index_get_charset(index);

	n_bytes = sizeof(que_t*) * FTS_NUM_AUX_INDEX;

	index_cache->ins_graph = static_cast<que_t**>(
		mem_heap_zalloc(static_cast<mem_heap_t*>(
			cache->self_heap->arg), n_bytes));

	index_cache->sel_graph = static_cast<que_t**>(
		mem_heap_zalloc(static_cast<mem_heap_t*>(
			cache->self_heap->arg), n_bytes));

	fts_index_cache_init(cache->sync_heap, index_cache);

	if (cache->get_docs) {
		fts_reset_get_doc(cache);
	}

	return(index_cache);
}

/****************************************************************//**
Release all resources help by the words rb tree e.g., the node ilist. */
static
void
fts_words_free(
/*===========*/
	ib_rbt_t*	words)			/*!< in: rb tree of words */
{
	const ib_rbt_node_t*	rbt_node;

	/* Free the resources held by a word. */
	for (rbt_node = rbt_first(words);
	     rbt_node != NULL;
	     rbt_node = rbt_first(words)) {

		ulint			i;
		fts_tokenizer_word_t*	word;

		word = rbt_value(fts_tokenizer_word_t, rbt_node);

		/* Free the ilists of this word. */
		for (i = 0; i < ib_vector_size(word->nodes); ++i) {

			fts_node_t* fts_node = static_cast<fts_node_t*>(
				ib_vector_get(word->nodes, i));

			ut_free(fts_node->ilist);
			fts_node->ilist = NULL;
		}

		/* NOTE: We are responsible for free'ing the node */
		ut_free(rbt_remove_node(words, rbt_node));
	}
}

/** Clear cache.
@param[in,out]	cache	fts cache */
void
fts_cache_clear(
	fts_cache_t*	cache)
{
	ulint		i;

	for (i = 0; i < ib_vector_size(cache->indexes); ++i) {
		ulint			j;
		fts_index_cache_t*	index_cache;

		index_cache = static_cast<fts_index_cache_t*>(
			ib_vector_get(cache->indexes, i));

		fts_words_free(index_cache->words);

		rbt_free(index_cache->words);

		index_cache->words = NULL;

		for (j = 0; j < FTS_NUM_AUX_INDEX; ++j) {

			if (index_cache->ins_graph[j] != NULL) {

				fts_que_graph_free_check_lock(
					NULL, index_cache,
					index_cache->ins_graph[j]);

				index_cache->ins_graph[j] = NULL;
			}

			if (index_cache->sel_graph[j] != NULL) {

				fts_que_graph_free_check_lock(
					NULL, index_cache,
					index_cache->sel_graph[j]);

				index_cache->sel_graph[j] = NULL;
			}
		}

		index_cache->doc_stats = NULL;
	}

	mem_heap_free(static_cast<mem_heap_t*>(cache->sync_heap->arg));
	cache->sync_heap->arg = NULL;

	fts_need_sync = false;

	cache->total_size = 0;

	mutex_enter((ib_mutex_t*) &cache->deleted_lock);
	cache->deleted_doc_ids = NULL;
	mutex_exit((ib_mutex_t*) &cache->deleted_lock);
}

/*********************************************************************//**
Search the index specific cache for a particular FTS index.
@return the index cache else NULL */
UNIV_INLINE
fts_index_cache_t*
fts_get_index_cache(
/*================*/
	fts_cache_t*		cache,		/*!< in: cache to search */
	const dict_index_t*	index)		/*!< in: index to search for */
{
	ulint			i;

	ut_ad(rw_lock_own((rw_lock_t*) &cache->lock, RW_LOCK_X)
	      || rw_lock_own((rw_lock_t*) &cache->init_lock, RW_LOCK_X));

	for (i = 0; i < ib_vector_size(cache->indexes); ++i) {
		fts_index_cache_t*	index_cache;

		index_cache = static_cast<fts_index_cache_t*>(
			ib_vector_get(cache->indexes, i));

		if (index_cache->index == index) {

			return(index_cache);
		}
	}

	return(NULL);
}

#ifdef FTS_DEBUG
/*********************************************************************//**
Search the index cache for a get_doc structure.
@return the fts_get_doc_t item else NULL */
static
fts_get_doc_t*
fts_get_index_get_doc(
/*==================*/
	fts_cache_t*		cache,		/*!< in: cache to search */
	const dict_index_t*	index)		/*!< in: index to search for */
{
	ulint			i;

	ut_ad(rw_lock_own((rw_lock_t*) &cache->init_lock, RW_LOCK_X));

	for (i = 0; i < ib_vector_size(cache->get_docs); ++i) {
		fts_get_doc_t*	get_doc;

		get_doc = static_cast<fts_get_doc_t*>(
			ib_vector_get(cache->get_docs, i));

		if (get_doc->index_cache->index == index) {

			return(get_doc);
		}
	}

	return(NULL);
}
#endif

/**********************************************************************//**
Free the FTS cache. */
void
fts_cache_destroy(
/*==============*/
	fts_cache_t*	cache)			/*!< in: cache*/
{
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
		mem_heap_free(static_cast<mem_heap_t*>(cache->sync_heap->arg));
	}

	mem_heap_free(cache->cache_heap);
}

/**********************************************************************//**
Find an existing word, or if not found, create one and return it.
@return specified word token */
static
fts_tokenizer_word_t*
fts_tokenizer_word_get(
/*===================*/
	fts_cache_t*	cache,			/*!< in: cache */
	fts_index_cache_t*
			index_cache,		/*!< in: index cache */
	fts_string_t*	text)			/*!< in: node text */
{
	fts_tokenizer_word_t*	word;
	ib_rbt_bound_t		parent;

	ut_ad(rw_lock_own(&cache->lock, RW_LOCK_X));

	/* If it is a stopword, do not index it */
	if (!fts_check_token(text,
		    cache->stopword_info.cached_stopword,
		    index_cache->index->is_ngram,
		    index_cache->charset)) {

		return(NULL);
	}

	/* Check if we found a match, if not then add word to tree. */
	if (rbt_search(index_cache->words, &parent, text) != 0) {
		mem_heap_t*		heap;
		fts_tokenizer_word_t	new_word;

		heap = static_cast<mem_heap_t*>(cache->sync_heap->arg);

		new_word.nodes = ib_vector_create(
			cache->sync_heap, sizeof(fts_node_t), 4);

		fts_string_dup(&new_word.text, text, heap);

		parent.last = rbt_add_node(
			index_cache->words, &parent, &new_word);

		/* Take into account the RB tree memory use and the vector. */
		cache->total_size += sizeof(new_word)
			+ sizeof(ib_rbt_node_t)
			+ text->f_len
			+ (sizeof(fts_node_t) * 4)
			+ sizeof(*new_word.nodes);

		ut_ad(rbt_validate(index_cache->words));
	}

	word = rbt_value(fts_tokenizer_word_t, parent.last);

	return(word);
}

/**********************************************************************//**
Add the given doc_id/word positions to the given node's ilist. */
void
fts_cache_node_add_positions(
/*=========================*/
	fts_cache_t*	cache,		/*!< in: cache */
	fts_node_t*	node,		/*!< in: word node */
	doc_id_t	doc_id,		/*!< in: doc id */
	ib_vector_t*	positions)	/*!< in: fts_token_t::positions */
{
	ulint		i;
	byte*		ptr;
	byte*		ilist;
	ulint		enc_len;
	ulint		last_pos;
	byte*		ptr_start;
	ulint		doc_id_delta;

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
		ulint	pos = *(static_cast<ulint*>(
			ib_vector_get(positions, i)));

		ut_ad(last_pos == 0 || pos > last_pos);

		enc_len += fts_get_encoded_len(pos - last_pos);
		last_pos = pos;
	}

	/* The 0x00 byte at the end of the token positions list. */
	enc_len++;

	if ((node->ilist_size_alloc - node->ilist_size) >= enc_len) {
		/* No need to allocate more space, we can fit in the new
		data at the end of the old one. */
		ilist = NULL;
		ptr = node->ilist + node->ilist_size;
	} else {
		ulint	new_size = node->ilist_size + enc_len;

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

		ilist = static_cast<byte*>(ut_malloc_nokey(new_size));
		ptr = ilist + node->ilist_size;

		node->ilist_size_alloc = new_size;
	}

	ptr_start = ptr;

	/* Encode the new fragment. */
	ptr += fts_encode_int(doc_id_delta, ptr);

	last_pos = 0;
	for (i = 0; i < ib_vector_size(positions); i++) {
		ulint	pos = *(static_cast<ulint*>(
			 ib_vector_get(positions, i)));

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
			ut_free(node->ilist);
		}

		node->ilist = ilist;
	}

	node->ilist_size += enc_len;

	if (cache) {
		cache->total_size += enc_len;
	}

	if (node->first_doc_id == FTS_NULL_DOC_ID) {
		node->first_doc_id = doc_id;
	}

	node->last_doc_id = doc_id;
	++node->doc_count;
}

/**********************************************************************//**
Add document to the cache. */
static
void
fts_cache_add_doc(
/*==============*/
	fts_cache_t*	cache,			/*!< in: cache */
	fts_index_cache_t*
			index_cache,		/*!< in: index cache */
	doc_id_t	doc_id,			/*!< in: doc id to add */
	ib_rbt_t*	tokens)			/*!< in: document tokens */
{
	const ib_rbt_node_t*	node;
	ulint			n_words;
	fts_doc_stats_t*	doc_stats;

	if (!tokens) {
		return;
	}

	ut_ad(rw_lock_own(&cache->lock, RW_LOCK_X));

	n_words = rbt_size(tokens);

	for (node = rbt_first(tokens); node; node = rbt_first(tokens)) {

		fts_tokenizer_word_t*	word;
		fts_node_t*		fts_node = NULL;
		fts_token_t*		token = rbt_value(fts_token_t, node);

		/* Find and/or add token to the cache. */
		word = fts_tokenizer_word_get(
			cache, index_cache, &token->text);

		if (!word) {
			ut_free(rbt_remove_node(tokens, node));
			continue;
		}

		if (ib_vector_size(word->nodes) > 0) {
			fts_node = static_cast<fts_node_t*>(
				ib_vector_last(word->nodes));
		}

		if (fts_node == NULL || fts_node->synced
		    || fts_node->ilist_size > FTS_ILIST_MAX_SIZE
		    || doc_id < fts_node->last_doc_id) {

			fts_node = static_cast<fts_node_t*>(
				ib_vector_push(word->nodes, NULL));

			memset(fts_node, 0x0, sizeof(*fts_node));

			cache->total_size += sizeof(*fts_node);
		}

		fts_cache_node_add_positions(
			cache, fts_node, doc_id, token->positions);

		ut_free(rbt_remove_node(tokens, node));
	}

	ut_a(rbt_empty(tokens));

	/* Add to doc ids processed so far. */
	doc_stats = static_cast<fts_doc_stats_t*>(
		ib_vector_push(index_cache->doc_stats, NULL));

	doc_stats->doc_id = doc_id;
	doc_stats->word_count = n_words;

	/* Add the doc stats memory usage too. */
	cache->total_size += sizeof(*doc_stats);

	if (doc_id > cache->sync->max_doc_id) {
		cache->sync->max_doc_id = doc_id;
	}
}

/****************************************************************//**
Drops a table. If the table can't be found we return a SUCCESS code.
@return DB_SUCCESS or error code */
static MY_ATTRIBUTE((nonnull, warn_unused_result))
dberr_t
fts_drop_table(
/*===========*/
	trx_t*		trx,			/*!< in: transaction */
	const char*	table_name)		/*!< in: table to drop */
{
	dict_table_t*	table;
	dberr_t		error = DB_SUCCESS;

	/* Check that the table exists in our data dictionary.
	Similar to regular drop table case, we will open table with
	DICT_ERR_IGNORE_INDEX_ROOT and DICT_ERR_IGNORE_CORRUPT option */
	table = dict_table_open_on_name(
		table_name, TRUE, FALSE,
		static_cast<dict_err_ignore_t>(
                        DICT_ERR_IGNORE_INDEX_ROOT | DICT_ERR_IGNORE_CORRUPT));

	if (table != 0) {

		dict_table_close(table, TRUE, FALSE);

		/* Pass nonatomic=false (dont allow data dict unlock),
		because the transaction may hold locks on SYS_* tables from
		previous calls to fts_drop_table(). */
		error = row_drop_table_for_mysql(table_name, trx, true, false);

		if (error != DB_SUCCESS) {
			ib::error() << "Unable to drop FTS index aux table "
				<< table_name << ": " << ut_strerr(error);
		}
	} else {
		error = DB_FAIL;
	}

	return(error);
}

/****************************************************************//**
Rename a single auxiliary table due to database name change.
@return DB_SUCCESS or error code */
static MY_ATTRIBUTE((nonnull, warn_unused_result))
dberr_t
fts_rename_one_aux_table(
/*=====================*/
	const char*	new_name,		/*!< in: new parent tbl name */
	const char*	fts_table_old_name,	/*!< in: old aux tbl name */
	trx_t*		trx)			/*!< in: transaction */
{
	char	fts_table_new_name[MAX_TABLE_NAME_LEN];
	ulint	new_db_name_len = dict_get_db_name_len(new_name);
	ulint	old_db_name_len = dict_get_db_name_len(fts_table_old_name);
	ulint	table_new_name_len = strlen(fts_table_old_name)
				     + new_db_name_len - old_db_name_len;

	/* Check if the new and old database names are the same, if so,
	nothing to do */
	ut_ad((new_db_name_len != old_db_name_len)
	      || strncmp(new_name, fts_table_old_name, old_db_name_len) != 0);

	/* Get the database name from "new_name", and table name
	from the fts_table_old_name */
	strncpy(fts_table_new_name, new_name, new_db_name_len);
	strncpy(fts_table_new_name + new_db_name_len,
	       strchr(fts_table_old_name, '/'),
	       table_new_name_len - new_db_name_len);
	fts_table_new_name[table_new_name_len] = 0;

	return(row_rename_table_for_mysql(
		fts_table_old_name, fts_table_new_name, trx, false));
}

/****************************************************************//**
Rename auxiliary tables for all fts index for a table. This(rename)
is due to database name change
@return DB_SUCCESS or error code */
dberr_t
fts_rename_aux_tables(
/*==================*/
	dict_table_t*	table,		/*!< in: user Table */
	const char*     new_name,       /*!< in: new table name */
	trx_t*		trx)		/*!< in: transaction */
{
	ulint		i;
	fts_table_t	fts_table;

	FTS_INIT_FTS_TABLE(&fts_table, NULL, FTS_COMMON_TABLE, table);

	/* Rename common auxiliary tables */
	for (i = 0; fts_common_tables[i] != NULL; ++i) {
		char	old_table_name[MAX_FULL_NAME_LEN];
		dberr_t	err = DB_SUCCESS;

		fts_table.suffix = fts_common_tables[i];

		fts_get_table_name(&fts_table, old_table_name);

		err = fts_rename_one_aux_table(new_name, old_table_name, trx);

		if (err != DB_SUCCESS) {
			return(err);
		}
	}

	fts_t*	fts = table->fts;

	/* Rename index specific auxiliary tables */
	for (i = 0; fts->indexes != 0 && i < ib_vector_size(fts->indexes);
	     ++i) {
		dict_index_t*	index;

		index = static_cast<dict_index_t*>(
			ib_vector_getp(fts->indexes, i));

		FTS_INIT_INDEX_TABLE(&fts_table, NULL, FTS_INDEX_TABLE, index);

		for (ulint j = 0; j < FTS_NUM_AUX_INDEX; ++j) {
			dberr_t	err;
			char	old_table_name[MAX_FULL_NAME_LEN];

			fts_table.suffix = fts_get_suffix(j);

			fts_get_table_name(&fts_table, old_table_name);

			err = fts_rename_one_aux_table(
				new_name, old_table_name, trx);

			DBUG_EXECUTE_IF("fts_rename_failure",
					err = DB_DEADLOCK;
					fts_sql_rollback(trx););

			if (err != DB_SUCCESS) {
				return(err);
			}
		}
	}

	return(DB_SUCCESS);
}

/****************************************************************//**
Drops the common ancillary tables needed for supporting an FTS index
on the given table. row_mysql_lock_data_dictionary must have been called
before this.
@return DB_SUCCESS or error code */
static MY_ATTRIBUTE((nonnull, warn_unused_result))
dberr_t
fts_drop_common_tables(
/*===================*/
	trx_t*		trx,			/*!< in: transaction */
	fts_table_t*	fts_table)		/*!< in: table with an FTS
						index */
{
	ulint		i;
	dberr_t		error = DB_SUCCESS;

	for (i = 0; fts_common_tables[i] != NULL; ++i) {
		dberr_t	err;
		char	table_name[MAX_FULL_NAME_LEN];

		fts_table->suffix = fts_common_tables[i];

		fts_get_table_name(fts_table, table_name);

		err = fts_drop_table(trx, table_name);

		/* We only return the status of the last error. */
		if (err != DB_SUCCESS && err != DB_FAIL) {
			error = err;
		}
	}

	return(error);
}

/****************************************************************//**
Since we do a horizontal split on the index table, we need to drop
all the split tables.
@return DB_SUCCESS or error code */
dberr_t
fts_drop_index_split_tables(
/*========================*/
	trx_t*		trx,			/*!< in: transaction */
	dict_index_t*	index)			/*!< in: fts instance */

{
	ulint		i;
	fts_table_t	fts_table;
	dberr_t		error = DB_SUCCESS;

	FTS_INIT_INDEX_TABLE(&fts_table, NULL, FTS_INDEX_TABLE, index);

	for (i = 0; i < FTS_NUM_AUX_INDEX; ++i) {
		dberr_t	err;
		char	table_name[MAX_FULL_NAME_LEN];

		fts_table.suffix = fts_get_suffix(i);

		fts_get_table_name(&fts_table, table_name);

		err = fts_drop_table(trx, table_name);

		/* We only return the status of the last error. */
		if (err != DB_SUCCESS && err != DB_FAIL) {
			error = err;
		}
	}

	return(error);
}

/****************************************************************//**
Drops FTS auxiliary tables for an FTS index
@return DB_SUCCESS or error code */
dberr_t
fts_drop_index_tables(
/*==================*/
	trx_t*		trx,		/*!< in: transaction */
	dict_index_t*	index)		/*!< in: Index to drop */
{
	dberr_t			error = DB_SUCCESS;

#ifdef FTS_DOC_STATS_DEBUG
	fts_table_t		fts_table;
	static const char*	index_tables[] = {
		"DOC_ID",
		NULL
	};
#endif /* FTS_DOC_STATS_DEBUG */

	dberr_t	err = fts_drop_index_split_tables(trx, index);

	/* We only return the status of the last error. */
	if (err != DB_SUCCESS) {
		error = err;
	}

#ifdef FTS_DOC_STATS_DEBUG
	FTS_INIT_INDEX_TABLE(&fts_table, NULL, FTS_INDEX_TABLE, index);

	for (ulint i = 0; index_tables[i] != NULL; ++i) {
		char	table_name[MAX_FULL_NAME_LEN];

		fts_table.suffix = index_tables[i];

		fts_get_table_name(&fts_table, table_name);

		err = fts_drop_table(trx, table_name);

		/* We only return the status of the last error. */
		if (err != DB_SUCCESS && err != DB_FAIL) {
			error = err;
		}
	}
#endif /* FTS_DOC_STATS_DEBUG */

	return(error);
}

/****************************************************************//**
Drops FTS ancillary tables needed for supporting an FTS index
on the given table. row_mysql_lock_data_dictionary must have been called
before this.
@return DB_SUCCESS or error code */
static MY_ATTRIBUTE((nonnull, warn_unused_result))
dberr_t
fts_drop_all_index_tables(
/*======================*/
	trx_t*		trx,			/*!< in: transaction */
	fts_t*		fts)			/*!< in: fts instance */
{
	dberr_t		error = DB_SUCCESS;

	for (ulint i = 0;
	     fts->indexes != 0 && i < ib_vector_size(fts->indexes);
	     ++i) {

		dberr_t		err;
		dict_index_t*	index;

		index = static_cast<dict_index_t*>(
			ib_vector_getp(fts->indexes, i));

		err = fts_drop_index_tables(trx, index);

		if (err != DB_SUCCESS) {
			error = err;
		}
	}

	return(error);
}

/*********************************************************************//**
Drops the ancillary tables needed for supporting an FTS index on a
given table. row_mysql_lock_data_dictionary must have been called before
this.
@return DB_SUCCESS or error code */
dberr_t
fts_drop_tables(
/*============*/
	trx_t*		trx,		/*!< in: transaction */
	dict_table_t*	table)		/*!< in: table has the FTS index */
{
	dberr_t		error;
	fts_table_t	fts_table;

	FTS_INIT_FTS_TABLE(&fts_table, NULL, FTS_COMMON_TABLE, table);

	/* TODO: This is not atomic and can cause problems during recovery. */

	error = fts_drop_common_tables(trx, &fts_table);

	if (error == DB_SUCCESS) {
		error = fts_drop_all_index_tables(trx, table->fts);
	}

	return(error);
}

/** Extract only the required flags from table->flags2 for FTS Aux
tables.
@param[in]	in_flags2	Table flags2
@return extracted flags2 for FTS aux tables */
static inline
ulint
fts_get_table_flags2_for_aux_tables(
	ulint	flags2)
{
	/* Extract the file_per_table flag & temporary file flag
	from the main FTS table flags2 */
	return((flags2 & DICT_TF2_USE_FILE_PER_TABLE) |
	       (flags2 & DICT_TF2_TEMPORARY));
}

/** Create dict_table_t object for FTS Aux tables.
@param[in]	aux_table_name	FTS Aux table name
@param[in]	table		table object of FTS Index
@param[in]	n_cols		number of columns for FTS Aux table
@return table object for FTS Aux table */
static
dict_table_t*
fts_create_in_mem_aux_table(
	const char*		aux_table_name,
	const dict_table_t*	table,
	ulint			n_cols)
{
	dict_table_t*	new_table = dict_mem_table_create(
		aux_table_name, table->space, n_cols, 0, table->flags,
		fts_get_table_flags2_for_aux_tables(table->flags2));

	if (DICT_TF_HAS_SHARED_SPACE(table->flags)) {
		ut_ad(table->space == fil_space_get_id_by_name(
			table->tablespace()));
		new_table->tablespace = mem_heap_strdup(
			new_table->heap, table->tablespace);
	}

	if (DICT_TF_HAS_DATA_DIR(table->flags)) {
		ut_ad(table->data_dir_path != NULL);
		new_table->data_dir_path = mem_heap_strdup(
			new_table->heap, table->data_dir_path);
	}

	return(new_table);
}

/** Function to create on FTS common table.
@param[in,out]	trx		InnoDB transaction
@param[in]	table		Table that has FTS Index
@param[in]	fts_table_name	FTS AUX table name
@param[in]	fts_suffix	FTS AUX table suffix
@param[in]	heap		heap
@return table object if created, else NULL */
static
dict_table_t*
fts_create_one_common_table(
	trx_t*			trx,
	const dict_table_t*	table,
	const char*		fts_table_name,
	const char*		fts_suffix,
	mem_heap_t*		heap)
{
	dict_table_t*		new_table = NULL;
	dberr_t			error;
	bool			is_config = strcmp(fts_suffix, "CONFIG") == 0;

	if (!is_config) {

		new_table = fts_create_in_mem_aux_table(
			fts_table_name, table, FTS_DELETED_TABLE_NUM_COLS);

		dict_mem_table_add_col(
			new_table, heap, "doc_id", DATA_INT, DATA_UNSIGNED,
			FTS_DELETED_TABLE_COL_LEN);
	} else {
		/* Config table has different schema. */
		new_table = fts_create_in_mem_aux_table(
			fts_table_name, table, FTS_CONFIG_TABLE_NUM_COLS);

		dict_mem_table_add_col(
			new_table, heap, "key", DATA_VARCHAR, 0,
			FTS_CONFIG_TABLE_KEY_COL_LEN);

		dict_mem_table_add_col(
			new_table, heap, "value", DATA_VARCHAR, DATA_NOT_NULL,
			FTS_CONFIG_TABLE_VALUE_COL_LEN);
	}

	error = row_create_table_for_mysql(new_table, NULL, trx, false);

	if (error == DB_SUCCESS) {

		dict_index_t*	index = dict_mem_index_create(
			fts_table_name, "FTS_COMMON_TABLE_IND",
			new_table->space, DICT_UNIQUE|DICT_CLUSTERED, 1);

		if (!is_config) {
			dict_mem_index_add_field(index, "doc_id", 0);
		} else {
			dict_mem_index_add_field(index, "key", 0);
		}

		/* We save and restore trx->dict_operation because
		row_create_index_for_mysql() changes the operation to
		TRX_DICT_OP_TABLE. */
		trx_dict_op_t op = trx_get_dict_operation(trx);

		error =	row_create_index_for_mysql(index, trx, NULL, NULL);

		trx->dict_operation = op;
	}

	if (error != DB_SUCCESS) {
		trx->error_state = error;
		dict_mem_table_free(new_table);
		new_table = NULL;
		ib::warn() << "Failed to create FTS common table "
			<< fts_table_name;
	}
	return(new_table);
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
@param[in,out]	trx			transaction
@param[in]	table			table with FTS index
@param[in]	name			table name normalized
@param[in]	skip_doc_id_index	Skip index on doc id
@return DB_SUCCESS if succeed */
dberr_t
fts_create_common_tables(
	trx_t*			trx,
	const dict_table_t*	table,
	const char*		name,
	bool			skip_doc_id_index)
{
	dberr_t		error;
	que_t*		graph;
	fts_table_t	fts_table;
	mem_heap_t*	heap = mem_heap_create(1024);
	pars_info_t*	info;
	char		fts_name[MAX_FULL_NAME_LEN];
	char		full_name[sizeof(fts_common_tables) / sizeof(char*)]
				[MAX_FULL_NAME_LEN];

	dict_index_t*					index = NULL;
	trx_dict_op_t					op;
	/* common_tables vector is used for dropping FTS common tables
	on error condition. */
	std::vector<dict_table_t*>			common_tables;
	std::vector<dict_table_t*>::const_iterator	it;

	FTS_INIT_FTS_TABLE(&fts_table, NULL, FTS_COMMON_TABLE, table);

	error = fts_drop_common_tables(trx, &fts_table);

	if (error != DB_SUCCESS) {

		goto func_exit;
	}

	/* Create the FTS tables that are common to an FTS index. */
	for (ulint i = 0; fts_common_tables[i] != NULL; ++i) {

		fts_table.suffix = fts_common_tables[i];
		fts_get_table_name(&fts_table, full_name[i]);
		dict_table_t*	common_table = fts_create_one_common_table(
			trx, table, full_name[i], fts_table.suffix, heap);

		 if (common_table == NULL) {
			error = DB_ERROR;
			goto func_exit;
		} else {
			common_tables.push_back(common_table);
		}

		DBUG_EXECUTE_IF("ib_fts_aux_table_error",
			/* Return error after creating FTS_AUX_CONFIG table. */
			if (i == 4) {
				error = DB_ERROR;
				goto func_exit;
			}
		);

	}

	/* Write the default settings to the config table. */
	info = pars_info_create();

	fts_table.suffix = "CONFIG";
	fts_get_table_name(&fts_table, fts_name);
	pars_info_bind_id(info, true, "config_table", fts_name);

	graph = fts_parse_sql_no_dict_lock(
		&fts_table, info, fts_config_table_insert_values_sql);

	error = fts_eval_sql(trx, graph);

	que_graph_free(graph);

	if (error != DB_SUCCESS || skip_doc_id_index) {

		goto func_exit;
	}

	index = dict_mem_index_create(
		name, FTS_DOC_ID_INDEX_NAME, table->space,
		DICT_UNIQUE, 1);
	dict_mem_index_add_field(index, FTS_DOC_ID_COL_NAME, 0);

	op = trx_get_dict_operation(trx);

	error =	row_create_index_for_mysql(index, trx, NULL, NULL);

	trx->dict_operation = op;

func_exit:
	if (error != DB_SUCCESS) {

		for (it = common_tables.begin(); it != common_tables.end();
		     ++it) {
			row_drop_table_for_mysql(
				(*it)->name.m_name, trx, FALSE);
		}
	}

	common_tables.clear();
	mem_heap_free(heap);

	return(error);
}
/** Creates one FTS auxiliary index table for an FTS index.
@param[in,out]	trx		transaction
@param[in]	index		the index instance
@param[in]	fts_table	fts_table structure
@param[in]	heap		memory heap
@return DB_SUCCESS or error code */
static
dict_table_t*
fts_create_one_index_table(
	trx_t*			trx,
	const dict_index_t*	index,
	fts_table_t*		fts_table,
	mem_heap_t*		heap)
{
	dict_field_t*		field;
	dict_table_t*		new_table = NULL;
	char			table_name[MAX_FULL_NAME_LEN];
	dberr_t			error;
	CHARSET_INFO*		charset;

	ut_ad(index->type & DICT_FTS);

	fts_get_table_name(fts_table, table_name);

	new_table = fts_create_in_mem_aux_table(
			table_name, fts_table->table,
			FTS_AUX_INDEX_TABLE_NUM_COLS);

	field = dict_index_get_nth_field(index, 0);
	charset = fts_get_charset(field->col->prtype);

	dict_mem_table_add_col(new_table, heap, "word",
			       charset == &my_charset_latin1
			       ? DATA_VARCHAR : DATA_VARMYSQL,
			       field->col->prtype,
			       FTS_INDEX_WORD_LEN);

	dict_mem_table_add_col(new_table, heap, "first_doc_id", DATA_INT,
			       DATA_NOT_NULL | DATA_UNSIGNED,
			       FTS_INDEX_FIRST_DOC_ID_LEN);

	dict_mem_table_add_col(new_table, heap, "last_doc_id", DATA_INT,
			       DATA_NOT_NULL | DATA_UNSIGNED,
			       FTS_INDEX_LAST_DOC_ID_LEN);

	dict_mem_table_add_col(new_table, heap, "doc_count", DATA_INT,
			       DATA_NOT_NULL | DATA_UNSIGNED,
			       FTS_INDEX_DOC_COUNT_LEN);

	/* The precise type calculation is as follows:
	least signficiant byte: MySQL type code (not applicable for sys cols)
	second least : DATA_NOT_NULL | DATA_BINARY_TYPE
	third least  : the MySQL charset-collation code (DATA_MTYPE_MAX) */

	dict_mem_table_add_col(
		new_table, heap, "ilist", DATA_BLOB,
		(DATA_MTYPE_MAX << 16) | DATA_UNSIGNED | DATA_NOT_NULL,
		FTS_INDEX_ILIST_LEN);

	error = row_create_table_for_mysql(new_table, NULL, trx, false);

	if (error == DB_SUCCESS) {
		dict_index_t*	index = dict_mem_index_create(
			table_name, "FTS_INDEX_TABLE_IND", new_table->space,
			DICT_UNIQUE|DICT_CLUSTERED, 2);
		dict_mem_index_add_field(index, "word", 0);
		dict_mem_index_add_field(index, "first_doc_id", 0);

		trx_dict_op_t op = trx_get_dict_operation(trx);

		error =	row_create_index_for_mysql(index, trx, NULL, NULL);

		trx->dict_operation = op;
	}

	if (error != DB_SUCCESS) {
		trx->error_state = error;
		dict_mem_table_free(new_table);
		new_table = NULL;
		ib::warn() << "Failed to create FTS index table "
			<< table_name;
	}

	return(new_table);
}

/** Create auxiliary index tables for an FTS index.
@param[in,out]	trx		transaction
@param[in]	index		the index instance
@param[in]	table_name	table name
@param[in]	table_id	the table id
@return DB_SUCCESS or error code */
dberr_t
fts_create_index_tables_low(
	trx_t*			trx,
	const dict_index_t*	index,
	const char*		table_name,
	table_id_t		table_id)
{
	ulint		i;
	fts_table_t	fts_table;
	dberr_t		error = DB_SUCCESS;
	mem_heap_t*	heap = mem_heap_create(1024);

	fts_table.type = FTS_INDEX_TABLE;
	fts_table.index_id = index->id;
	fts_table.table_id = table_id;
	fts_table.parent = table_name;
	fts_table.table = index->table;

#ifdef FTS_DOC_STATS_DEBUG
	/* Create the FTS auxiliary tables that are specific
	to an FTS index. */
	info = pars_info_create();

	fts_table.suffix = "DOC_ID";
	fts_get_table_name(&fts_table, fts_name);

	pars_info_bind_id(info, true, "doc_id_table", fts_name);

	graph = fts_parse_sql_no_dict_lock(NULL, info,
					   fts_create_index_tables_sql);

	error = fts_eval_sql(trx, graph);
	que_graph_free(graph);
#endif /* FTS_DOC_STATS_DEBUG */

	/* aux_idx_tables vector is used for dropping FTS AUX INDEX
	tables on error condition. */
	std::vector<dict_table_t*>			aux_idx_tables;
	std::vector<dict_table_t*>::const_iterator	it;

	for (i = 0; i < FTS_NUM_AUX_INDEX && error == DB_SUCCESS; ++i) {
		dict_table_t*	new_table;

		/* Create the FTS auxiliary tables that are specific
		to an FTS index. We need to preserve the table_id %s
		which fts_parse_sql_no_dict_lock() will fill in for us. */
		fts_table.suffix = fts_get_suffix(i);

		new_table = fts_create_one_index_table(
			trx, index, &fts_table, heap);

		if (new_table == NULL) {
			error = DB_FAIL;
			break;
		} else {
			aux_idx_tables.push_back(new_table);
		}

		DBUG_EXECUTE_IF("ib_fts_index_table_error",
			/* Return error after creating FTS_INDEX_5
			aux table. */
			if (i == 4) {
				error = DB_FAIL;
				break;
			}
		);
	}

	if (error != DB_SUCCESS) {

		for (it = aux_idx_tables.begin(); it != aux_idx_tables.end();
		     ++it) {
			row_drop_table_for_mysql(
				(*it)->name.m_name, trx, FALSE);
		}
	}

	aux_idx_tables.clear();
	mem_heap_free(heap);

	return(error);
}

/** Creates the column specific ancillary tables needed for supporting an
FTS index on the given table. row_mysql_lock_data_dictionary must have
been called before this.

All FTS AUX Index tables have the following schema.
CREAT TABLE $FTS_PREFIX_INDEX_[1-6](
	word		VARCHAR(FTS_MAX_WORD_LEN),
	first_doc_id	INT NOT NULL,
	last_doc_id	UNSIGNED NOT NULL,
	doc_count	UNSIGNED INT NOT NULL,
	ilist		VARBINARY NOT NULL,
	UNIQUE CLUSTERED INDEX ON (word, first_doc_id))
@param[in,out]	trx	transaction
@param[in]	index	index instance
@return DB_SUCCESS or error code */
dberr_t
fts_create_index_tables(
	trx_t*			trx,
	const dict_index_t*	index)
{
	dberr_t		err;
	dict_table_t*	table;

	table = dict_table_get_low(index->table_name);
	ut_a(table != NULL);

	err = fts_create_index_tables_low(
		trx, index, table->name.m_name, table->id);

	if (err == DB_SUCCESS) {
		trx_commit(trx);
	}

	return(err);
}
#if 0
/******************************************************************//**
Return string representation of state. */
static
const char*
fts_get_state_str(
/*==============*/
				/* out: string representation of state */
	fts_row_state	state)	/*!< in: state */
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

/******************************************************************//**
Calculate the new state of a row given the existing state and a new event.
@return new state of row */
static
fts_row_state
fts_trx_row_get_new_state(
/*======================*/
	fts_row_state	old_state,		/*!< in: existing state of row */
	fts_row_state	event)			/*!< in: new event */
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
		/* I */	{ FTS_INVALID, FTS_INSERT,  FTS_NOTHING, FTS_INVALID },
		/* M */	{ FTS_INVALID, FTS_MODIFY,  FTS_DELETE,  FTS_INVALID },
		/* D */	{ FTS_MODIFY,  FTS_INVALID, FTS_INVALID, FTS_INVALID },
		/* N */	{ FTS_INVALID, FTS_INVALID, FTS_INVALID, FTS_INVALID }
	};

	fts_row_state result;

	ut_a(old_state < FTS_INVALID);
	ut_a(event < FTS_INVALID);

	result = table[(int) old_state][(int) event];
	ut_a(result != FTS_INVALID);

	return(result);
}

/******************************************************************//**
Create a savepoint instance.
@return savepoint instance */
static
fts_savepoint_t*
fts_savepoint_create(
/*=================*/
	ib_vector_t*	savepoints,		/*!< out: InnoDB transaction */
	const char*	name,			/*!< in: savepoint name */
	mem_heap_t*	heap)			/*!< in: heap */
{
	fts_savepoint_t*	savepoint;

	savepoint = static_cast<fts_savepoint_t*>(
		ib_vector_push(savepoints, NULL));

	memset(savepoint, 0x0, sizeof(*savepoint));

	if (name) {
		savepoint->name = mem_heap_strdup(heap, name);
	}

	savepoint->tables = rbt_create(
		sizeof(fts_trx_table_t*), fts_trx_table_cmp);

	return(savepoint);
}

/******************************************************************//**
Create an FTS trx.
@return FTS trx */
static
fts_trx_t*
fts_trx_create(
/*===========*/
	trx_t*	trx)				/*!< in/out: InnoDB
						transaction */
{
	fts_trx_t*		ftt;
	ib_alloc_t*		heap_alloc;
	mem_heap_t*		heap = mem_heap_create(1024);
	trx_named_savept_t*	savep;

	ut_a(trx->fts_trx == NULL);

	ftt = static_cast<fts_trx_t*>(mem_heap_alloc(heap, sizeof(fts_trx_t)));
	ftt->trx = trx;
	ftt->heap = heap;

	heap_alloc = ib_heap_allocator_create(heap);

	ftt->savepoints = static_cast<ib_vector_t*>(ib_vector_create(
		heap_alloc, sizeof(fts_savepoint_t), 4));

	ftt->last_stmt = static_cast<ib_vector_t*>(ib_vector_create(
		heap_alloc, sizeof(fts_savepoint_t), 4));

	/* Default instance has no name and no heap. */
	fts_savepoint_create(ftt->savepoints, NULL, NULL);
	fts_savepoint_create(ftt->last_stmt, NULL, NULL);

	/* Copy savepoints that already set before. */
	for (savep = UT_LIST_GET_FIRST(trx->trx_savepoints);
	     savep != NULL;
	     savep = UT_LIST_GET_NEXT(trx_savepoints, savep)) {

		fts_savepoint_take(trx, ftt, savep->name);
	}

	return(ftt);
}

/******************************************************************//**
Create an FTS trx table.
@return FTS trx table */
static
fts_trx_table_t*
fts_trx_table_create(
/*=================*/
	fts_trx_t*	fts_trx,		/*!< in: FTS trx */
	dict_table_t*	table)			/*!< in: table */
{
	fts_trx_table_t*	ftt;

	ftt = static_cast<fts_trx_table_t*>(
		mem_heap_alloc(fts_trx->heap, sizeof(*ftt)));

	memset(ftt, 0x0, sizeof(*ftt));

	ftt->table = table;
	ftt->fts_trx = fts_trx;

	ftt->rows = rbt_create(sizeof(fts_trx_row_t), fts_trx_row_doc_id_cmp);

	return(ftt);
}

/******************************************************************//**
Clone an FTS trx table.
@return FTS trx table */
static
fts_trx_table_t*
fts_trx_table_clone(
/*=================*/
	const fts_trx_table_t*	ftt_src)	/*!< in: FTS trx */
{
	fts_trx_table_t*	ftt;

	ftt = static_cast<fts_trx_table_t*>(
		mem_heap_alloc(ftt_src->fts_trx->heap, sizeof(*ftt)));

	memset(ftt, 0x0, sizeof(*ftt));

	ftt->table = ftt_src->table;
	ftt->fts_trx = ftt_src->fts_trx;

	ftt->rows = rbt_create(sizeof(fts_trx_row_t), fts_trx_row_doc_id_cmp);

	/* Copy the rb tree values to the new savepoint. */
	rbt_merge_uniq(ftt->rows, ftt_src->rows);

	/* These are only added on commit. At this stage we only have
	the updated row state. */
	ut_a(ftt_src->added_doc_ids == NULL);

	return(ftt);
}

/******************************************************************//**
Initialize the FTS trx instance.
@return FTS trx instance */
static
fts_trx_table_t*
fts_trx_init(
/*=========*/
	trx_t*			trx,		/*!< in: transaction */
	dict_table_t*		table,		/*!< in: FTS table instance */
	ib_vector_t*		savepoints)	/*!< in: Savepoints */
{
	fts_trx_table_t*	ftt;
	ib_rbt_bound_t		parent;
	ib_rbt_t*		tables;
	fts_savepoint_t*	savepoint;

	savepoint = static_cast<fts_savepoint_t*>(ib_vector_last(savepoints));

	tables = savepoint->tables;
	rbt_search_cmp(tables, &parent, &table->id, fts_trx_table_id_cmp, NULL);

	if (parent.result == 0) {
		fts_trx_table_t**	fttp;

		fttp = rbt_value(fts_trx_table_t*, parent.last);
		ftt = *fttp;
	} else {
		ftt = fts_trx_table_create(trx->fts_trx, table);
		rbt_add_node(tables, &parent, &ftt);
	}

	ut_a(ftt->table == table);

	return(ftt);
}

/******************************************************************//**
Notify the FTS system about an operation on an FTS-indexed table. */
static
void
fts_trx_table_add_op(
/*=================*/
	fts_trx_table_t*ftt,			/*!< in: FTS trx table */
	doc_id_t	doc_id,			/*!< in: doc id */
	fts_row_state	state,			/*!< in: state of the row */
	ib_vector_t*	fts_indexes)		/*!< in: FTS indexes affected */
{
	ib_rbt_t*	rows;
	ib_rbt_bound_t	parent;

	rows = ftt->rows;
	rbt_search(rows, &parent, &doc_id);

	/* Row id found, update state, and if new state is FTS_NOTHING,
	we delete the row from our tree. */
	if (parent.result == 0) {
		fts_trx_row_t*	row = rbt_value(fts_trx_row_t, parent.last);

		row->state = fts_trx_row_get_new_state(row->state, state);

		if (row->state == FTS_NOTHING) {
			if (row->fts_indexes) {
				ib_vector_free(row->fts_indexes);
			}

			ut_free(rbt_remove_node(rows, parent.last));
			row = NULL;
		} else if (row->fts_indexes != NULL) {
			ib_vector_free(row->fts_indexes);
			row->fts_indexes = fts_indexes;
		}

	} else { /* Row-id not found, create a new one. */
		fts_trx_row_t	row;

		row.doc_id = doc_id;
		row.state = state;
		row.fts_indexes = fts_indexes;

		rbt_add_node(rows, &parent, &row);
	}
}

/******************************************************************//**
Notify the FTS system about an operation on an FTS-indexed table. */
void
fts_trx_add_op(
/*===========*/
	trx_t*		trx,			/*!< in: InnoDB transaction */
	dict_table_t*	table,			/*!< in: table */
	doc_id_t	doc_id,			/*!< in: new doc id */
	fts_row_state	state,			/*!< in: state of the row */
	ib_vector_t*	fts_indexes)		/*!< in: FTS indexes affected
						(NULL=all) */
{
	fts_trx_table_t*	tran_ftt;
	fts_trx_table_t*	stmt_ftt;

	if (!trx->fts_trx) {
		trx->fts_trx = fts_trx_create(trx);
	}

	tran_ftt = fts_trx_init(trx, table, trx->fts_trx->savepoints);
	stmt_ftt = fts_trx_init(trx, table, trx->fts_trx->last_stmt);

	fts_trx_table_add_op(tran_ftt, doc_id, state, fts_indexes);
	fts_trx_table_add_op(stmt_ftt, doc_id, state, fts_indexes);
}

/******************************************************************//**
Fetch callback that converts a textual document id to a binary value and
stores it in the given place.
@return always returns NULL */
static
ibool
fts_fetch_store_doc_id(
/*===================*/
	void*		row,			/*!< in: sel_node_t* */
	void*		user_arg)		/*!< in: doc_id_t* to store
						doc_id in */
{
	int		n_parsed;
	sel_node_t*	node = static_cast<sel_node_t*>(row);
	doc_id_t*	doc_id = static_cast<doc_id_t*>(user_arg);
	dfield_t*	dfield = que_node_get_val(node->select_list);
	dtype_t*	type = dfield_get_type(dfield);
	ulint		len = dfield_get_len(dfield);

	char		buf[32];

	ut_a(dtype_get_mtype(type) == DATA_VARCHAR);
	ut_a(len > 0 && len < sizeof(buf));

	memcpy(buf, dfield_get_data(dfield), len);
	buf[len] = '\0';

	n_parsed = sscanf(buf, FTS_DOC_ID_FORMAT, doc_id);
	ut_a(n_parsed == 1);

	return(FALSE);
}

#ifdef FTS_CACHE_SIZE_DEBUG
/******************************************************************//**
Get the max cache size in bytes. If there is an error reading the
value we simply print an error message here and return the default
value to the caller.
@return max cache size in bytes */
static
ulint
fts_get_max_cache_size(
/*===================*/
	trx_t*		trx,			/*!< in: transaction */
	fts_table_t*	fts_table)		/*!< in: table instance */
{
	dberr_t		error;
	fts_string_t	value;
	ulint		cache_size_in_mb;

	/* Set to the default value. */
	cache_size_in_mb = FTS_CACHE_SIZE_LOWER_LIMIT_IN_MB;

	/* We set the length of value to the max bytes it can hold. This
	information is used by the callback that reads the value. */
	value.f_n_char = 0;
	value.f_len = FTS_MAX_CONFIG_VALUE_LEN;
	value.f_str = ut_malloc_nokey(value.f_len + 1);

	error = fts_config_get_value(
		trx, fts_table, FTS_MAX_CACHE_SIZE_IN_MB, &value);

	if (error == DB_SUCCESS) {

		value.f_str[value.f_len] = 0;
		cache_size_in_mb = strtoul((char*) value.f_str, NULL, 10);

		if (cache_size_in_mb > FTS_CACHE_SIZE_UPPER_LIMIT_IN_MB) {

			ib::warn() << "FTS max cache size ("
				<< cache_size_in_mb << ") out of range."
				" Minimum value is "
				<< FTS_CACHE_SIZE_LOWER_LIMIT_IN_MB
				<< "MB and the maximum value is "
				<< FTS_CACHE_SIZE_UPPER_LIMIT_IN_MB
				<< "MB, setting cache size to upper limit";

			cache_size_in_mb = FTS_CACHE_SIZE_UPPER_LIMIT_IN_MB;

		} else if  (cache_size_in_mb
			    < FTS_CACHE_SIZE_LOWER_LIMIT_IN_MB) {

			ib::warn() << "FTS max cache size ("
				<< cache_size_in_mb << ") out of range."
				" Minimum value is "
				<< FTS_CACHE_SIZE_LOWER_LIMIT_IN_MB
				<< "MB and the maximum value is"
				<< FTS_CACHE_SIZE_UPPER_LIMIT_IN_MB
				<< "MB, setting cache size to lower limit";

			cache_size_in_mb = FTS_CACHE_SIZE_LOWER_LIMIT_IN_MB;
		}
	} else {
		ib::error() << "(" << ut_strerr(error) << ") reading max"
			" cache config value from config table";
	}

	ut_free(value.f_str);

	return(cache_size_in_mb * 1024 * 1024);
}
#endif

#ifdef FTS_DOC_STATS_DEBUG
/*********************************************************************//**
Get the total number of words in the FTS for a particular FTS index.
@return DB_SUCCESS if all OK else error code */
dberr_t
fts_get_total_word_count(
/*=====================*/
	trx_t*		trx,			/*!< in: transaction */
	dict_index_t*	index,			/*!< in: for this index */
	ulint*		total)			/* out: total words */
{
	dberr_t		error;
	fts_string_t	value;

	*total = 0;

	/* We set the length of value to the max bytes it can hold. This
	information is used by the callback that reads the value. */
	value.f_n_char = 0;
	value.f_len = FTS_MAX_CONFIG_VALUE_LEN;
	value.f_str = static_cast<byte*>(ut_malloc_nokey(value.f_len + 1));

	error = fts_config_get_index_value(
		trx, index, FTS_TOTAL_WORD_COUNT, &value);

	if (error == DB_SUCCESS) {

		value.f_str[value.f_len] = 0;
		*total = strtoul((char*) value.f_str, NULL, 10);
	} else {
		ib::error() << "(" << ut_strerr(error) << ") reading total"
			" words value from config table";
	}

	ut_free(value.f_str);

	return(error);
}
#endif /* FTS_DOC_STATS_DEBUG */

/*********************************************************************//**
Update the next and last Doc ID in the CONFIG table to be the input
"doc_id" value (+ 1). We would do so after each FTS index build or
table truncate */
void
fts_update_next_doc_id(
/*===================*/
	trx_t*			trx,		/*!< in/out: transaction */
	const dict_table_t*	table,		/*!< in: table */
	const char*		table_name,	/*!< in: table name, or NULL */
	doc_id_t		doc_id)		/*!< in: DOC ID to set */
{
	table->fts->cache->synced_doc_id = doc_id;
	table->fts->cache->next_doc_id = doc_id + 1;

	table->fts->cache->first_doc_id = table->fts->cache->next_doc_id;

	fts_update_sync_doc_id(
		table, table_name, table->fts->cache->synced_doc_id, trx);

}

/*********************************************************************//**
Get the next available document id.
@return DB_SUCCESS if OK */
dberr_t
fts_get_next_doc_id(
/*================*/
	const dict_table_t*	table,		/*!< in: table */
	doc_id_t*		doc_id)		/*!< out: new document id */
{
	fts_cache_t*	cache = table->fts->cache;

	/* If the Doc ID system has not yet been initialized, we
	will consult the CONFIG table and user table to re-establish
	the initial value of the Doc ID */
	if (cache->first_doc_id == FTS_NULL_DOC_ID) {
		fts_init_doc_id(table);
	}

	if (!DICT_TF2_FLAG_IS_SET(table, DICT_TF2_FTS_HAS_DOC_ID)) {
		*doc_id = FTS_NULL_DOC_ID;
		return(DB_SUCCESS);
	}

	mutex_enter(&cache->doc_id_lock);
	*doc_id = ++cache->next_doc_id;
	mutex_exit(&cache->doc_id_lock);

	return(DB_SUCCESS);
}

/*********************************************************************//**
This function fetch the Doc ID from CONFIG table, and compare with
the Doc ID supplied. And store the larger one to the CONFIG table.
@return DB_SUCCESS if OK */
static MY_ATTRIBUTE((nonnull))
dberr_t
fts_cmp_set_sync_doc_id(
/*====================*/
	const dict_table_t*	table,		/*!< in: table */
	doc_id_t		doc_id_cmp,	/*!< in: Doc ID to compare */
	ibool			read_only,	/*!< in: TRUE if read the
						synced_doc_id only */
	doc_id_t*		doc_id)		/*!< out: larger document id
						after comparing "doc_id_cmp"
						to the one stored in CONFIG
						table */
{
	trx_t*		trx;
	pars_info_t*	info;
	dberr_t		error;
	fts_table_t	fts_table;
	que_t*		graph = NULL;
	fts_cache_t*	cache = table->fts->cache;
	char		table_name[MAX_FULL_NAME_LEN];
retry:
	ut_a(table->fts->doc_col != ULINT_UNDEFINED);

	fts_table.suffix = "CONFIG";
	fts_table.table_id = table->id;
	fts_table.type = FTS_COMMON_TABLE;
	fts_table.table = table;

	fts_table.parent = table->name.m_name;

	trx = trx_allocate_for_background();

	trx->op_info = "update the next FTS document id";

	info = pars_info_create();

	pars_info_bind_function(
		info, "my_func", fts_fetch_store_doc_id, doc_id);

	fts_get_table_name(&fts_table, table_name);
	pars_info_bind_id(info, true, "config_table", table_name);

	graph = fts_parse_sql(
		&fts_table, info,
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

	fts_que_graph_free_check_lock(&fts_table, NULL, graph);

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
		cache->synced_doc_id = ut_max(doc_id_cmp, *doc_id);
	}

	mutex_enter(&cache->doc_id_lock);
	/* For each sync operation, we will add next_doc_id by 1,
	so to mark a sync operation */
	if (cache->next_doc_id < cache->synced_doc_id + 1) {
		cache->next_doc_id = cache->synced_doc_id + 1;
	}
	mutex_exit(&cache->doc_id_lock);

	if (doc_id_cmp > *doc_id) {
		error = fts_update_sync_doc_id(
			table, table->name.m_name, cache->synced_doc_id, trx);
	}

	*doc_id = cache->next_doc_id;

func_exit:

	if (error == DB_SUCCESS) {
		fts_sql_commit(trx);
	} else {
		*doc_id = 0;

		ib::error() << "(" << ut_strerr(error) << ") while getting"
			" next doc id.";
		fts_sql_rollback(trx);

		if (error == DB_DEADLOCK) {
			os_thread_sleep(FTS_DEADLOCK_RETRY_WAIT);
			goto retry;
		}
	}

	trx_free_for_background(trx);

	return(error);
}

/*********************************************************************//**
Update the last document id. This function could create a new
transaction to update the last document id.
@return DB_SUCCESS if OK */
static
dberr_t
fts_update_sync_doc_id(
/*===================*/
	const dict_table_t*	table,		/*!< in: table */
	const char*		table_name,	/*!< in: table name, or NULL */
	doc_id_t		doc_id,		/*!< in: last document id */
	trx_t*			trx)		/*!< in: update trx, or NULL */
{
	byte		id[FTS_MAX_ID_LEN];
	pars_info_t*	info;
	fts_table_t	fts_table;
	ulint		id_len;
	que_t*		graph = NULL;
	dberr_t		error;
	ibool		local_trx = FALSE;
	fts_cache_t*	cache = table->fts->cache;
	char		fts_name[MAX_FULL_NAME_LEN];

	fts_table.suffix = "CONFIG";
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
		local_trx = TRUE;
	}

	info = pars_info_create();

	id_len = ut_snprintf(
		(char*) id, sizeof(id), FTS_DOC_ID_FORMAT, doc_id + 1);

	pars_info_bind_varchar_literal(info, "doc_id", id, id_len);

	fts_get_table_name(&fts_table, fts_name);
	pars_info_bind_id(info, true, "table_name", fts_name);

	graph = fts_parse_sql(
		&fts_table, info,
		"BEGIN"
		" UPDATE $table_name SET value = :doc_id"
		" WHERE key = 'synced_doc_id';");

	error = fts_eval_sql(trx, graph);

	fts_que_graph_free_check_lock(&fts_table, NULL, graph);

	if (local_trx) {
		if (error == DB_SUCCESS) {
			fts_sql_commit(trx);
			cache->synced_doc_id = doc_id;
		} else {

			ib::error() << "(" << ut_strerr(error) << ") while"
				" updating last doc id.";

			fts_sql_rollback(trx);
		}
		trx_free_for_background(trx);
	}

	return(error);
}

/*********************************************************************//**
Create a new fts_doc_ids_t.
@return new fts_doc_ids_t */
fts_doc_ids_t*
fts_doc_ids_create(void)
/*====================*/
{
	fts_doc_ids_t*	fts_doc_ids;
	mem_heap_t*	heap = mem_heap_create(512);

	fts_doc_ids = static_cast<fts_doc_ids_t*>(
		mem_heap_alloc(heap, sizeof(*fts_doc_ids)));

	fts_doc_ids->self_heap = ib_heap_allocator_create(heap);

	fts_doc_ids->doc_ids = static_cast<ib_vector_t*>(ib_vector_create(
		fts_doc_ids->self_heap, sizeof(fts_update_t), 32));

	return(fts_doc_ids);
}

/*********************************************************************//**
Free a fts_doc_ids_t. */
void
fts_doc_ids_free(
/*=============*/
	fts_doc_ids_t*	fts_doc_ids)
{
	mem_heap_t*	heap = static_cast<mem_heap_t*>(
		fts_doc_ids->self_heap->arg);

	memset(fts_doc_ids, 0, sizeof(*fts_doc_ids));

	mem_heap_free(heap);
}

/*********************************************************************//**
Do commit-phase steps necessary for the insertion of a new row.
@return DB_SUCCESS or error code */
void
fts_add(
/*====*/
	fts_trx_table_t*ftt,			/*!< in: FTS trx table */
	fts_trx_row_t*	row)			/*!< in: row */
{
	dict_table_t*	table = ftt->table;
	doc_id_t	doc_id = row->doc_id;

	ut_a(row->state == FTS_INSERT || row->state == FTS_MODIFY);

	fts_add_doc_by_id(ftt, doc_id, row->fts_indexes);

	mutex_enter(&table->fts->cache->deleted_lock);
	++table->fts->cache->added;
	mutex_exit(&table->fts->cache->deleted_lock);

	if (!DICT_TF2_FLAG_IS_SET(table, DICT_TF2_FTS_HAS_DOC_ID)
	    && doc_id >= table->fts->cache->next_doc_id) {
		table->fts->cache->next_doc_id = doc_id + 1;
	}
}

/*********************************************************************//**
Do commit-phase steps necessary for the deletion of a row.
@return DB_SUCCESS or error code */
static MY_ATTRIBUTE((nonnull, warn_unused_result))
dberr_t
fts_delete(
/*=======*/
	fts_trx_table_t*ftt,			/*!< in: FTS trx table */
	fts_trx_row_t*	row)			/*!< in: row */
{
	que_t*		graph;
	fts_table_t	fts_table;
	dberr_t		error = DB_SUCCESS;
	doc_id_t	write_doc_id;
	dict_table_t*	table = ftt->table;
	doc_id_t	doc_id = row->doc_id;
	trx_t*		trx = ftt->fts_trx->trx;
	pars_info_t*	info = pars_info_create();
	fts_cache_t*	cache = table->fts->cache;

	/* we do not index Documents whose Doc ID value is 0 */
	if (doc_id == FTS_NULL_DOC_ID) {
		ut_ad(!DICT_TF2_FLAG_IS_SET(table, DICT_TF2_FTS_HAS_DOC_ID));
		return(error);
	}

	ut_a(row->state == FTS_DELETE || row->state == FTS_MODIFY);

	FTS_INIT_FTS_TABLE(&fts_table, "DELETED", FTS_COMMON_TABLE, table);

	/* Convert to "storage" byte order. */
	fts_write_doc_id((byte*) &write_doc_id, doc_id);
	fts_bind_doc_id(info, "doc_id", &write_doc_id);

	/* It is possible we update a record that has not yet been sync-ed
	into cache from last crash (delete Doc will not initialize the
	sync). Avoid any added counter accounting until the FTS cache
	is re-established and sync-ed */
	if (table->fts->fts_status & ADDED_TABLE_SYNCED
	    && doc_id > cache->synced_doc_id) {
		mutex_enter(&table->fts->cache->deleted_lock);

		/* The Doc ID could belong to those left in
		ADDED table from last crash. So need to check
		if it is less than first_doc_id when we initialize
		the Doc ID system after reboot */
		if (doc_id >= table->fts->cache->first_doc_id
		    && table->fts->cache->added > 0) {
			--table->fts->cache->added;
		}

		mutex_exit(&table->fts->cache->deleted_lock);

		/* Only if the row was really deleted. */
		ut_a(row->state == FTS_DELETE || row->state == FTS_MODIFY);
	}

	/* Note the deleted document for OPTIMIZE to purge. */
	if (error == DB_SUCCESS) {
		char	table_name[MAX_FULL_NAME_LEN];

		trx->op_info = "adding doc id to FTS DELETED";

		info->graph_owns_us = TRUE;

		fts_table.suffix = "DELETED";

		fts_get_table_name(&fts_table, table_name);
		pars_info_bind_id(info, true, "deleted", table_name);

		graph = fts_parse_sql(
			&fts_table,
			info,
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

	return(error);
}

/*********************************************************************//**
Do commit-phase steps necessary for the modification of a row.
@return DB_SUCCESS or error code */
static MY_ATTRIBUTE((nonnull, warn_unused_result))
dberr_t
fts_modify(
/*=======*/
	fts_trx_table_t*	ftt,		/*!< in: FTS trx table */
	fts_trx_row_t*		row)		/*!< in: row */
{
	dberr_t	error;

	ut_a(row->state == FTS_MODIFY);

	error = fts_delete(ftt, row);

	if (error == DB_SUCCESS) {
		fts_add(ftt, row);
	}

	return(error);
}

/*********************************************************************//**
Create a new document id.
@return DB_SUCCESS if all went well else error */
dberr_t
fts_create_doc_id(
/*==============*/
	dict_table_t*	table,		/*!< in: row is of this table. */
	dtuple_t*	row,		/* in/out: add doc id value to this
					row. This is the current row that is
					being inserted. */
	mem_heap_t*	heap)		/*!< in: heap */
{
	doc_id_t	doc_id;
	dberr_t		error = DB_SUCCESS;

	ut_a(table->fts->doc_col != ULINT_UNDEFINED);

	if (!DICT_TF2_FLAG_IS_SET(table, DICT_TF2_FTS_HAS_DOC_ID)) {
		if (table->fts->cache->first_doc_id == FTS_NULL_DOC_ID) {
			error = fts_get_next_doc_id(table, &doc_id);
		}
		return(error);
	}

	error = fts_get_next_doc_id(table, &doc_id);

	if (error == DB_SUCCESS) {
		dfield_t*	dfield;
		doc_id_t*	write_doc_id;

		ut_a(doc_id > 0);

		dfield = dtuple_get_nth_field(row, table->fts->doc_col);
		write_doc_id = static_cast<doc_id_t*>(
			mem_heap_alloc(heap, sizeof(*write_doc_id)));

		ut_a(doc_id != FTS_NULL_DOC_ID);
		ut_a(sizeof(doc_id) == dfield->type.len);
		fts_write_doc_id((byte*) write_doc_id, doc_id);

		dfield_set_data(dfield, write_doc_id, sizeof(*write_doc_id));
	}

	return(error);
}

/*********************************************************************//**
The given transaction is about to be committed; do whatever is necessary
from the FTS system's POV.
@return DB_SUCCESS or error code */
static MY_ATTRIBUTE((nonnull, warn_unused_result))
dberr_t
fts_commit_table(
/*=============*/
	fts_trx_table_t*	ftt)		/*!< in: FTS table to commit*/
{
	const ib_rbt_node_t*	node;
	ib_rbt_t*		rows;
	dberr_t			error = DB_SUCCESS;
	fts_cache_t*		cache = ftt->table->fts->cache;
	trx_t*			trx = trx_allocate_for_background();

	rows = ftt->rows;

	ftt->fts_trx->trx = trx;

	if (cache->get_docs == NULL) {
		rw_lock_x_lock(&cache->init_lock);
		if (cache->get_docs == NULL) {
			cache->get_docs = fts_get_docs_create(cache);
		}
		rw_lock_x_unlock(&cache->init_lock);
	}

	for (node = rbt_first(rows);
	     node != NULL && error == DB_SUCCESS;
	     node = rbt_next(rows, node)) {

		fts_trx_row_t*	row = rbt_value(fts_trx_row_t, node);

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

	return(error);
}

/*********************************************************************//**
The given transaction is about to be committed; do whatever is necessary
from the FTS system's POV.
@return DB_SUCCESS or error code */
dberr_t
fts_commit(
/*=======*/
	trx_t*	trx)				/*!< in: transaction */
{
	const ib_rbt_node_t*	node;
	dberr_t			error;
	ib_rbt_t*		tables;
	fts_savepoint_t*	savepoint;

	savepoint = static_cast<fts_savepoint_t*>(
		ib_vector_last(trx->fts_trx->savepoints));
	tables = savepoint->tables;

	for (node = rbt_first(tables), error = DB_SUCCESS;
	     node != NULL && error == DB_SUCCESS;
	     node = rbt_next(tables, node)) {

		fts_trx_table_t**	ftt;

		ftt = rbt_value(fts_trx_table_t*, node);

		error = fts_commit_table(*ftt);
	}

	return(error);
}

/*********************************************************************//**
Initialize a document. */
void
fts_doc_init(
/*=========*/
	fts_doc_t*	doc)			/*!< in: doc to initialize */
{
	mem_heap_t*	heap = mem_heap_create(32);

	memset(doc, 0, sizeof(*doc));

	doc->self_heap = ib_heap_allocator_create(heap);
}

/*********************************************************************//**
Free document. */
void
fts_doc_free(
/*=========*/
	fts_doc_t*	doc)			/*!< in: document */
{
	mem_heap_t*	heap = static_cast<mem_heap_t*>(doc->self_heap->arg);

	if (doc->tokens) {
		rbt_free(doc->tokens);
	}

	ut_d(memset(doc, 0, sizeof(*doc)));

	mem_heap_free(heap);
}

/*********************************************************************//**
Callback function for fetch that stores a row id to the location pointed.
The column's type must be DATA_FIXBINARY, DATA_BINARY_TYPE, length = 8.
@return always returns NULL */
void*
fts_fetch_row_id(
/*=============*/
	void*	row,				/*!< in: sel_node_t* */
	void*	user_arg)			/*!< in: data pointer */
{
	sel_node_t*	node = static_cast<sel_node_t*>(row);

	dfield_t*	dfield = que_node_get_val(node->select_list);
	dtype_t*	type = dfield_get_type(dfield);
	ulint		len = dfield_get_len(dfield);

	ut_a(dtype_get_mtype(type) == DATA_FIXBINARY);
	ut_a(dtype_get_prtype(type) & DATA_BINARY_TYPE);
	ut_a(len == 8);

	memcpy(user_arg, dfield_get_data(dfield), 8);

	return(NULL);
}

/*********************************************************************//**
Callback function for fetch that stores the text of an FTS document,
converting each column to UTF-16.
@return always FALSE */
ibool
fts_query_expansion_fetch_doc(
/*==========================*/
	void*		row,			/*!< in: sel_node_t* */
	void*		user_arg)		/*!< in: fts_doc_t* */
{
	que_node_t*	exp;
	sel_node_t*	node = static_cast<sel_node_t*>(row);
	fts_doc_t*	result_doc = static_cast<fts_doc_t*>(user_arg);
	dfield_t*	dfield;
	ulint		len;
	ulint		doc_len;
	fts_doc_t	doc;
	CHARSET_INFO*	doc_charset = NULL;
	ulint		field_no = 0;

	len = 0;

	fts_doc_init(&doc);
	doc.found = TRUE;

	exp = node->select_list;
	doc_len = 0;

	doc_charset  = result_doc->charset;

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

			doc.text.f_str = static_cast<byte*>(
				dfield_get_data(dfield));

			doc.text.f_len = len;
		}

		if (field_no == 0) {
			fts_tokenize_document(&doc, result_doc,
					      result_doc->parser);
		} else {
			fts_tokenize_document_next(&doc, doc_len, result_doc,
						   result_doc->parser);
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

	return(FALSE);
}

/*********************************************************************//**
fetch and tokenize the document. */
static
void
fts_fetch_doc_from_rec(
/*===================*/
	fts_get_doc_t*  get_doc,	/*!< in: FTS index's get_doc struct */
	dict_index_t*	clust_index,	/*!< in: cluster index */
	btr_pcur_t*	pcur,		/*!< in: cursor whose position
					has been stored */
	ulint*		offsets,	/*!< in: offsets */
	fts_doc_t*	doc)		/*!< out: fts doc to hold parsed
					documents */
{
	dict_index_t*		index;
	dict_table_t*		table;
	const rec_t*		clust_rec;
	ulint			num_field;
	const dict_field_t*	ifield;
	const dict_col_t*	col;
	ulint			clust_pos;
	ulint			i;
	ulint			doc_len = 0;
	ulint			processed_doc = 0;
	st_mysql_ftparser*	parser;

	if (!get_doc) {
		return;
	}

	index = get_doc->index_cache->index;
	table = get_doc->index_cache->index->table;
	parser = get_doc->index_cache->index->parser;

	clust_rec = btr_pcur_get_rec(pcur);

	num_field = dict_index_get_n_fields(index);

	for (i = 0; i < num_field; i++) {
		ifield = dict_index_get_nth_field(index, i);
		col = dict_field_get_col(ifield);
		clust_pos = dict_col_get_clust_pos(col, clust_index);

		if (!get_doc->index_cache->charset) {
			get_doc->index_cache->charset = fts_get_charset(
				ifield->col->prtype);
		}

		if (rec_offs_nth_extern(offsets, clust_pos)) {
			doc->text.f_str =
				btr_rec_copy_externally_stored_field(
					clust_rec, offsets,
					dict_table_page_size(table),
					clust_pos, &doc->text.f_len,
					static_cast<mem_heap_t*>(
						doc->self_heap->arg));
		} else {
			doc->text.f_str = (byte*) rec_get_nth_field(
				clust_rec, offsets, clust_pos,
				&doc->text.f_len);
		}

		doc->found = TRUE;
		doc->charset = get_doc->index_cache->charset;
		doc->is_ngram = index->is_ngram;

		/* Null Field */
		if (doc->text.f_len == UNIV_SQL_NULL || doc->text.f_len == 0) {
			continue;
		}

		if (processed_doc == 0) {
			fts_tokenize_document(doc, NULL, parser);
		} else {
			fts_tokenize_document_next(doc, doc_len, NULL, parser);
		}

		processed_doc++;
		doc_len += doc->text.f_len + 1;
	}
}

/*********************************************************************//**
This function fetches the document inserted during the committing
transaction, and tokenize the inserted text data and insert into
FTS auxiliary table and its cache.
@return TRUE if successful */
static
ulint
fts_add_doc_by_id(
/*==============*/
	fts_trx_table_t*ftt,		/*!< in: FTS trx table */
	doc_id_t	doc_id,		/*!< in: doc id */
	ib_vector_t*	fts_indexes MY_ATTRIBUTE((unused)))
					/*!< in: affected fts indexes */
{
	mtr_t		mtr;
	mem_heap_t*	heap;
	btr_pcur_t	pcur;
	dict_table_t*	table;
	dtuple_t*	tuple;
	dfield_t*       dfield;
	fts_get_doc_t*	get_doc;
	doc_id_t        temp_doc_id;
	dict_index_t*   clust_index;
	dict_index_t*	fts_id_index;
	ibool		is_id_cluster;
	fts_cache_t*   	cache = ftt->table->fts->cache;

	ut_ad(cache->get_docs);

	/* If Doc ID has been supplied by the user, then the table
	might not yet be sync-ed */

	if (!(ftt->table->fts->fts_status & ADDED_TABLE_SYNCED)) {
		fts_init_index(ftt->table, FALSE);
	}

	/* Get the first FTS index's get_doc */
	get_doc = static_cast<fts_get_doc_t*>(
		ib_vector_get(cache->get_docs, 0));
	ut_ad(get_doc);

	table = get_doc->index_cache->index->table;

	heap = mem_heap_create(512);

	clust_index = dict_table_get_first_index(table);
	fts_id_index = table->fts_doc_id_index;

	/* Check whether the index on FTS_DOC_ID is cluster index */
	is_id_cluster = (clust_index == fts_id_index);

	mtr_start(&mtr);
	btr_pcur_init(&pcur);

	/* Search based on Doc ID. Here, we'll need to consider the case
	when there is no primary index on Doc ID */
	tuple = dtuple_create(heap, 1);
	dfield = dtuple_get_nth_field(tuple, 0);
	dfield->type.mtype = DATA_INT;
	dfield->type.prtype = DATA_NOT_NULL | DATA_UNSIGNED | DATA_BINARY_TYPE;

	mach_write_to_8((byte*) &temp_doc_id, doc_id);
	dfield_set_data(dfield, &temp_doc_id, sizeof(temp_doc_id));

	btr_pcur_open_with_no_init(
		fts_id_index, tuple, PAGE_CUR_LE, BTR_SEARCH_LEAF,
		&pcur, 0, &mtr);

	/* If we have a match, add the data to doc structure */
	if (btr_pcur_get_low_match(&pcur) == 1) {
		const rec_t*	rec;
		btr_pcur_t*	doc_pcur;
		const rec_t*	clust_rec;
		btr_pcur_t	clust_pcur;
		ulint*		offsets = NULL;
		ulint		num_idx = ib_vector_size(cache->get_docs);

		rec = btr_pcur_get_rec(&pcur);

		/* Doc could be deleted */
		if (page_rec_is_infimum(rec)
		    || rec_get_deleted_flag(rec, dict_table_is_comp(table))) {

			goto func_exit;
		}

		if (is_id_cluster) {
			clust_rec = rec;
			doc_pcur = &pcur;
		} else {
			dtuple_t*	clust_ref;
			ulint		n_fields;

			btr_pcur_init(&clust_pcur);
			n_fields = dict_index_get_n_unique(clust_index);

			clust_ref = dtuple_create(heap, n_fields);
			dict_index_copy_types(clust_ref, clust_index, n_fields);

			row_build_row_ref_in_tuple(
				clust_ref, rec, fts_id_index, NULL, NULL);

			btr_pcur_open_with_no_init(
				clust_index, clust_ref, PAGE_CUR_LE,
				BTR_SEARCH_LEAF, &clust_pcur, 0, &mtr);

			doc_pcur = &clust_pcur;
			clust_rec = btr_pcur_get_rec(&clust_pcur);

		}

		offsets = rec_get_offsets(clust_rec, clust_index,
					  NULL, ULINT_UNDEFINED, &heap);

		 for (ulint i = 0; i < num_idx; ++i) {
			fts_doc_t       doc;
			dict_table_t*   table;
			fts_get_doc_t*  get_doc;

			get_doc = static_cast<fts_get_doc_t*>(
				ib_vector_get(cache->get_docs, i));

			table = get_doc->index_cache->index->table;

			fts_doc_init(&doc);

			fts_fetch_doc_from_rec(
				get_doc, clust_index, doc_pcur, offsets, &doc);

			if (doc.found) {
				ibool	success MY_ATTRIBUTE((unused));

				btr_pcur_store_position(doc_pcur, &mtr);
				mtr_commit(&mtr);

				rw_lock_x_lock(&table->fts->cache->lock);

				if (table->fts->cache->stopword_info.status
				    & STOPWORD_NOT_INIT) {
					fts_load_stopword(table, NULL, NULL,
							  NULL, TRUE, TRUE);
				}

				fts_cache_add_doc(
					table->fts->cache,
					get_doc->index_cache,
					doc_id, doc.tokens);

				bool	need_sync = false;
				if ((cache->total_size > fts_max_cache_size / 10
				     || fts_need_sync)
				    && !cache->sync->in_progress) {
					need_sync = true;
				}

				rw_lock_x_unlock(&table->fts->cache->lock);

				DBUG_EXECUTE_IF(
					"fts_instrument_sync",
					fts_optimize_request_sync_table(table);
					os_event_wait(cache->sync->event);
				);

				DBUG_EXECUTE_IF(
					"fts_instrument_sync_debug",
					fts_sync(cache->sync, true, true, false);
				);

				DEBUG_SYNC_C("fts_instrument_sync_request");
				DBUG_EXECUTE_IF(
					"fts_instrument_sync_request",
					fts_optimize_request_sync_table(table);
				);

				if (need_sync) {
					fts_optimize_request_sync_table(table);
				}

				mtr_start(&mtr);

				if (i < num_idx - 1) {

					success = btr_pcur_restore_position(
						BTR_SEARCH_LEAF, doc_pcur,
						&mtr);

					ut_ad(success);
				}
			}

			fts_doc_free(&doc);
		}

		if (!is_id_cluster) {
			btr_pcur_close(doc_pcur);
		}
	}
func_exit:
	mtr_commit(&mtr);

	btr_pcur_close(&pcur);

	mem_heap_free(heap);
	return(TRUE);
}


/*********************************************************************//**
Callback function to read a single ulint column.
return always returns TRUE */
static
ibool
fts_read_ulint(
/*===========*/
	void*		row,		/*!< in: sel_node_t* */
	void*		user_arg)	/*!< in: pointer to ulint */
{
	sel_node_t*	sel_node = static_cast<sel_node_t*>(row);
	ulint*		value = static_cast<ulint*>(user_arg);
	que_node_t*	exp = sel_node->select_list;
	dfield_t*	dfield = que_node_get_val(exp);
	void*		data = dfield_get_data(dfield);

	*value = static_cast<ulint>(mach_read_from_4(
		static_cast<const byte*>(data)));

	return(TRUE);
}

/*********************************************************************//**
Get maximum Doc ID in a table if index "FTS_DOC_ID_INDEX" exists
@return max Doc ID or 0 if index "FTS_DOC_ID_INDEX" does not exist */
doc_id_t
fts_get_max_doc_id(
/*===============*/
	dict_table_t*	table)		/*!< in: user table */
{
	dict_index_t*	index;
	dict_field_t*	dfield MY_ATTRIBUTE((unused)) = NULL;
	doc_id_t	doc_id = 0;
	mtr_t		mtr;
	btr_pcur_t	pcur;

	index = table->fts_doc_id_index;

	if (!index) {
		return(0);
	}

	dfield = dict_index_get_nth_field(index, 0);

#if 0 /* This can fail when renaming a column to FTS_DOC_ID_COL_NAME. */
	ut_ad(innobase_strcasecmp(FTS_DOC_ID_COL_NAME, dfield->name) == 0);
#endif

	mtr_start(&mtr);

	/* fetch the largest indexes value */
	btr_pcur_open_at_index_side(
		false, index, BTR_SEARCH_LEAF, &pcur, true, 0, &mtr);

	if (!page_is_empty(btr_pcur_get_page(&pcur))) {
		const rec_t*    rec = NULL;
		ulint		offsets_[REC_OFFS_NORMAL_SIZE];
		ulint*		offsets = offsets_;
		mem_heap_t*	heap = NULL;
		ulint		len;
		const void*	data;

		rec_offs_init(offsets_);

		do {
			rec = btr_pcur_get_rec(&pcur);

			if (page_rec_is_user_rec(rec)) {
				break;
			}
		} while (btr_pcur_move_to_prev(&pcur, &mtr));

		if (!rec) {
			goto func_exit;
		}

		offsets = rec_get_offsets(
			rec, index, offsets, ULINT_UNDEFINED, &heap);

		data = rec_get_nth_field(rec, offsets, 0, &len);

		doc_id = static_cast<doc_id_t>(fts_read_doc_id(
			static_cast<const byte*>(data)));
	}

func_exit:
	btr_pcur_close(&pcur);
	mtr_commit(&mtr);
	return(doc_id);
}

/*********************************************************************//**
Fetch document with the given document id.
@return DB_SUCCESS if OK else error */
dberr_t
fts_doc_fetch_by_doc_id(
/*====================*/
	fts_get_doc_t*	get_doc,	/*!< in: state */
	doc_id_t	doc_id,		/*!< in: id of document to
					fetch */
	dict_index_t*	index_to_use,	/*!< in: caller supplied FTS index,
					or NULL */
	ulint		option,		/*!< in: search option, if it is
					greater than doc_id or equal */
	fts_sql_callback
			callback,	/*!< in: callback to read */
	void*		arg)		/*!< in: callback arg */
{
	pars_info_t*	info;
	dberr_t		error;
	const char*	select_str;
	doc_id_t	write_doc_id;
	dict_index_t*	index;
	trx_t*		trx = trx_allocate_for_background();
	que_t*          graph;

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
	fts_write_doc_id((byte*) &write_doc_id, doc_id);
	fts_bind_doc_id(info, "doc_id", &write_doc_id);
	pars_info_bind_function(info, "my_func", callback, arg);

	select_str = fts_get_select_columns_str(index, info, info->heap);
	pars_info_bind_id(info, TRUE, "table_name", index->table_name);

	if (!get_doc || !get_doc->get_document_graph) {
		if (option == FTS_FETCH_DOC_BY_ID_EQUAL) {
			graph = fts_parse_sql(
				NULL,
				info,
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
			column, and this scan is to retreive the largest
			DOC ID being used in the table to determine the
			appropriate next DOC ID.
			In the case of there exists fulltext index(es), this
			operation will re-tokenize any docs that have not
			been sync-ed to the disk, and re-prime the FTS
			cached */
			graph = fts_parse_sql(
				NULL,
				info,
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
					FTS_DOC_ID_COL_NAME,
					select_str, FTS_DOC_ID_COL_NAME));
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

	return(error);
}

/*********************************************************************//**
Write out a single word's data as new entry/entries in the INDEX table.
@return DB_SUCCESS if all OK. */
dberr_t
fts_write_node(
/*===========*/
	trx_t*		trx,			/*!< in: transaction */
	que_t**		graph,			/*!< in: query graph */
	fts_table_t*	fts_table,		/*!< in: aux table */
	fts_string_t*	word,			/*!< in: word in UTF-8 */
	fts_node_t*	node)			/*!< in: node columns */
{
	pars_info_t*	info;
	dberr_t		error;
	ib_uint32_t	doc_count;
	ib_time_t	start_time;
	doc_id_t	last_doc_id;
	doc_id_t	first_doc_id;
	char		table_name[MAX_FULL_NAME_LEN];

	ut_a(node->ilist != NULL);

	if (*graph) {
		info = (*graph)->info;
	} else {
		info = pars_info_create();

		fts_get_table_name(fts_table, table_name);
		pars_info_bind_id(info, true, "index_table_name", table_name);
	}

	pars_info_bind_varchar_literal(info, "token", word->f_str, word->f_len);

	/* Convert to "storage" byte order. */
	fts_write_doc_id((byte*) &first_doc_id, node->first_doc_id);
	fts_bind_doc_id(info, "first_doc_id", &first_doc_id);

	/* Convert to "storage" byte order. */
	fts_write_doc_id((byte*) &last_doc_id, node->last_doc_id);
	fts_bind_doc_id(info, "last_doc_id", &last_doc_id);

	ut_a(node->last_doc_id >= node->first_doc_id);

	/* Convert to "storage" byte order. */
	mach_write_to_4((byte*) &doc_count, node->doc_count);
	pars_info_bind_int4_literal(
		info, "doc_count", (const ib_uint32_t*) &doc_count);

	/* Set copy_name to FALSE since it's a static. */
	pars_info_bind_literal(
		info, "ilist", node->ilist, node->ilist_size,
		DATA_BLOB, DATA_BINARY_TYPE);

	if (!*graph) {

		*graph = fts_parse_sql(
			fts_table,
			info,
			"BEGIN\n"
			"INSERT INTO $index_table_name VALUES"
			" (:token, :first_doc_id,"
			"  :last_doc_id, :doc_count, :ilist);");
	}

	start_time = ut_time();
	error = fts_eval_sql(trx, *graph);
	elapsed_time += ut_time() - start_time;
	++n_nodes;

	return(error);
}

/*********************************************************************//**
Add rows to the DELETED_CACHE table.
@return DB_SUCCESS if all went well else error code*/
static MY_ATTRIBUTE((nonnull, warn_unused_result))
dberr_t
fts_sync_add_deleted_cache(
/*=======================*/
	fts_sync_t*	sync,			/*!< in: sync state */
	ib_vector_t*	doc_ids)		/*!< in: doc ids to add */
{
	ulint		i;
	pars_info_t*	info;
	que_t*		graph;
	fts_table_t	fts_table;
	char		table_name[MAX_FULL_NAME_LEN];
	doc_id_t	dummy = 0;
	dberr_t		error = DB_SUCCESS;
	ulint		n_elems = ib_vector_size(doc_ids);

	ut_a(ib_vector_size(doc_ids) > 0);

	ib_vector_sort(doc_ids, fts_update_doc_id_cmp);

	info = pars_info_create();

	fts_bind_doc_id(info, "doc_id", &dummy);

	FTS_INIT_FTS_TABLE(
		&fts_table, "DELETED_CACHE", FTS_COMMON_TABLE, sync->table);

	fts_get_table_name(&fts_table, table_name);
	pars_info_bind_id(info, true, "table_name", table_name);

	graph = fts_parse_sql(
		&fts_table,
		info,
		"BEGIN INSERT INTO $table_name VALUES (:doc_id);");

	for (i = 0; i < n_elems && error == DB_SUCCESS; ++i) {
		fts_update_t*	update;
		doc_id_t	write_doc_id;

		update = static_cast<fts_update_t*>(ib_vector_get(doc_ids, i));

		/* Convert to "storage" byte order. */
		fts_write_doc_id((byte*) &write_doc_id, update->doc_id);
		fts_bind_doc_id(info, "doc_id", &write_doc_id);

		error = fts_eval_sql(sync->trx, graph);
	}

	fts_que_graph_free(graph);

	return(error);
}

/** Write the words and ilist to disk.
@param[in,out]	trx		transaction
@param[in]	index_cache	index cache
@param[in]	unlock_cache	whether unlock cache when write node
@return DB_SUCCESS if all went well else error code */
static MY_ATTRIBUTE((nonnull, warn_unused_result))
dberr_t
fts_sync_write_words(
	trx_t*			trx,
	fts_index_cache_t*	index_cache,
	bool			unlock_cache)
{
	fts_table_t	fts_table;
	ulint		n_nodes = 0;
	ulint		n_words = 0;
	const ib_rbt_node_t* rbt_node;
	dberr_t		error = DB_SUCCESS;
	ibool		print_error = FALSE;
	dict_table_t*	table = index_cache->index->table;
#ifdef FTS_DOC_STATS_DEBUG
	ulint		n_new_words = 0;
#endif /* FTS_DOC_STATS_DEBUG */

	FTS_INIT_INDEX_TABLE(
		&fts_table, NULL, FTS_INDEX_TABLE, index_cache->index);

	n_words = rbt_size(index_cache->words);

	/* We iterate over the entire tree, even if there is an error,
	since we want to free the memory used during caching. */
	for (rbt_node = rbt_first(index_cache->words);
	     rbt_node;
	     rbt_node = rbt_next(index_cache->words, rbt_node)) {

		ulint			i;
		ulint			selected;
		fts_tokenizer_word_t*	word;

		word = rbt_value(fts_tokenizer_word_t, rbt_node);

		selected = fts_select_index(
			index_cache->charset, word->text.f_str,
			word->text.f_len);

		fts_table.suffix = fts_get_suffix(selected);

#ifdef FTS_DOC_STATS_DEBUG
		/* Check if the word exists in the FTS index and if not
		then we need to increment the total word count stats. */
		if (error == DB_SUCCESS && fts_enable_diag_print) {
			ibool	found = FALSE;

			error = fts_is_word_in_index(
				trx,
				&index_cache->sel_graph[selected],
				&fts_table,
				&word->text, &found);

			if (error == DB_SUCCESS && !found) {

				++n_new_words;
			}
		}
#endif /* FTS_DOC_STATS_DEBUG */

		/* We iterate over all the nodes even if there was an error */
		for (i = 0; i < ib_vector_size(word->nodes); ++i) {

			fts_node_t* fts_node = static_cast<fts_node_t*>(
				ib_vector_get(word->nodes, i));

			if (fts_node->synced) {
				continue;
			} else {
				fts_node->synced = true;
			}

			/*FIXME: we need to handle the error properly. */
			if (error == DB_SUCCESS) {
				if (unlock_cache) {
					rw_lock_x_unlock(
						&table->fts->cache->lock);
				}

				error = fts_write_node(
					trx,
					&index_cache->ins_graph[selected],
					&fts_table, &word->text, fts_node);

				DEBUG_SYNC_C("fts_write_node");
				DBUG_EXECUTE_IF("fts_write_node_crash",
					DBUG_SUICIDE(););

				DBUG_EXECUTE_IF("fts_instrument_sync_sleep",
					os_thread_sleep(1000000);
				);

				if (unlock_cache) {
					rw_lock_x_lock(
						&table->fts->cache->lock);
				}
			}
		}

		n_nodes += ib_vector_size(word->nodes);

		if (error != DB_SUCCESS && !print_error) {
			ib::error() << "(" << ut_strerr(error) << ") writing"
				" word node to FTS auxiliary index table.";
			print_error = TRUE;
		}
	}

#ifdef FTS_DOC_STATS_DEBUG
	if (error == DB_SUCCESS && n_new_words > 0 && fts_enable_diag_print) {
		fts_table_t	fts_table;

		FTS_INIT_FTS_TABLE(&fts_table, NULL, FTS_COMMON_TABLE, table);

		/* Increment the total number of words in the FTS index */
		error = fts_config_increment_index_value(
			trx, index_cache->index, FTS_TOTAL_WORD_COUNT,
			n_new_words);
	}
#endif /* FTS_DOC_STATS_DEBUG */

	if (fts_enable_diag_print) {
		printf("Avg number of nodes: %lf\n",
		       (double) n_nodes / (double) (n_words > 1 ? n_words : 1));
	}

	return(error);
}

#ifdef FTS_DOC_STATS_DEBUG
/*********************************************************************//**
Write a single documents statistics to disk.
@return DB_SUCCESS if all went well else error code */
static MY_ATTRIBUTE((nonnull, warn_unused_result))
dberr_t
fts_sync_write_doc_stat(
/*====================*/
	trx_t*			trx,		/*!< in: transaction */
	dict_index_t*		index,		/*!< in: index */
	que_t**			graph,		/* out: query graph */
	const fts_doc_stats_t*	doc_stat)	/*!< in: doc stats to write */
{
	pars_info_t*	info;
	doc_id_t	doc_id;
	dberr_t		error = DB_SUCCESS;
	ib_uint32_t	word_count;
	char		table_name[MAX_FULL_NAME_LEN];

	if (*graph) {
		info = (*graph)->info;
	} else {
		info = pars_info_create();
	}

	/* Convert to "storage" byte order. */
	mach_write_to_4((byte*) &word_count, doc_stat->word_count);
	pars_info_bind_int4_literal(
		info, "count", (const ib_uint32_t*) &word_count);

	/* Convert to "storage" byte order. */
	fts_write_doc_id((byte*) &doc_id, doc_stat->doc_id);
	fts_bind_doc_id(info, "doc_id", &doc_id);

	if (!*graph) {
		fts_table_t	fts_table;

		FTS_INIT_INDEX_TABLE(
			&fts_table, "DOC_ID", FTS_INDEX_TABLE, index);

		fts_get_table_name(&fts_table, table_name);

		pars_info_bind_id(info, true, "doc_id_table", table_name);

		*graph = fts_parse_sql(
			&fts_table,
			info,
			"BEGIN"
			" INSERT INTO $doc_id_table VALUES (:doc_id, :count);");
	}

	for (;;) {
		error = fts_eval_sql(trx, *graph);

		if (error == DB_SUCCESS) {

			break;				/* Exit the loop. */
		} else {

			if (error == DB_LOCK_WAIT_TIMEOUT) {
				ib::warn() << "Lock wait timeout writing to"
					" FTS doc_id. Retrying!";

				trx->error_state = DB_SUCCESS;
			} else {
				ib::error() << "(" << ut_strerr(error)
					<< ") while writing to FTS doc_id.";

				break;			/* Exit the loop. */
			}
		}
	}

	return(error);
}

/*********************************************************************//**
Write document statistics to disk.
@return DB_SUCCESS if all OK */
static
ulint
fts_sync_write_doc_stats(
/*=====================*/
	trx_t*			trx,		/*!< in: transaction */
	const fts_index_cache_t*index_cache)	/*!< in: index cache */
{
	dberr_t		error = DB_SUCCESS;
	que_t*		graph = NULL;
	fts_doc_stats_t*  doc_stat;

	if (ib_vector_is_empty(index_cache->doc_stats)) {
		return(DB_SUCCESS);
	}

	doc_stat = static_cast<ts_doc_stats_t*>(
		ib_vector_pop(index_cache->doc_stats));

	while (doc_stat) {
		error = fts_sync_write_doc_stat(
			trx, index_cache->index, &graph, doc_stat);

		if (error != DB_SUCCESS) {
			break;
		}

		if (ib_vector_is_empty(index_cache->doc_stats)) {
			break;
		}

		doc_stat = static_cast<ts_doc_stats_t*>(
			ib_vector_pop(index_cache->doc_stats));
	}

	if (graph != NULL) {
		fts_que_graph_free_check_lock(NULL, index_cache, graph);
	}

	return(error);
}

/*********************************************************************//**
Callback to check the existince of a word.
@return always return NULL */
static
ibool
fts_lookup_word(
/*============*/
	void*	row,				/*!< in:  sel_node_t* */
	void*	user_arg)			/*!< in:  fts_doc_t* */
{

	que_node_t*	exp;
	sel_node_t*	node = static_cast<sel_node_t*>(row);
	ibool*		found = static_cast<ibool*>(user_arg);

	exp = node->select_list;

	while (exp) {
		dfield_t*	dfield = que_node_get_val(exp);
		ulint		len = dfield_get_len(dfield);

		if (len != UNIV_SQL_NULL && len != 0) {
			*found = TRUE;
		}

		exp = que_node_get_next(exp);
	}

	return(FALSE);
}

/*********************************************************************//**
Check whether a particular word (term) exists in the FTS index.
@return DB_SUCCESS if all went well else error code */
static
dberr_t
fts_is_word_in_index(
/*=================*/
	trx_t*		trx,			/*!< in: FTS query state */
	que_t**		graph,			/* out: Query graph */
	fts_table_t*	fts_table,		/*!< in: table instance */
	const fts_string_t*
			word,			/*!< in: the word to check */
	ibool*		found)			/* out: TRUE if exists */
{
	pars_info_t*	info;
	dberr_t		error;
	char		table_name[MAX_FULL_NAME_LEN];

	trx->op_info = "looking up word in FTS index";

	if (*graph) {
		info = (*graph)->info;
	} else {
		info = pars_info_create();
	}

	fts_get_table_name(fts_table, table_name);
	pars_info_bind_id(info, true, "table_name", table_name);
	pars_info_bind_function(info, "my_func", fts_lookup_word, found);
	pars_info_bind_varchar_literal(info, "word", word->f_str, word->f_len);

	if (*graph == NULL) {
		*graph = fts_parse_sql(
			fts_table,
			info,
			"DECLARE FUNCTION my_func;\n"
			"DECLARE CURSOR c IS"
			" SELECT doc_count\n"
			" FROM $table_name\n"
			" WHERE word = :word"
			" ORDER BY first_doc_id;\n"
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
	}

	for (;;) {
		error = fts_eval_sql(trx, *graph);

		if (error == DB_SUCCESS) {

			break;				/* Exit the loop. */
		} else {

			if (error == DB_LOCK_WAIT_TIMEOUT) {
				ib::warn() << "Lock wait timeout reading"
					" FTS index. Retrying!";

				trx->error_state = DB_SUCCESS;
			} else {
				ib::error() << "(" << ut_strerr(error)
					<< ") while reading FTS index.";

				break;			/* Exit the loop. */
			}
		}
	}

	return(error);
}
#endif /* FTS_DOC_STATS_DEBUG */

/*********************************************************************//**
Begin Sync, create transaction, acquire locks, etc. */
static
void
fts_sync_begin(
/*===========*/
	fts_sync_t*	sync)			/*!< in: sync state */
{
	fts_cache_t*	cache = sync->table->fts->cache;

	n_nodes = 0;
	elapsed_time = 0;

	sync->start_time = ut_time();

	sync->trx = trx_allocate_for_background();

	if (fts_enable_diag_print) {
		ib::info() << "FTS SYNC for table " << sync->table->name
			<< ", deleted count: "
			<< ib_vector_size(cache->deleted_doc_ids)
			<< " size: " << cache->total_size << " bytes";
	}
}

/*********************************************************************//**
Run SYNC on the table, i.e., write out data from the index specific
cache to the FTS aux INDEX table and FTS aux doc id stats table.
@return DB_SUCCESS if all OK */
static MY_ATTRIBUTE((nonnull, warn_unused_result))
dberr_t
fts_sync_index(
/*===========*/
	fts_sync_t*		sync,		/*!< in: sync state */
	fts_index_cache_t*	index_cache)	/*!< in: index cache */
{
	trx_t*		trx = sync->trx;
	dberr_t		error = DB_SUCCESS;

	trx->op_info = "doing SYNC index";

	if (fts_enable_diag_print) {
		ib::info() << "SYNC words: " << rbt_size(index_cache->words);
	}

	ut_ad(rbt_validate(index_cache->words));

	error = fts_sync_write_words(trx, index_cache, sync->unlock_cache);

#ifdef FTS_DOC_STATS_DEBUG
	/* FTS_RESOLVE: the word counter info in auxiliary table "DOC_ID"
	is not used currently for ranking. We disable fts_sync_write_doc_stats()
	for now */
	/* Write the per doc statistics that will be used for ranking. */
	if (error == DB_SUCCESS) {

		error = fts_sync_write_doc_stats(trx, index_cache);
	}
#endif /* FTS_DOC_STATS_DEBUG */

	return(error);
}

/** Check if index cache has been synced completely
@param[in,out]	index_cache	index cache
@return true if index is synced, otherwise false. */
static
bool
fts_sync_index_check(
	fts_index_cache_t*	index_cache)
{
	const ib_rbt_node_t*	rbt_node;

	for (rbt_node = rbt_first(index_cache->words);
	     rbt_node != NULL;
	     rbt_node = rbt_next(index_cache->words, rbt_node)) {

		fts_tokenizer_word_t*	word;
		word = rbt_value(fts_tokenizer_word_t, rbt_node);

		fts_node_t*	fts_node;
		fts_node = static_cast<fts_node_t*>(ib_vector_last(word->nodes));

		if (!fts_node->synced) {
			return(false);
		}
	}

	return(true);
}

/** Reset synced flag in index cache when rollback
@param[in,out]	index_cache	index cache */
static
void
fts_sync_index_reset(
	fts_index_cache_t*	index_cache)
{
	const ib_rbt_node_t*	rbt_node;

	for (rbt_node = rbt_first(index_cache->words);
	     rbt_node != NULL;
	     rbt_node = rbt_next(index_cache->words, rbt_node)) {

		fts_tokenizer_word_t*	word;
		word = rbt_value(fts_tokenizer_word_t, rbt_node);

		fts_node_t*	fts_node;
		fts_node = static_cast<fts_node_t*>(ib_vector_last(word->nodes));

		fts_node->synced = false;
	}
}

/** Commit the SYNC, change state of processed doc ids etc.
@param[in,out]	sync	sync state
@return DB_SUCCESS if all OK */
static  MY_ATTRIBUTE((nonnull, warn_unused_result))
dberr_t
fts_sync_commit(
	fts_sync_t*	sync)
{
	dberr_t		error;
	trx_t*		trx = sync->trx;
	fts_cache_t*	cache = sync->table->fts->cache;
	doc_id_t	last_doc_id;

	trx->op_info = "doing SYNC commit";

	/* After each Sync, update the CONFIG table about the max doc id
	we just sync-ed to index table */
	error = fts_cmp_set_sync_doc_id(sync->table, sync->max_doc_id, FALSE,
					&last_doc_id);

	/* Get the list of deleted documents that are either in the
	cache or were headed there but were deleted before the add
	thread got to them. */

	if (error == DB_SUCCESS && ib_vector_size(cache->deleted_doc_ids) > 0) {

		error = fts_sync_add_deleted_cache(
			sync, cache->deleted_doc_ids);
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

		ib::error() << "(" << ut_strerr(error) << ") during SYNC.";
	}

	if (fts_enable_diag_print && elapsed_time) {
		ib::info() << "SYNC for table " << sync->table->name
			<< ": SYNC time: "
			<< (ut_time() - sync->start_time)
			<< " secs: elapsed "
			<< (double) n_nodes / elapsed_time
			<< " ins/sec";
	}

	/* Avoid assertion in trx_free(). */
	trx->dict_operation_lock_mode = 0;
	trx_free_for_background(trx);

	return(error);
}

/*********************************************************************//**
Rollback a sync operation */
static
void
fts_sync_rollback(
/*==============*/
	fts_sync_t*	sync)			/*!< in: sync state */
{
	trx_t*		trx = sync->trx;
	fts_cache_t*	cache = sync->table->fts->cache;

	for (ulint i = 0; i < ib_vector_size(cache->indexes); ++i) {
		ulint			j;
		fts_index_cache_t*	index_cache;

		index_cache = static_cast<fts_index_cache_t*>(
			ib_vector_get(cache->indexes, i));

		/* Reset synced flag so nodes will not be skipped
		in the next sync, see fts_sync_write_words(). */
		fts_sync_index_reset(index_cache);

		for (j = 0; fts_index_selector[j].value; ++j) {

			if (index_cache->ins_graph[j] != NULL) {

				fts_que_graph_free_check_lock(
					NULL, index_cache,
					index_cache->ins_graph[j]);

				index_cache->ins_graph[j] = NULL;
			}

			if (index_cache->sel_graph[j] != NULL) {

				fts_que_graph_free_check_lock(
					NULL, index_cache,
					index_cache->sel_graph[j]);

				index_cache->sel_graph[j] = NULL;
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
@param[in,out]	sync		sync state
@param[in]	unlock_cache	whether unlock cache lock when write node
@param[in]	wait		whether wait when a sync is in progress
@param[in]      has_dict        whether has dict operation lock
@return DB_SUCCESS if all OK */
static
dberr_t
fts_sync(
	fts_sync_t*	sync,
	bool		unlock_cache,
	bool		wait,
	bool		has_dict)
{
	ulint		i;
	dberr_t		error = DB_SUCCESS;
	fts_cache_t*	cache = sync->table->fts->cache;

	rw_lock_x_lock(&cache->lock);

	/* Check if cache is being synced.
	Note: we release cache lock in fts_sync_write_words() to
	avoid long wait for the lock by other threads. */
	while (sync->in_progress) {
		rw_lock_x_unlock(&cache->lock);

		if (wait) {
			os_event_wait(sync->event);
		} else {
			return(DB_SUCCESS);
		}

		rw_lock_x_lock(&cache->lock);
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
		insert/update keeps comming. */
		ut_ad(sync->unlock_cache);
		sync->unlock_cache = false;
	}

	for (i = 0; i < ib_vector_size(cache->indexes); ++i) {
		fts_index_cache_t*	index_cache;

		index_cache = static_cast<fts_index_cache_t*>(
			ib_vector_get(cache->indexes, i));

		error = fts_sync_index(sync, index_cache);

		if (error != DB_SUCCESS && !sync->interrupted) {

			goto end_sync;
		}
	}

	DBUG_EXECUTE_IF("fts_instrument_sync_interrupted",
			sync->interrupted = true;
			error = DB_INTERRUPTED;
			goto end_sync;
	);

	/* Make sure all the caches are synced. */
	for (i = 0; i < ib_vector_size(cache->indexes); ++i) {
		fts_index_cache_t*	index_cache;

		index_cache = static_cast<fts_index_cache_t*>(
			ib_vector_get(cache->indexes, i));

		if (index_cache->index->to_be_dropped
		    || fts_sync_index_check(index_cache)) {
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

	rw_lock_x_lock(&cache->lock);
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

	return(error);
}

/** Run SYNC on the table, i.e., write out data from the cache to the
FTS auxiliary INDEX table and clear the cache at the end.
@param[in,out]	table		fts table
@param[in]	unlock_cache	whether unlock cache when write node
@param[in]	wait		whether wait for existing sync to finish
@param[in]	has_dict	whether has dict operation lock
@return DB_SUCCESS on success, error code on failure. */
dberr_t
fts_sync_table(
	dict_table_t*	table,
	bool		unlock_cache,
	bool		wait,
	bool		has_dict)
{
	dberr_t	err = DB_SUCCESS;

	ut_ad(table->fts);

	if (!dict_table_is_discarded(table) && table->fts->cache
	    && !dict_table_is_corrupted(table)) {
		err = fts_sync(table->fts->cache->sync,
			       unlock_cache, wait, has_dict);
	}

	return(err);
}

/** Check fts token
1. for ngram token, check whether the token contains any words in stopwords
2. for non-ngram token, check if it's stopword or less than fts_min_token_size
or greater than fts_max_token_size.
@param[in]	token		token string
@param[in]	stopwords	stopwords rb tree
@param[in]	is_ngram	is ngram parser
@param[in]	cs		token charset
@retval	true	if it is not stopword and length in range
@retval	false	if it is stopword or lenght not in range */
bool
fts_check_token(
	const fts_string_t*		token,
	const ib_rbt_t*			stopwords,
	bool				is_ngram,
	const CHARSET_INFO*		cs)
{
	ut_ad(cs != NULL || stopwords == NULL);

	if (!is_ngram) {
		ib_rbt_bound_t  parent;

		if (token->f_n_char < fts_min_token_size
		    || token->f_n_char > fts_max_token_size
		    || (stopwords != NULL
			&& rbt_search(stopwords, &parent, token) == 0)) {
			return(false);
		} else {
			return(true);
		}
	}

	/* Check token for ngram. */
	DBUG_EXECUTE_IF(
		"fts_instrument_ignore_ngram_check",
		return(true);
	);

	/* We ignore fts_min_token_size when ngram */
	ut_ad(token->f_n_char > 0
	      && token->f_n_char <= fts_max_token_size);

	if (stopwords == NULL) {
		return(true);
	}

	/*Ngram checks whether the token contains any words in stopwords.
	We can't simply use CONTAIN to search in stopwords, because it's
	built on COMPARE. So we need to tokenize the token into words
	from unigram to f_n_char, and check them separately. */
	for (ulint ngram_token_size = 1; ngram_token_size <= token->f_n_char;
	     ngram_token_size ++) {
		const char*	start;
		const char*	next;
		const char*	end;
		ulint		char_len;
		ulint		n_chars;

		start = reinterpret_cast<char*>(token->f_str);
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
				fts_string_t	ngram_token;
				ngram_token.f_str =
					reinterpret_cast<byte*>(
					const_cast<char*>(start));
				ngram_token.f_len = next - start;
				ngram_token.f_n_char = ngram_token_size;

				ib_rbt_bound_t  parent;
				if (rbt_search(stopwords, &parent,
					       &ngram_token) == 0) {
					return(false);
				}

				/* Move a char forward */
				start += my_mbcharlen_ptr(cs, start, end);
				n_chars = ngram_token_size - 1;
			}
		}
	}

	return(true);
}

/** Add the token and its start position to the token's list of positions.
@param[in,out]	result_doc	result doc rb tree
@param[in]	str		token string
@param[in]	position	token position */
static
void
fts_add_token(
	fts_doc_t*	result_doc,
	fts_string_t	str,
	ulint		position)
{
	/* Ignore string whose character number is less than
	"fts_min_token_size" or more than "fts_max_token_size" */

	if (fts_check_token(&str, NULL, result_doc->is_ngram,
			    result_doc->charset)) {

		mem_heap_t*	heap;
		fts_string_t	t_str;
		fts_token_t*	token;
		ib_rbt_bound_t	parent;
		ulint		newlen;

		heap = static_cast<mem_heap_t*>(result_doc->self_heap->arg);

		t_str.f_n_char = str.f_n_char;

		t_str.f_len = str.f_len * result_doc->charset->casedn_multiply + 1;

		t_str.f_str = static_cast<byte*>(
			mem_heap_alloc(heap, t_str.f_len));

		newlen = innobase_fts_casedn_str(
			result_doc->charset,
			reinterpret_cast<char*>(str.f_str), str.f_len,
			reinterpret_cast<char*>(t_str.f_str), t_str.f_len);

		t_str.f_len = newlen;
		t_str.f_str[newlen] = 0;

		/* Add the word to the document statistics. If the word
		hasn't been seen before we create a new entry for it. */
		if (rbt_search(result_doc->tokens, &parent, &t_str) != 0) {
			fts_token_t	new_token;

			new_token.text.f_len = newlen;
			new_token.text.f_str = t_str.f_str;
			new_token.text.f_n_char = t_str.f_n_char;

			new_token.positions = ib_vector_create(
				result_doc->self_heap, sizeof(ulint), 32);

			parent.last = rbt_add_node(
				result_doc->tokens, &parent, &new_token);

			ut_ad(rbt_validate(result_doc->tokens));
		}

		token = rbt_value(fts_token_t, parent.last);
		ib_vector_push(token->positions, &position);
	}
}

/********************************************************************
Process next token from document starting at the given position, i.e., add
the token's start position to the token's list of positions.
@return number of characters handled in this call */
static
ulint
fts_process_token(
/*==============*/
	fts_doc_t*	doc,		/* in/out: document to
					tokenize */
	fts_doc_t*	result,		/* out: if provided, save
					result here */
	ulint		start_pos,	/*!< in: start position in text */
	ulint		add_pos)	/*!< in: add this position to all
					tokens from this tokenization */
{
	ulint		ret;
	fts_string_t	str;
	ulint		position;
	fts_doc_t*	result_doc;
	byte		buf[FTS_MAX_WORD_LEN + 1];

	str.f_str = buf;

	/* Determine where to save the result. */
	result_doc = (result != NULL) ? result : doc;

	/* The length of a string in characters is set here only. */

	ret = innobase_mysql_fts_get_token(
		doc->charset, doc->text.f_str + start_pos,
		doc->text.f_str + doc->text.f_len, &str);

	position = start_pos + ret - str.f_len + add_pos;

	fts_add_token(result_doc, str, position);

	return(ret);
}

/*************************************************************//**
Get token char size by charset
@return token size */
ulint
fts_get_token_size(
/*===============*/
	const CHARSET_INFO*	cs,	/*!< in: Character set */
	const char*		token,	/*!< in: token */
	ulint			len)	/*!< in: token length */
{
	char*	start;
	char*	end;
	ulint	size = 0;

	/* const_cast is for reinterpret_cast below, or it will fail. */
	start = const_cast<char*>(token);
	end = start + len;
	while (start < end) {
		int	ctype;
		int	mbl;

		mbl = cs->cset->ctype(
			cs, &ctype,
			reinterpret_cast<uchar*>(start),
			reinterpret_cast<uchar*>(end));

		size++;

		start += mbl > 0 ? mbl : (mbl < 0 ? -mbl : 1);
	}

	return(size);
}

/*************************************************************//**
FTS plugin parser 'myql_parser' callback function for document tokenize.
Refer to 'st_mysql_ftparser_param' for more detail.
@return always returns 0 */
int
fts_tokenize_document_internal(
/*===========================*/
	MYSQL_FTPARSER_PARAM*	param,	/*!< in: parser parameter */
	char*			doc,	/*!< in/out: document */
	int			len)	/*!< in: document length */
{
	fts_string_t	str;
	byte		buf[FTS_MAX_WORD_LEN + 1];
	MYSQL_FTPARSER_BOOLEAN_INFO bool_info =
		{ FT_TOKEN_WORD, 0, 0, 0, 0, 0, ' ', 0 };

	ut_ad(len >= 0);

	str.f_str = buf;

	for (ulint i = 0, inc = 0; i < static_cast<ulint>(len); i += inc) {
		inc = innobase_mysql_fts_get_token(
			const_cast<CHARSET_INFO*>(param->cs),
			reinterpret_cast<byte*>(doc) + i,
			reinterpret_cast<byte*>(doc) + len,
			&str);

		if (str.f_len > 0) {
			bool_info.position =
				static_cast<int>(i + inc - str.f_len);
			ut_ad(bool_info.position >= 0);

			/* Stop when add word fails */
			if (param->mysql_add_word(
				param,
				reinterpret_cast<char*>(str.f_str),
				static_cast<int>(str.f_len),
				&bool_info)) {
				break;
			}
		}
	}

	return(0);
}

/******************************************************************//**
FTS plugin parser 'myql_add_word' callback function for document tokenize.
Refer to 'st_mysql_ftparser_param' for more detail.
@return always returns 0 */
static
int
fts_tokenize_add_word_for_parser(
/*=============================*/
	MYSQL_FTPARSER_PARAM*	param,		/* in: parser paramter */
	char*			word,		/* in: token word */
	int			word_len,	/* in: word len */
	MYSQL_FTPARSER_BOOLEAN_INFO* boolean_info) /* in: word boolean info */
{
	fts_string_t	str;
	fts_tokenize_param_t*	fts_param;
	fts_doc_t*	result_doc;
	ulint		position;

	fts_param = static_cast<fts_tokenize_param_t*>(param->mysql_ftparam);
	result_doc = fts_param->result_doc;
	ut_ad(result_doc != NULL);

	str.f_str = reinterpret_cast<byte*>(word);
	str.f_len = word_len;
	str.f_n_char = fts_get_token_size(
		const_cast<CHARSET_INFO*>(param->cs), word, word_len);

	ut_ad(boolean_info->position >= 0);
	position = boolean_info->position + fts_param->add_pos;

	fts_add_token(result_doc, str, position);

	return(0);
}

/******************************************************************//**
Parse a document using an external / user supplied parser */
static
void
fts_tokenize_by_parser(
/*===================*/
	fts_doc_t*		doc,	/* in/out: document to tokenize */
	st_mysql_ftparser*	parser, /* in: plugin fts parser */
	fts_tokenize_param_t*	fts_param) /* in: fts tokenize param */
{
	MYSQL_FTPARSER_PARAM	param;

	ut_a(parser);

	/* Set paramters for param */
	param.mysql_parse = fts_tokenize_document_internal;
	param.mysql_add_word = fts_tokenize_add_word_for_parser;
	param.mysql_ftparam = fts_param;
	param.cs = doc->charset;
	param.doc = reinterpret_cast<char*>(doc->text.f_str);
	param.length = static_cast<int>(doc->text.f_len);
	param.mode= MYSQL_FTPARSER_SIMPLE_MODE;

	PARSER_INIT(parser, &param);
	parser->parse(&param);
	PARSER_DEINIT(parser, &param);
}

/******************************************************************//**
Tokenize a document. */
void
fts_tokenize_document(
/*==================*/
	fts_doc_t*	doc,		/* in/out: document to
					tokenize */
	fts_doc_t*	result,		/* out: if provided, save
					the result token here */
	st_mysql_ftparser*	parser) /* in: plugin fts parser */
{
	ut_a(!doc->tokens);
	ut_a(doc->charset);

	doc->tokens = rbt_create_arg_cmp(
		sizeof(fts_token_t), innobase_fts_text_cmp, doc->charset);

	if (parser != NULL) {
		fts_tokenize_param_t	fts_param;

		fts_param.result_doc = (result != NULL) ? result : doc;
		fts_param.add_pos = 0;

		fts_tokenize_by_parser(doc, parser, &fts_param);
	} else {
		ulint		inc;

		for (ulint i = 0; i < doc->text.f_len; i += inc) {
			inc = fts_process_token(doc, result, i, 0);
			ut_a(inc > 0);
		}
	}
}

/******************************************************************//**
Continue to tokenize a document. */
void
fts_tokenize_document_next(
/*=======================*/
	fts_doc_t*	doc,		/*!< in/out: document to
					tokenize */
	ulint		add_pos,	/*!< in: add this position to all
					tokens from this tokenization */
	fts_doc_t*	result,		/*!< out: if provided, save
					the result token here */
	st_mysql_ftparser*	parser) /* in: plugin fts parser */
{
	ut_a(doc->tokens);

	if (parser) {
		fts_tokenize_param_t	fts_param;

		fts_param.result_doc = (result != NULL) ? result : doc;
		fts_param.add_pos = add_pos;

		fts_tokenize_by_parser(doc, parser, &fts_param);
	} else {
		ulint		inc;

		for (ulint i = 0; i < doc->text.f_len; i += inc) {
			inc = fts_process_token(doc, result, i, add_pos);
			ut_a(inc > 0);
		}
	}
}

/********************************************************************
Create the vector of fts_get_doc_t instances. */
ib_vector_t*
fts_get_docs_create(
/*================*/
						/* out: vector of
						fts_get_doc_t instances */
	fts_cache_t*	cache)			/*!< in: fts cache */
{
	ib_vector_t*	get_docs;

	ut_ad(rw_lock_own(&cache->init_lock, RW_LOCK_X));

	/* We need one instance of fts_get_doc_t per index. */
	get_docs = ib_vector_create(cache->self_heap, sizeof(fts_get_doc_t), 4);

	/* Create the get_doc instance, we need one of these
	per FTS index. */
	for (ulint i = 0; i < ib_vector_size(cache->indexes); ++i) {

		dict_index_t**	index;
		fts_get_doc_t*	get_doc;

		index = static_cast<dict_index_t**>(
			ib_vector_get(cache->indexes, i));

		get_doc = static_cast<fts_get_doc_t*>(
			ib_vector_push(get_docs, NULL));

		memset(get_doc, 0x0, sizeof(*get_doc));

		get_doc->index_cache = fts_get_index_cache(cache, *index);
		get_doc->cache = cache;

		/* Must find the index cache. */
		ut_a(get_doc->index_cache != NULL);
	}

	return(get_docs);
}

/********************************************************************
Release any resources held by the fts_get_doc_t instances. */
static
void
fts_get_docs_clear(
/*===============*/
	ib_vector_t*	get_docs)		/*!< in: Doc retrieval vector */
{
	ulint		i;

	/* Release the get doc graphs if any. */
	for (i = 0; i < ib_vector_size(get_docs); ++i) {

		fts_get_doc_t*	get_doc = static_cast<fts_get_doc_t*>(
			ib_vector_get(get_docs, i));

		if (get_doc->get_document_graph != NULL) {

			ut_a(get_doc->index_cache);

			fts_que_graph_free(get_doc->get_document_graph);
			get_doc->get_document_graph = NULL;
		}
	}
}

/*********************************************************************//**
Get the initial Doc ID by consulting the CONFIG table
@return initial Doc ID */
doc_id_t
fts_init_doc_id(
/*============*/
	const dict_table_t*	table)		/*!< in: table */
{
	doc_id_t	max_doc_id = 0;

	rw_lock_x_lock(&table->fts->cache->lock);

	/* Return if the table is already initialized for DOC ID */
	if (table->fts->cache->first_doc_id != FTS_NULL_DOC_ID) {
		rw_lock_x_unlock(&table->fts->cache->lock);
		return(0);
	}

	DEBUG_SYNC_C("fts_initialize_doc_id");

	/* Then compare this value with the ID value stored in the CONFIG
	table. The larger one will be our new initial Doc ID */
	fts_cmp_set_sync_doc_id(table, 0, FALSE, &max_doc_id);

	/* If DICT_TF2_FTS_ADD_DOC_ID is set, we are in the process of
	creating index (and add doc id column. No need to recovery
	documents */
	if (!DICT_TF2_FLAG_IS_SET(table, DICT_TF2_FTS_ADD_DOC_ID)) {
		fts_init_index((dict_table_t*) table, TRUE);
	}

	table->fts->fts_status |= ADDED_TABLE_SYNCED;

	table->fts->cache->first_doc_id = max_doc_id;

	rw_lock_x_unlock(&table->fts->cache->lock);

	ut_ad(max_doc_id > 0);

	return(max_doc_id);
}

#ifdef FTS_MULT_INDEX
/*********************************************************************//**
Check if the index is in the affected set.
@return TRUE if index is updated */
static
ibool
fts_is_index_updated(
/*=================*/
	const ib_vector_t*	fts_indexes,	/*!< in: affected FTS indexes */
	const fts_get_doc_t*	get_doc)	/*!< in: info for reading
						document */
{
	ulint		i;
	dict_index_t*	index = get_doc->index_cache->index;

	for (i = 0; i < ib_vector_size(fts_indexes); ++i) {
		const dict_index_t*	updated_fts_index;

		updated_fts_index = static_cast<const dict_index_t*>(
			ib_vector_getp_const(fts_indexes, i));

		ut_a(updated_fts_index != NULL);

		if (updated_fts_index == index) {
			return(TRUE);
		}
	}

	return(FALSE);
}
#endif

/*********************************************************************//**
Fetch COUNT(*) from specified table.
@return the number of rows in the table */
ulint
fts_get_rows_count(
/*===============*/
	fts_table_t*	fts_table)	/*!< in: fts table to read */
{
	trx_t*		trx;
	pars_info_t*	info;
	que_t*		graph;
	dberr_t		error;
	ulint		count = 0;
	char		table_name[MAX_FULL_NAME_LEN];

	trx = trx_allocate_for_background();

	trx->op_info = "fetching FT table rows count";

	info = pars_info_create();

	pars_info_bind_function(info, "my_func", fts_read_ulint, &count);

	fts_get_table_name(fts_table, table_name);
	pars_info_bind_id(info, true, "table_name", table_name);

	graph = fts_parse_sql(
		fts_table,
		info,
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

			break;				/* Exit the loop. */
		} else {
			fts_sql_rollback(trx);

			if (error == DB_LOCK_WAIT_TIMEOUT) {
				ib::warn() << "lock wait timeout reading"
					" FTS table. Retrying!";

				trx->error_state = DB_SUCCESS;
			} else {
				ib::error() << "(" << ut_strerr(error)
					<< ") while reading FTS table.";

				break;			/* Exit the loop. */
			}
		}
	}

	fts_que_graph_free(graph);

	trx_free_for_background(trx);

	return(count);
}

#ifdef FTS_CACHE_SIZE_DEBUG
/*********************************************************************//**
Read the max cache size parameter from the config table. */
static
void
fts_update_max_cache_size(
/*======================*/
	fts_sync_t*	sync)			/*!< in: sync state */
{
	trx_t*		trx;
	fts_table_t	fts_table;

	trx = trx_allocate_for_background();

	FTS_INIT_FTS_TABLE(&fts_table, "CONFIG", FTS_COMMON_TABLE, sync->table);

	/* The size returned is in bytes. */
	sync->max_cache_size = fts_get_max_cache_size(trx, &fts_table);

	fts_sql_commit(trx);

	trx_free_for_background(trx);
}
#endif /* FTS_CACHE_SIZE_DEBUG */

/*********************************************************************//**
Free the modified rows of a table. */
UNIV_INLINE
void
fts_trx_table_rows_free(
/*====================*/
	ib_rbt_t*	rows)			/*!< in: rbt of rows to free */
{
	const ib_rbt_node_t*	node;

	for (node = rbt_first(rows); node; node = rbt_first(rows)) {
		fts_trx_row_t*	row;

		row = rbt_value(fts_trx_row_t, node);

		if (row->fts_indexes != NULL) {
			/* This vector shouldn't be using the
			heap allocator.  */
			ut_a(row->fts_indexes->allocator->arg == NULL);

			ib_vector_free(row->fts_indexes);
			row->fts_indexes = NULL;
		}

		ut_free(rbt_remove_node(rows, node));
	}

	ut_a(rbt_empty(rows));
	rbt_free(rows);
}

/*********************************************************************//**
Free an FTS savepoint instance. */
UNIV_INLINE
void
fts_savepoint_free(
/*===============*/
	fts_savepoint_t*	savepoint)	/*!< in: savepoint instance */
{
	const ib_rbt_node_t*	node;
	ib_rbt_t*		tables = savepoint->tables;

	/* Nothing to free! */
	if (tables == NULL) {
		return;
	}

	for (node = rbt_first(tables); node; node = rbt_first(tables)) {
		fts_trx_table_t*	ftt;
		fts_trx_table_t**	fttp;

		fttp = rbt_value(fts_trx_table_t*, node);
		ftt = *fttp;

		/* This can be NULL if a savepoint was released. */
		if (ftt->rows != NULL) {
			fts_trx_table_rows_free(ftt->rows);
			ftt->rows = NULL;
		}

		/* This can be NULL if a savepoint was released. */
		if (ftt->added_doc_ids != NULL) {
			fts_doc_ids_free(ftt->added_doc_ids);
			ftt->added_doc_ids = NULL;
		}

		/* The default savepoint name must be NULL. */
		if (ftt->docs_added_graph) {
			fts_que_graph_free(ftt->docs_added_graph);
		}

		/* NOTE: We are responsible for free'ing the node */
		ut_free(rbt_remove_node(tables, node));
	}

	ut_a(rbt_empty(tables));
	rbt_free(tables);
	savepoint->tables = NULL;
}

/*********************************************************************//**
Free an FTS trx. */
void
fts_trx_free(
/*=========*/
	fts_trx_t*	fts_trx)		/* in, own: FTS trx */
{
	ulint		i;

	for (i = 0; i < ib_vector_size(fts_trx->savepoints); ++i) {
		fts_savepoint_t*	savepoint;

		savepoint = static_cast<fts_savepoint_t*>(
			ib_vector_get(fts_trx->savepoints, i));

		/* The default savepoint name must be NULL. */
		if (i == 0) {
			ut_a(savepoint->name == NULL);
		}

		fts_savepoint_free(savepoint);
	}

	for (i = 0; i < ib_vector_size(fts_trx->last_stmt); ++i) {
		fts_savepoint_t*	savepoint;

		savepoint = static_cast<fts_savepoint_t*>(
			ib_vector_get(fts_trx->last_stmt, i));

		/* The default savepoint name must be NULL. */
		if (i == 0) {
			ut_a(savepoint->name == NULL);
		}

		fts_savepoint_free(savepoint);
	}

	if (fts_trx->heap) {
		mem_heap_free(fts_trx->heap);
	}
}

/*********************************************************************//**
Extract the doc id from the FTS hidden column.
@return doc id that was extracted from rec */
doc_id_t
fts_get_doc_id_from_row(
/*====================*/
	dict_table_t*	table,			/*!< in: table */
	dtuple_t*	row)			/*!< in: row whose FTS doc id we
						want to extract.*/
{
	dfield_t*	field;
	doc_id_t	doc_id = 0;

	ut_a(table->fts->doc_col != ULINT_UNDEFINED);

	field = dtuple_get_nth_field(row, table->fts->doc_col);

	ut_a(dfield_get_len(field) == sizeof(doc_id));
	ut_a(dfield_get_type(field)->mtype == DATA_INT);

	doc_id = fts_read_doc_id(
		static_cast<const byte*>(dfield_get_data(field)));

	return(doc_id);
}

/** Extract the doc id from the record that belongs to index.
@param[in]	table	table
@param[in]	rec	record contains FTS_DOC_ID
@param[in]	index	index of rec
@param[in]	heap	heap memory
@return doc id that was extracted from rec */
doc_id_t
fts_get_doc_id_from_rec(
	dict_table_t*		table,
	const rec_t*		rec,
	const dict_index_t*	index,
	mem_heap_t*		heap)
{
	ulint		len;
	const byte*	data;
	ulint		col_no;
	doc_id_t	doc_id = 0;
	ulint		offsets_[REC_OFFS_NORMAL_SIZE];
	ulint*		offsets = offsets_;
	mem_heap_t*	my_heap = heap;

	ut_a(table->fts->doc_col != ULINT_UNDEFINED);

	rec_offs_init(offsets_);

	offsets = rec_get_offsets(
		rec, index, offsets, ULINT_UNDEFINED, &my_heap);

	col_no = dict_col_get_index_pos(
		&table->cols[table->fts->doc_col], index);

	ut_ad(col_no != ULINT_UNDEFINED);

	data = rec_get_nth_field(rec, offsets, col_no, &len);

	ut_a(len == 8);
	ut_ad(8 == sizeof(doc_id));
	doc_id = static_cast<doc_id_t>(mach_read_from_8(data));

	if (my_heap && !heap) {
		mem_heap_free(my_heap);
	}

	return(doc_id);
}

/*********************************************************************//**
Search the index specific cache for a particular FTS index.
@return the index specific cache else NULL */
fts_index_cache_t*
fts_find_index_cache(
/*=================*/
	const fts_cache_t*	cache,		/*!< in: cache to search */
	const dict_index_t*	index)		/*!< in: index to search for */
{
	/* We cast away the const because our internal function, takes
	non-const cache arg and returns a non-const pointer. */
	return(static_cast<fts_index_cache_t*>(
		fts_get_index_cache((fts_cache_t*) cache, index)));
}

/*********************************************************************//**
Search cache for word.
@return the word node vector if found else NULL */
const ib_vector_t*
fts_cache_find_word(
/*================*/
	const fts_index_cache_t*index_cache,	/*!< in: cache to search */
	const fts_string_t*	text)		/*!< in: word to search for */
{
	ib_rbt_bound_t		parent;
	const ib_vector_t*	nodes = NULL;
#ifdef UNIV_DEBUG
	dict_table_t*		table = index_cache->index->table;
	fts_cache_t*		cache = table->fts->cache;

	ut_ad(rw_lock_own(&cache->lock, RW_LOCK_X));
#endif /* UNIV_DEBUG */

	/* Lookup the word in the rb tree */
	if (rbt_search(index_cache->words, &parent, text) == 0) {
		const fts_tokenizer_word_t*	word;

		word = rbt_value(fts_tokenizer_word_t, parent.last);

		nodes = word->nodes;
	}

	return(nodes);
}

/*********************************************************************//**
Check cache for deleted doc id.
@return TRUE if deleted */
ibool
fts_cache_is_deleted_doc_id(
/*========================*/
	const fts_cache_t*	cache,		/*!< in: cache ito search */
	doc_id_t		doc_id)		/*!< in: doc id to search for */
{
	ut_ad(mutex_own(&cache->deleted_lock));

	for (ulint i = 0; i < ib_vector_size(cache->deleted_doc_ids); ++i) {
		const fts_update_t*	update;

		update = static_cast<const fts_update_t*>(
			ib_vector_get_const(cache->deleted_doc_ids, i));

		if (doc_id == update->doc_id) {

			return(TRUE);
		}
	}

	return(FALSE);
}

/*********************************************************************//**
Append deleted doc ids to vector. */
void
fts_cache_append_deleted_doc_ids(
/*=============================*/
	const fts_cache_t*	cache,		/*!< in: cache to use */
	ib_vector_t*		vector)		/*!< in: append to this vector */
{
	mutex_enter(const_cast<ib_mutex_t*>(&cache->deleted_lock));

	if (cache->deleted_doc_ids == NULL) {
		mutex_exit((ib_mutex_t*) &cache->deleted_lock);
		return;
	}


	for (ulint i = 0; i < ib_vector_size(cache->deleted_doc_ids); ++i) {
		fts_update_t*	update;

		update = static_cast<fts_update_t*>(
			ib_vector_get(cache->deleted_doc_ids, i));

		ib_vector_push(vector, &update->doc_id);
	}

	mutex_exit((ib_mutex_t*) &cache->deleted_lock);
}

/*********************************************************************//**
Wait for the background thread to start. We poll to detect change
of state, which is acceptable, since the wait should happen only
once during startup.
@return true if the thread started else FALSE (i.e timed out) */
ibool
fts_wait_for_background_thread_to_start(
/*====================================*/
	dict_table_t*		table,		/*!< in: table to which the thread
						is attached */
	ulint			max_wait)	/*!< in: time in microseconds, if
						set to 0 then it disables
						timeout checking */
{
	ulint			count = 0;
	ibool			done = FALSE;

	ut_a(max_wait == 0 || max_wait >= FTS_MAX_BACKGROUND_THREAD_WAIT);

	for (;;) {
		fts_t*		fts = table->fts;

		mutex_enter(&fts->bg_threads_mutex);

		if (fts->fts_status & BG_THREAD_READY) {

			done = TRUE;
		}

		mutex_exit(&fts->bg_threads_mutex);

		if (!done) {
			os_thread_sleep(FTS_MAX_BACKGROUND_THREAD_WAIT);

			if (max_wait > 0) {

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
			ib::error() << "The background thread for the FTS"
				" table " << table->name
				<< " refuses to start";

			count = 0;
		}
	}

	return(done);
}

/*********************************************************************//**
Add the FTS document id hidden column. */
void
fts_add_doc_id_column(
/*==================*/
	dict_table_t*	table,	/*!< in/out: Table with FTS index */
	mem_heap_t*	heap)	/*!< in: temporary memory heap, or NULL */
{
	dict_mem_table_add_col(
		table, heap,
		FTS_DOC_ID_COL_NAME,
		DATA_INT,
		dtype_form_prtype(
			DATA_NOT_NULL | DATA_UNSIGNED
			| DATA_BINARY_TYPE | DATA_FTS_DOC_ID, 0),
		sizeof(doc_id_t));
	DICT_TF2_FLAG_SET(table, DICT_TF2_FTS_HAS_DOC_ID);
}

/** Add new fts doc id to the update vector.
@param[in]	table		the table that contains the FTS index.
@param[in,out]	ufield		the fts doc id field in the update vector.
				No new memory is allocated for this in this
				function.
@param[in,out]	next_doc_id	the fts doc id that has been added to the
				update vector.  If 0, a new fts doc id is
				automatically generated.  The memory provided
				for this argument will be used by the update
				vector. Ensure that the life time of this
				memory matches that of the update vector.
@return the fts doc id used in the update vector */
doc_id_t
fts_update_doc_id(
	dict_table_t*	table,
	upd_field_t*	ufield,
	doc_id_t*	next_doc_id)
{
	doc_id_t	doc_id;
	dberr_t		error = DB_SUCCESS;

	if (*next_doc_id) {
		doc_id = *next_doc_id;
	} else {
		/* Get the new document id that will be added. */
		error = fts_get_next_doc_id(table, &doc_id);
	}

	if (error == DB_SUCCESS) {
		dict_index_t*	clust_index;
		dict_col_t*	col = dict_table_get_nth_col(
			table, table->fts->doc_col);

		ufield->exp = NULL;

		ufield->new_val.len = sizeof(doc_id);

		clust_index = dict_table_get_first_index(table);

		ufield->field_no = dict_col_get_clust_pos(col, clust_index);
		dict_col_copy_type(col, dfield_get_type(&ufield->new_val));

		/* It is possible we update record that has
		not yet be sync-ed from last crash. */

		/* Convert to storage byte order. */
		ut_a(doc_id != FTS_NULL_DOC_ID);
		fts_write_doc_id((byte*) next_doc_id, doc_id);

		ufield->new_val.data = next_doc_id;
                ufield->new_val.ext = 0;
	}

	return(doc_id);
}

/*********************************************************************//**
Check if the table has an FTS index. This is the non-inline version
of dict_table_has_fts_index().
@return TRUE if table has an FTS index */
ibool
fts_dict_table_has_fts_index(
/*=========================*/
	dict_table_t*	table)		/*!< in: table */
{
	return(dict_table_has_fts_index(table));
}

/** fts_t constructor.
@param[in]	table	table with FTS indexes
@param[in,out]	heap	memory heap where 'this' is stored */
fts_t::fts_t(
	const dict_table_t*	table,
	mem_heap_t*		heap)
	:
	bg_threads(0),
	fts_status(0),
	add_wq(NULL),
	cache(NULL),
	doc_col(ULINT_UNDEFINED),
	fts_heap(heap)
{
	ut_a(table->fts == NULL);

	mutex_create(LATCH_ID_FTS_BG_THREADS, &bg_threads_mutex);

	ib_alloc_t*	heap_alloc = ib_heap_allocator_create(fts_heap);

	indexes = ib_vector_create(heap_alloc, sizeof(dict_index_t*), 4);

	dict_table_get_all_fts_indexes(table, indexes);
}

/** fts_t destructor. */
fts_t::~fts_t()
{
	mutex_free(&bg_threads_mutex);

	ut_ad(add_wq == NULL);

	if (cache != NULL) {
		fts_cache_clear(cache);
		fts_cache_destroy(cache);
		cache = NULL;
	}

	/* There is no need to call ib_vector_free() on this->indexes
	because it is stored in this->fts_heap. */
}

/*********************************************************************//**
Create an instance of fts_t.
@return instance of fts_t */
fts_t*
fts_create(
/*=======*/
	dict_table_t*	table)		/*!< in/out: table with FTS indexes */
{
	fts_t*		fts;
	mem_heap_t*	heap;

	heap = mem_heap_create(512);

	fts = static_cast<fts_t*>(mem_heap_alloc(heap, sizeof(*fts)));

	new(fts) fts_t(table, heap);

	return(fts);
}

/*********************************************************************//**
Free the FTS resources. */
void
fts_free(
/*=====*/
	dict_table_t*	table)	/*!< in/out: table with FTS indexes */
{
	fts_t*	fts = table->fts;

	fts->~fts_t();

	mem_heap_free(fts->fts_heap);

	table->fts = NULL;
}

/*********************************************************************//**
Signal FTS threads to initiate shutdown. */
void
fts_start_shutdown(
/*===============*/
	dict_table_t*	table,		/*!< in: table with FTS indexes */
	fts_t*		fts)		/*!< in: fts instance that needs
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
/*=========*/
	dict_table_t*	table,		/*!< in: table with FTS indexes */
	fts_t*		fts)		/*!< in: fts instance to shutdown */
{
	mutex_enter(&fts->bg_threads_mutex);

	ut_a(fts->fts_status & BG_THREAD_STOP);

	dict_table_wait_for_bg_threads_to_exit(table, 20000);

	mutex_exit(&fts->bg_threads_mutex);
}

/*********************************************************************//**
Take a FTS savepoint. */
UNIV_INLINE
void
fts_savepoint_copy(
/*===============*/
	const fts_savepoint_t*	src,	/*!< in: source savepoint */
	fts_savepoint_t*	dst)	/*!< out: destination savepoint */
{
	const ib_rbt_node_t*	node;
	const ib_rbt_t*		tables;

	tables = src->tables;

	for (node = rbt_first(tables); node; node = rbt_next(tables, node)) {

		fts_trx_table_t*	ftt_dst;
		const fts_trx_table_t**	ftt_src;

		ftt_src = rbt_value(const fts_trx_table_t*, node);

		ftt_dst = fts_trx_table_clone(*ftt_src);

		rbt_insert(dst->tables, &ftt_dst, &ftt_dst);
	}
}

/*********************************************************************//**
Take a FTS savepoint. */
void
fts_savepoint_take(
/*===============*/
	trx_t*		trx,		/*!< in: transaction */
	fts_trx_t*	fts_trx,	/*!< in: fts transaction */
	const char*	name)		/*!< in: savepoint name */
{
	mem_heap_t*		heap;
	fts_savepoint_t*	savepoint;
	fts_savepoint_t*	last_savepoint;

	ut_a(name != NULL);

	heap = fts_trx->heap;

	/* The implied savepoint must exist. */
	ut_a(ib_vector_size(fts_trx->savepoints) > 0);

	last_savepoint = static_cast<fts_savepoint_t*>(
		ib_vector_last(fts_trx->savepoints));
	savepoint = fts_savepoint_create(fts_trx->savepoints, name, heap);

	if (last_savepoint->tables != NULL) {
		fts_savepoint_copy(last_savepoint, savepoint);
	}
}

/*********************************************************************//**
Lookup a savepoint instance by name.
@return ULINT_UNDEFINED if not found */
UNIV_INLINE
ulint
fts_savepoint_lookup(
/*==================*/
	ib_vector_t*	savepoints,	/*!< in: savepoints */
	const char*	name)		/*!< in: savepoint name */
{
	ulint			i;

	ut_a(ib_vector_size(savepoints) > 0);

	for (i = 1; i < ib_vector_size(savepoints); ++i) {
		fts_savepoint_t*	savepoint;

		savepoint = static_cast<fts_savepoint_t*>(
			ib_vector_get(savepoints, i));

		if (strcmp(name, savepoint->name) == 0) {
			return(i);
		}
	}

	return(ULINT_UNDEFINED);
}

/*********************************************************************//**
Release the savepoint data identified by  name. All savepoints created
after the named savepoint are kept.
@return DB_SUCCESS or error code */
void
fts_savepoint_release(
/*==================*/
	trx_t*		trx,		/*!< in: transaction */
	const char*	name)		/*!< in: savepoint name */
{
	ut_a(name != NULL);

	ib_vector_t*	savepoints = trx->fts_trx->savepoints;

	ut_a(ib_vector_size(savepoints) > 0);

	ulint   i = fts_savepoint_lookup(savepoints, name);
	if (i != ULINT_UNDEFINED) {
		ut_a(i >= 1);

		fts_savepoint_t*        savepoint;
		savepoint = static_cast<fts_savepoint_t*>(
			ib_vector_get(savepoints, i));

		if (i == ib_vector_size(savepoints) - 1) {
			/* If the savepoint is the last, we save its
			tables to the  previous savepoint. */
			fts_savepoint_t*	prev_savepoint;
			prev_savepoint = static_cast<fts_savepoint_t*>(
				ib_vector_get(savepoints, i - 1));

			ib_rbt_t*	tables = savepoint->tables;
			savepoint->tables = prev_savepoint->tables;
			prev_savepoint->tables = tables;
		}

		fts_savepoint_free(savepoint);
		ib_vector_remove(savepoints, *(void**)savepoint);

		/* Make sure we don't delete the implied savepoint. */
		ut_a(ib_vector_size(savepoints) > 0);
	}
}

/**********************************************************************//**
Refresh last statement savepoint. */
void
fts_savepoint_laststmt_refresh(
/*===========================*/
	trx_t*			trx)	/*!< in: transaction */
{

	fts_trx_t*              fts_trx;
	fts_savepoint_t*        savepoint;

	fts_trx = trx->fts_trx;

	savepoint = static_cast<fts_savepoint_t*>(
		ib_vector_pop(fts_trx->last_stmt));
	fts_savepoint_free(savepoint);

	ut_ad(ib_vector_is_empty(fts_trx->last_stmt));
	savepoint = fts_savepoint_create(fts_trx->last_stmt, NULL, NULL);
}

/********************************************************************
Undo the Doc ID add/delete operations in last stmt */
static
void
fts_undo_last_stmt(
/*===============*/
	fts_trx_table_t*	s_ftt,	/*!< in: Transaction FTS table */
	fts_trx_table_t*	l_ftt)	/*!< in: last stmt FTS table */
{
	ib_rbt_t*		s_rows;
	ib_rbt_t*		l_rows;
	const ib_rbt_node_t*	node;

	l_rows = l_ftt->rows;
	s_rows = s_ftt->rows;

	for (node = rbt_first(l_rows);
	     node;
	     node = rbt_next(l_rows, node)) {
		fts_trx_row_t*	l_row = rbt_value(fts_trx_row_t, node);
		ib_rbt_bound_t	parent;

		rbt_search(s_rows, &parent, &(l_row->doc_id));

		if (parent.result == 0) {
			fts_trx_row_t*	s_row = rbt_value(
				fts_trx_row_t, parent.last);

			switch (l_row->state) {
			case FTS_INSERT:
				ut_free(rbt_remove_node(s_rows, parent.last));
				break;

			case FTS_DELETE:
				if (s_row->state == FTS_NOTHING) {
					s_row->state = FTS_INSERT;
				} else if (s_row->state == FTS_DELETE) {
					ut_free(rbt_remove_node(
						s_rows, parent.last));
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

/**********************************************************************//**
Rollback to savepoint indentified by name.
@return DB_SUCCESS or error code */
void
fts_savepoint_rollback_last_stmt(
/*=============================*/
	trx_t*		trx)		/*!< in: transaction */
{
	ib_vector_t*		savepoints;
	fts_savepoint_t*	savepoint;
	fts_savepoint_t*	last_stmt;
	fts_trx_t*		fts_trx;
	ib_rbt_bound_t		parent;
	const ib_rbt_node_t*    node;
	ib_rbt_t*		l_tables;
	ib_rbt_t*		s_tables;

	fts_trx = trx->fts_trx;
	savepoints = fts_trx->savepoints;

	savepoint = static_cast<fts_savepoint_t*>(ib_vector_last(savepoints));
	last_stmt = static_cast<fts_savepoint_t*>(
		ib_vector_last(fts_trx->last_stmt));

	l_tables = last_stmt->tables;
	s_tables = savepoint->tables;

	for (node = rbt_first(l_tables);
	     node;
	     node = rbt_next(l_tables, node)) {

		fts_trx_table_t**	l_ftt;

		l_ftt = rbt_value(fts_trx_table_t*, node);

		rbt_search_cmp(
			s_tables, &parent, &(*l_ftt)->table->id,
			fts_trx_table_id_cmp, NULL);

		if (parent.result == 0) {
			fts_trx_table_t**	s_ftt;

			s_ftt = rbt_value(fts_trx_table_t*, parent.last);

			fts_undo_last_stmt(*s_ftt, *l_ftt);
		}
	}
}

/**********************************************************************//**
Rollback to savepoint indentified by name.
@return DB_SUCCESS or error code */
void
fts_savepoint_rollback(
/*===================*/
	trx_t*		trx,		/*!< in: transaction */
	const char*	name)		/*!< in: savepoint name */
{
	ulint		i;
	ib_vector_t*	savepoints;

	ut_a(name != NULL);

	savepoints = trx->fts_trx->savepoints;

	/* We pop all savepoints from the the top of the stack up to
	and including the instance that was found. */
	i = fts_savepoint_lookup(savepoints, name);

	if (i != ULINT_UNDEFINED) {
		fts_savepoint_t*	savepoint;

		ut_a(i > 0);

		while (ib_vector_size(savepoints) > i) {
			fts_savepoint_t*	savepoint;

			savepoint = static_cast<fts_savepoint_t*>(
				ib_vector_pop(savepoints));

			if (savepoint->name != NULL) {
				/* Since name was allocated on the heap, the
				memory will be released when the transaction
				completes. */
				savepoint->name = NULL;

				fts_savepoint_free(savepoint);
			}
		}

		/* Pop all a elements from the top of the stack that may
		have been released. We have to be careful that we don't
		delete the implied savepoint. */

		for (savepoint = static_cast<fts_savepoint_t*>(
				ib_vector_last(savepoints));
		     ib_vector_size(savepoints) > 1
		     && savepoint->name == NULL;
		     savepoint = static_cast<fts_savepoint_t*>(
				ib_vector_last(savepoints))) {

			ib_vector_pop(savepoints);
		}

		/* Make sure we don't delete the implied savepoint. */
		ut_a(ib_vector_size(savepoints) > 0);

		/* Restore the savepoint. */
		fts_savepoint_take(trx, trx->fts_trx, name);
	}
}

/** Check if a table is an FTS auxiliary table name.
@param[out]	table	FTS table info
@param[in]	name	Table name
@param[in]	len	Length of table name
@return true if the name matches an auxiliary table name pattern */
static
bool
fts_is_aux_table_name(
	fts_aux_table_t*	table,
	const char*		name,
	ulint			len)
{
	const char*	ptr;
	char*		end;
	char		my_name[MAX_FULL_NAME_LEN + 1];

	ut_ad(len <= MAX_FULL_NAME_LEN);
	ut_memcpy(my_name, name, len);
	my_name[len] = 0;
	end = my_name + len;

	ptr = static_cast<const char*>(memchr(my_name, '/', len));

	if (ptr != NULL) {
		/* We will start the match after the '/' */
		++ptr;
		len = end - ptr;
	}

	/* All auxiliary tables are prefixed with "FTS_" and the name
	length will be at the very least greater than 20 bytes. */
	if (ptr != NULL && len > 20 && strncmp(ptr, "FTS_", 4) == 0) {
		ulint		i;

		/* Skip the prefix. */
		ptr += 4;
		len -= 4;

		/* Try and read the table id. */
		if (!fts_read_object_id(&table->parent_id, ptr)) {
			return(false);
		}

		/* Skip the table id. */
		ptr = static_cast<const char*>(memchr(ptr, '_', len));

		if (ptr == NULL) {
			return(false);
		}

		/* Skip the underscore. */
		++ptr;
		ut_a(end > ptr);
		len = end - ptr;

		/* First search the common table suffix array. */
		for (i = 0; fts_common_tables[i] != NULL; ++i) {

			if (strncmp(ptr, fts_common_tables[i], len) == 0) {
				return(true);
			}
		}

		/* Could be obsolete common tables. */
		if (strncmp(ptr, "ADDED", len) == 0
		    || strncmp(ptr, "STOPWORDS", len) == 0) {
			return(true);
		}

		/* Try and read the index id. */
		if (!fts_read_object_id(&table->index_id, ptr)) {
			return(false);
		}

		/* Skip the table id. */
		ptr = static_cast<const char*>(memchr(ptr, '_', len));

		if (ptr == NULL) {
			return(false);
		}

		/* Skip the underscore. */
		++ptr;
		ut_a(end > ptr);
		len = end - ptr;

		/* Search the FT index specific array. */
		for (i = 0; i < FTS_NUM_AUX_INDEX; ++i) {

			if (strncmp(ptr, fts_get_suffix(i), len) == 0) {
				return(true);
			}
		}

		/* Other FT index specific table(s). */
		if (strncmp(ptr, "DOC_ID", len) == 0) {
			return(true);
		}
	}

	return(false);
}

/**********************************************************************//**
Callback function to read a single table ID column.
@return Always return TRUE */
static
ibool
fts_read_tables(
/*============*/
	void*		row,		/*!< in: sel_node_t* */
	void*		user_arg)	/*!< in: pointer to ib_vector_t */
{
	int		i;
	fts_aux_table_t*table;
	mem_heap_t*	heap;
	ibool		done = FALSE;
	ib_vector_t*	tables = static_cast<ib_vector_t*>(user_arg);
	sel_node_t*	sel_node = static_cast<sel_node_t*>(row);
	que_node_t*	exp = sel_node->select_list;

	/* Must be a heap allocated vector. */
	ut_a(tables->allocator->arg != NULL);

	/* We will use this heap for allocating strings. */
	heap = static_cast<mem_heap_t*>(tables->allocator->arg);
	table = static_cast<fts_aux_table_t*>(ib_vector_push(tables, NULL));

	memset(table, 0x0, sizeof(*table));

	/* Iterate over the columns and read the values. */
	for (i = 0; exp && !done; exp = que_node_get_next(exp), ++i) {

		dfield_t*	dfield = que_node_get_val(exp);
		void*		data = dfield_get_data(dfield);
		ulint		len = dfield_get_len(dfield);

		ut_a(len != UNIV_SQL_NULL);

		/* Note: The column numbers below must match the SELECT */
		switch (i) {
		case 0: /* NAME */

			if (!fts_is_aux_table_name(
				table, static_cast<const char*>(data), len)) {
				ib_vector_pop(tables);
				done = TRUE;
				break;
			}

			table->name = static_cast<char*>(
				mem_heap_alloc(heap, len + 1));
			memcpy(table->name, data, len);
			table->name[len] = 0;
			break;

		case 1: /* ID */
			ut_a(len == 8);
			table->id = mach_read_from_8(
				static_cast<const byte*>(data));
			break;

		default:
			ut_error;
		}
	}

	return(TRUE);
}

/******************************************************************//**
Callback that sets a hex formatted FTS table's flags2 in
SYS_TABLES. The flags is stored in MIX_LEN column.
@return FALSE if all OK */
static
ibool
fts_set_hex_format(
/*===============*/
	void*		row,		/*!< in: sel_node_t* */
	void*		user_arg)	/*!< in: bool set/unset flag */
{
	sel_node_t*	node = static_cast<sel_node_t*>(row);
	dfield_t*	dfield = que_node_get_val(node->select_list);

	ut_ad(dtype_get_mtype(dfield_get_type(dfield)) == DATA_INT);
	ut_ad(dfield_get_len(dfield) == sizeof(ib_uint32_t));
	/* There should be at most one matching record. So the value
	must be the default value. */
	ut_ad(mach_read_from_4(static_cast<byte*>(user_arg))
	      == ULINT32_UNDEFINED);

	ulint		flags2 = mach_read_from_4(
			static_cast<byte*>(dfield_get_data(dfield)));

	flags2 |= DICT_TF2_FTS_AUX_HEX_NAME;

	mach_write_to_4(static_cast<byte*>(user_arg), flags2);

	return(FALSE);
}

/*****************************************************************//**
Update the DICT_TF2_FTS_AUX_HEX_NAME flag in SYS_TABLES.
@return DB_SUCCESS or error code. */
dberr_t
fts_update_hex_format_flag(
/*=======================*/
	trx_t*		trx,		/*!< in/out: transaction that
					covers the update */
	table_id_t	table_id,	/*!< in: Table for which we want
					to set the root table->flags2 */
	bool		dict_locked)	/*!< in: set to true if the
					caller already owns the
					dict_sys_t::mutex. */
{
	pars_info_t*		info;
	ib_uint32_t		flags2;

	static const char	sql[] =
		"PROCEDURE UPDATE_HEX_FORMAT_FLAG() IS\n"
		"DECLARE FUNCTION my_func;\n"
		"DECLARE CURSOR c IS\n"
		" SELECT MIX_LEN"
		" FROM SYS_TABLES"
		" WHERE ID = :table_id FOR UPDATE;"
		"\n"
		"BEGIN\n"
		"OPEN c;\n"
		"WHILE 1 = 1 LOOP\n"
		"  FETCH c INTO my_func();\n"
		"  IF c % NOTFOUND THEN\n"
		"    EXIT;\n"
		"  END IF;\n"
		"END LOOP;\n"
		"UPDATE SYS_TABLES"
		" SET MIX_LEN = :flags2"
		" WHERE ID = :table_id;\n"
		"CLOSE c;\n"
		"END;\n";

	flags2 = ULINT32_UNDEFINED;

	info = pars_info_create();

	pars_info_add_ull_literal(info, "table_id", table_id);
	pars_info_bind_int4_literal(info, "flags2", &flags2);

	pars_info_bind_function(
		info, "my_func", fts_set_hex_format, &flags2);

	if (trx_get_dict_operation(trx) == TRX_DICT_OP_NONE) {
		trx_set_dict_operation(trx, TRX_DICT_OP_INDEX);
	}

	dberr_t err = que_eval_sql(info, sql, !dict_locked, trx);

	ut_a(flags2 != ULINT32_UNDEFINED);

	return(err);
}

/*********************************************************************//**
Rename an aux table to HEX format. It's called when "%016llu" is used
to format an object id in table name, which only happens in Windows. */
static MY_ATTRIBUTE((nonnull, warn_unused_result))
dberr_t
fts_rename_one_aux_table_to_hex_format(
/*===================================*/
	trx_t*			trx,		/*!< in: transaction */
	const fts_aux_table_t*	aux_table,	/*!< in: table info */
	const dict_table_t*	parent_table)	/*!< in: parent table name */
{
	const char*     ptr;
	fts_table_t	fts_table;
	char		new_name[MAX_FULL_NAME_LEN];
	dberr_t		error;

	ptr = strchr(aux_table->name, '/');
	ut_a(ptr != NULL);
	++ptr;
	/* Skip "FTS_", table id and underscore */
	for (ulint i = 0; i < 2; ++i) {
		ptr = strchr(ptr, '_');
		ut_a(ptr != NULL);
		++ptr;
	}

	fts_table.suffix = NULL;
	if (aux_table->index_id == 0) {
		fts_table.type = FTS_COMMON_TABLE;

		for (ulint i = 0; fts_common_tables[i] != NULL; ++i) {
			if (strcmp(ptr, fts_common_tables[i]) == 0) {
				fts_table.suffix = fts_common_tables[i];
				break;
			}
		}
	} else {
		fts_table.type = FTS_INDEX_TABLE;

		/* Skip index id and underscore */
		ptr = strchr(ptr, '_');
		ut_a(ptr != NULL);
		++ptr;

		for (ulint i = 0; fts_index_selector[i].value; ++i) {
			if (strcmp(ptr, fts_get_suffix(i)) == 0) {
				fts_table.suffix = fts_get_suffix(i);
				break;
			}
		}
	}

	ut_a(fts_table.suffix != NULL);

	fts_table.parent = parent_table->name.m_name;
	fts_table.table_id = aux_table->parent_id;
	fts_table.index_id = aux_table->index_id;
	fts_table.table = parent_table;

	fts_get_table_name(&fts_table, new_name);
	ut_ad(strcmp(new_name, aux_table->name) != 0);

	if (trx_get_dict_operation(trx) == TRX_DICT_OP_NONE) {
		trx_set_dict_operation(trx, TRX_DICT_OP_INDEX);
	}

	error = row_rename_table_for_mysql(aux_table->name, new_name, trx,
					   FALSE);

	if (error != DB_SUCCESS) {
		ib::warn() << "Failed to rename aux table '"
			<< aux_table->name << "' to new format '"
			<< new_name << "'.";
	} else {
		ib::info() << "Renamed aux table '" << aux_table->name
			<< "' to '" << new_name << "'.";
	}

	return(error);
}

/**********************************************************************//**
Rename all aux tables of a parent table to HEX format. Also set aux tables'
flags2 and parent table's flags2 with DICT_TF2_FTS_AUX_HEX_NAME.
It's called when "%016llu" is used to format an object id in table name,
which only happens in Windows.
Note the ids in tables are correct but the names are old ambiguous ones.

This function should make sure that either all the parent table and aux tables
are set DICT_TF2_FTS_AUX_HEX_NAME with flags2 or none of them are set */
static MY_ATTRIBUTE((nonnull, warn_unused_result))
dberr_t
fts_rename_aux_tables_to_hex_format_low(
/*====================================*/
	trx_t*		trx,		/*!< in: transaction */
	dict_table_t*	parent_table,	/*!< in: parent table */
	ib_vector_t*	tables)		/*!< in: aux tables to rename. */
{
	dberr_t		error;
	ulint		count;

	ut_ad(!DICT_TF2_FLAG_IS_SET(parent_table, DICT_TF2_FTS_AUX_HEX_NAME));
	ut_ad(!ib_vector_is_empty(tables));

	error = fts_update_hex_format_flag(trx, parent_table->id, true);

	if (error != DB_SUCCESS) {
		ib::warn() << "Setting parent table " << parent_table->name
			<< " to hex format failed.";
		fts_sql_rollback(trx);
		return(error);
	}

	DICT_TF2_FLAG_SET(parent_table, DICT_TF2_FTS_AUX_HEX_NAME);

	for (count = 0; count < ib_vector_size(tables); ++count) {
		dict_table_t*		table;
		fts_aux_table_t*	aux_table;

		aux_table = static_cast<fts_aux_table_t*>(
			ib_vector_get(tables, count));

		table = dict_table_open_on_id(aux_table->id, TRUE,
					      DICT_TABLE_OP_NORMAL);

		ut_ad(table != NULL);
		ut_ad(!DICT_TF2_FLAG_IS_SET(table, DICT_TF2_FTS_AUX_HEX_NAME));

		/* Set HEX_NAME flag here to make sure we can get correct
		new table name in following function */
		DICT_TF2_FLAG_SET(table, DICT_TF2_FTS_AUX_HEX_NAME);
		error = fts_rename_one_aux_table_to_hex_format(trx,
				aux_table, parent_table);
		/* We will rollback the trx if the error != DB_SUCCESS,
		so setting the flag here is the same with setting it in
		row_rename_table_for_mysql */
		DBUG_EXECUTE_IF("rename_aux_table_fail", error = DB_ERROR;);

		if (error != DB_SUCCESS) {
			dict_table_close(table, TRUE, FALSE);

			ib::warn() << "Failed to rename one aux table "
				<< aux_table->name << ". Will revert"
				" all successful rename operations.";

			fts_sql_rollback(trx);
			break;
		}

		error = fts_update_hex_format_flag(trx, aux_table->id, true);
		dict_table_close(table, TRUE, FALSE);

		if (error != DB_SUCCESS) {
			ib::warn() << "Setting aux table " << aux_table->name
				<< " to hex format failed.";

			fts_sql_rollback(trx);
			break;
		}
	}

	if (error != DB_SUCCESS) {
		ut_ad(count != ib_vector_size(tables));

		/* If rename fails, thr trx would be rolled back, we can't
		use it any more, we'll start a new background trx to do
		the reverting. */

		ut_ad(!trx_is_started(trx));

		bool not_rename = false;

		/* Try to revert those succesful rename operations
		in order to revert the ibd file rename. */
		for (ulint i = 0; i <= count; ++i) {
			dict_table_t*		table;
			fts_aux_table_t*	aux_table;
			trx_t*			trx_bg;
			dberr_t			err;

			aux_table = static_cast<fts_aux_table_t*>(
				ib_vector_get(tables, i));

			table = dict_table_open_on_id(aux_table->id, TRUE,
						      DICT_TABLE_OP_NORMAL);
			ut_ad(table != NULL);

			if (not_rename) {
				DICT_TF2_FLAG_UNSET(table,
						    DICT_TF2_FTS_AUX_HEX_NAME);
			}

			if (!DICT_TF2_FLAG_IS_SET(table,
						  DICT_TF2_FTS_AUX_HEX_NAME)) {
				dict_table_close(table, TRUE, FALSE);
				continue;
			}

			trx_bg = trx_allocate_for_background();
			trx_bg->op_info = "Revert half done rename";
			trx_bg->dict_operation_lock_mode = RW_X_LATCH;
			trx_start_for_ddl(trx_bg, TRX_DICT_OP_TABLE);

			DICT_TF2_FLAG_UNSET(table, DICT_TF2_FTS_AUX_HEX_NAME);
			err = row_rename_table_for_mysql(table->name.m_name,
							 aux_table->name,
							 trx_bg, FALSE);

			trx_bg->dict_operation_lock_mode = 0;
			dict_table_close(table, TRUE, FALSE);

			if (err != DB_SUCCESS) {
				ib::warn() << "Failed to revert table "
					<< table->name << ". Please revert"
					" manually.";
				fts_sql_rollback(trx_bg);
				trx_free_for_background(trx_bg);
				/* Continue to clear aux tables' flags2 */
				not_rename = true;
				continue;
			}

			fts_sql_commit(trx_bg);
			trx_free_for_background(trx_bg);
		}

		DICT_TF2_FLAG_UNSET(parent_table, DICT_TF2_FTS_AUX_HEX_NAME);
	}

	return(error);
}

/**********************************************************************//**
Convert an id, which is actually a decimal number but was regard as a HEX
from a string, to its real value. */
static
ib_id_t
fts_fake_hex_to_dec(
/*================*/
	ib_id_t		id)			/*!< in: number to convert */
{
	ib_id_t		dec_id = 0;
	char		tmp_id[FTS_AUX_MIN_TABLE_ID_LENGTH];

#ifdef UNIV_DEBUG
	int		ret =
#endif /* UNIV_DEBUG */
	sprintf(tmp_id, UINT64PFx, id);
	ut_ad(ret == 16);
#ifdef UNIV_DEBUG
	ret =
#endif /* UNIV_DEBUG */
#ifdef _WIN32
	sscanf(tmp_id, "%016llu", &dec_id);
#else
	sscanf(tmp_id, "%016" PRIu64, &dec_id);
#endif /* _WIN32 */
	ut_ad(ret == 1);

	return dec_id;
}

/*********************************************************************//**
Compare two fts_aux_table_t parent_ids.
@return < 0 if n1 < n2, 0 if n1 == n2, > 0 if n1 > n2 */
UNIV_INLINE
int
fts_check_aux_table_parent_id_cmp(
/*==============================*/
	const void*	p1,		/*!< in: id1 */
	const void*	p2)		/*!< in: id2 */
{
	const fts_aux_table_t*	fa1 = static_cast<const fts_aux_table_t*>(p1);
	const fts_aux_table_t*	fa2 = static_cast<const fts_aux_table_t*>(p2);

	return static_cast<int>(fa1->parent_id - fa2->parent_id);
}

/** Mark all the fts index associated with the parent table as corrupted.
@param[in]	trx		transaction
@param[in, out] parent_table	fts index associated with this parent table
				will be marked as corrupted. */
static
void
fts_parent_all_index_set_corrupt(
	trx_t*		trx,
	dict_table_t*	parent_table)
{
	fts_t*	fts = parent_table->fts;

	if (trx_get_dict_operation(trx) == TRX_DICT_OP_NONE) {
		trx_set_dict_operation(trx, TRX_DICT_OP_INDEX);
	}

	for (ulint j = 0; j < ib_vector_size(fts->indexes); j++) {
		dict_index_t*	index = static_cast<dict_index_t*>(
			ib_vector_getp_const(fts->indexes, j));
		dict_set_corrupted(index,
				   trx, "DROP ORPHANED TABLE");
	}
}

/** Mark the fts index which index id matches the id as corrupted.
@param[in]	trx		transaction
@param[in]	id		index id to search
@param[in, out]	parent_table	parent table to check with all
				the index. */
static
void
fts_set_index_corrupt(
	trx_t*		trx,
	index_id_t	id,
	dict_table_t*	table)
{
	fts_t*	fts = table->fts;

	if (trx_get_dict_operation(trx) == TRX_DICT_OP_NONE) {
		trx_set_dict_operation(trx, TRX_DICT_OP_INDEX);
	}

	for (ulint j = 0; j < ib_vector_size(fts->indexes); j++) {
		dict_index_t*	index = static_cast<dict_index_t*>(
			ib_vector_getp_const(fts->indexes, j));
		if (index->id == id) {
			dict_set_corrupted(index, trx,
					   "DROP ORPHANED TABLE");
			break;
		}
	}
}

/** Check the index for the aux table is corrupted.
@param[in]	aux_table	auxiliary table
@retval nonzero if index is corrupted, zero for valid index */
static
ulint
fts_check_corrupt_index(
	fts_aux_table_t*	aux_table)
{
	dict_table_t*	table;
	dict_index_t*	index;
	table = dict_table_open_on_id(
		aux_table->parent_id, TRUE, DICT_TABLE_OP_NORMAL);

	if (table == NULL) {
		return(0);
	}

	for (index = UT_LIST_GET_FIRST(table->indexes);
	     index;
	     index = UT_LIST_GET_NEXT(indexes, index)) {
		if (index->id == aux_table->index_id) {
			ut_ad(index->type & DICT_FTS);
			dict_table_close(table, true, false);
			return(dict_index_is_corrupted(index));
		}
	}

	dict_table_close(table, true, false);
	return(0);
}

/* Get parent table name if it's a fts aux table
@param[in]	aux_table_name	aux table name
@param[in]	aux_table_len	aux table length
@return parent table name, or NULL */
char*
fts_get_parent_table_name(
	const char*	aux_table_name,
	ulint		aux_table_len)
{
	fts_aux_table_t	aux_table;
	char*		parent_table_name = NULL;

	if (fts_is_aux_table_name(&aux_table, aux_table_name, aux_table_len)) {
		dict_table_t*	parent_table;

		parent_table = dict_table_open_on_id(
			aux_table.parent_id, TRUE, DICT_TABLE_OP_NORMAL);

		if (parent_table != NULL) {
			parent_table_name = mem_strdupl(
				parent_table->name.m_name,
				strlen(parent_table->name.m_name));

			dict_table_close(parent_table, TRUE, FALSE);
		}
	}

	return(parent_table_name);
}

/** Check the validity of the parent table.
@param[in]	aux_table	auxiliary table
@return true if it is a valid table or false if it is not */
static
bool
fts_valid_parent_table(
	const fts_aux_table_t*	aux_table)
{
	dict_table_t*	parent_table;
	bool		valid = false;

	parent_table = dict_table_open_on_id(
		aux_table->parent_id, TRUE, DICT_TABLE_OP_NORMAL);

	if (parent_table != NULL && parent_table->fts != NULL) {
		if (aux_table->index_id == 0) {
			valid = true;
		} else {
			index_id_t	id = aux_table->index_id;
			dict_index_t*	index;

			/* Search for the FT index in the table's list. */
			for (index = UT_LIST_GET_FIRST(parent_table->indexes);
			     index;
			     index = UT_LIST_GET_NEXT(indexes, index)) {
				if (index->id == id) {
					valid = true;
					break;
				}

			}
		}
	}

	if (parent_table) {
		dict_table_close(parent_table, TRUE, FALSE);
	}

	return(valid);
}

/** Try to rename all aux tables of the specified parent table.
@param[in]	aux_tables	aux_tables to be renamed
@param[in]	parent_table	parent table of all aux
				tables stored in tables. */
static
void
fts_rename_aux_tables_to_hex_format(
	ib_vector_t*	aux_tables,
	dict_table_t*	parent_table)
{
	dberr_t err;
	trx_t*	trx_rename = trx_allocate_for_background();
	trx_rename->op_info = "Rename aux tables to hex format";
	trx_rename->dict_operation_lock_mode = RW_X_LATCH;
	trx_start_for_ddl(trx_rename, TRX_DICT_OP_TABLE);

	err = fts_rename_aux_tables_to_hex_format_low(trx_rename,
						      parent_table, aux_tables);

	trx_rename->dict_operation_lock_mode = 0;

	if (err != DB_SUCCESS) {

		ib::warn() << "Rollback operations on all aux tables of "
			"table "<< parent_table->name << ". All the fts index "
			"associated with the table are marked as corrupted. "
			"Please rebuild the index again.";

		/* Corrupting the fts index related to parent table. */
		trx_t*	trx_corrupt;
		trx_corrupt = trx_allocate_for_background();
		trx_corrupt->dict_operation_lock_mode = RW_X_LATCH;
		trx_start_for_ddl(trx_corrupt, TRX_DICT_OP_TABLE);
		fts_parent_all_index_set_corrupt(trx_corrupt, parent_table);
		trx_corrupt->dict_operation_lock_mode = 0;
		fts_sql_commit(trx_corrupt);
		trx_free_for_background(trx_corrupt);
	} else {
		fts_sql_commit(trx_rename);
	}

	trx_free_for_background(trx_rename);
	ib_vector_reset(aux_tables);
}

/** Set the hex format flag for the parent table.
@param[in, out]	parent_table	parent table
@param[in]	trx		transaction */
static
void
fts_set_parent_hex_format_flag(
	dict_table_t*	parent_table,
	trx_t*		trx)
{
	if (!DICT_TF2_FLAG_IS_SET(parent_table,
				  DICT_TF2_FTS_AUX_HEX_NAME)) {
		DBUG_EXECUTE_IF("parent_table_flag_fail", DBUG_SUICIDE(););

		dberr_t	err = fts_update_hex_format_flag(
				trx, parent_table->id, true);

		if (err != DB_SUCCESS) {
			ib::fatal() << "Setting parent table "
				<< parent_table->name
				<< "to hex format failed. Please try "
				<< "to restart the server again, if it "
				<< "doesn't work, the system tables "
				<< "might be corrupted.";
		} else {
			DICT_TF2_FLAG_SET(
				parent_table, DICT_TF2_FTS_AUX_HEX_NAME);
		}
	}
}

/** Drop the obsolete auxilary table.
@param[in]	tables	tables to be dropped. */
static
void
fts_drop_obsolete_aux_table_from_vector(
	ib_vector_t*	tables)
{
	dberr_t		err;

	for (ulint count = 0; count < ib_vector_size(tables);
	     ++count) {

		fts_aux_table_t*	aux_drop_table;
		aux_drop_table = static_cast<fts_aux_table_t*>(
			ib_vector_get(tables, count));
		trx_t*	trx_drop = trx_allocate_for_background();
		trx_drop->op_info = "Drop obsolete aux tables";
		trx_drop->dict_operation_lock_mode = RW_X_LATCH;
		trx_start_for_ddl(trx_drop, TRX_DICT_OP_TABLE);

		err = row_drop_table_for_mysql(
			aux_drop_table->name, trx_drop, false, true);

		trx_drop->dict_operation_lock_mode = 0;

		if (err != DB_SUCCESS) {
			/* We don't need to worry about the
			failure, since server would try to
			drop it on next restart, even if
			the table was broken. */
			ib::warn() << "Failed to drop obsolete aux table "
				<< aux_drop_table->name << ", which is "
				<< "harmless. will try to drop it on next "
				<< "restart.";

			fts_sql_rollback(trx_drop);
		} else {
			ib::info() << "Dropped obsolete aux"
				" table '" << aux_drop_table->name
				<< "'.";

			fts_sql_commit(trx_drop);
		}

		trx_free_for_background(trx_drop);
	}
}

/** Drop all the auxiliary table present in the vector.
@param[in]	trx	transaction
@param[in]	tables	tables to be dropped */
static
void
fts_drop_aux_table_from_vector(
	trx_t*		trx,
	ib_vector_t*	tables)
{
	for (ulint count = 0; count < ib_vector_size(tables);
	    ++count) {
		fts_aux_table_t*	aux_drop_table;
		aux_drop_table = static_cast<fts_aux_table_t*>(
				ib_vector_get(tables, count));

		/* Check for the validity of the parent table */
		if (!fts_valid_parent_table(aux_drop_table)) {

			ib::warn() << "Parent table of FTS auxiliary table "
				<< aux_drop_table->name << " not found.";

			dberr_t err = fts_drop_table(trx, aux_drop_table->name);
			if (err == DB_FAIL) {

				char*	path = fil_make_filepath(
					NULL, aux_drop_table->name, IBD, false);

				if (path != NULL) {
					os_file_delete_if_exists(
							innodb_data_file_key,
							path , NULL);
					ut_free(path);
				}
			}
		}
	}
}

/**********************************************************************//**
Check and drop all orphaned FTS auxiliary tables, those that don't have
a parent table or FTS index defined on them.
@return DB_SUCCESS or error code */
static MY_ATTRIBUTE((nonnull))
void
fts_check_and_drop_orphaned_tables(
/*===============================*/
	trx_t*		trx,			/*!< in: transaction */
	ib_vector_t*	tables)			/*!< in: tables to check */
{
	mem_heap_t*	heap;
	ib_vector_t*	aux_tables_to_rename;
	ib_vector_t*	invalid_aux_tables;
	ib_vector_t*	valid_aux_tables;
	ib_vector_t*	drop_aux_tables;
	ib_vector_t*	obsolete_aux_tables;
	ib_alloc_t*	heap_alloc;

	heap = mem_heap_create(1024);
	heap_alloc = ib_heap_allocator_create(heap);

	/* We store all aux tables belonging to the same parent table here,
	and rename all these tables in a batch mode. */
	aux_tables_to_rename = ib_vector_create(heap_alloc,
						sizeof(fts_aux_table_t), 128);

	/* We store all fake auxiliary table and orphaned table here. */
	invalid_aux_tables = ib_vector_create(heap_alloc,
					      sizeof(fts_aux_table_t), 128);

	/* We store all valid aux tables. We use this to filter the
	fake auxiliary table from invalid auxiliary tables. */
	valid_aux_tables = ib_vector_create(heap_alloc,
					    sizeof(fts_aux_table_t), 128);

	/* We store all auxiliary tables to be dropped. */
	drop_aux_tables = ib_vector_create(heap_alloc,
					   sizeof(fts_aux_table_t), 128);

	/* We store all obsolete auxiliary tables to be dropped. */
	obsolete_aux_tables = ib_vector_create(heap_alloc,
					       sizeof(fts_aux_table_t), 128);

	/* Sort by parent_id first, in case rename will fail */
	ib_vector_sort(tables, fts_check_aux_table_parent_id_cmp);

	for (ulint i = 0; i < ib_vector_size(tables); ++i) {
		dict_table_t*		parent_table;
		fts_aux_table_t*	aux_table;
		bool			drop = false;
		dict_table_t*		table;
		fts_aux_table_t*	next_aux_table = NULL;
		ib_id_t			orig_parent_id = 0;
		ib_id_t			orig_index_id = 0;
		bool			rename = false;

		aux_table = static_cast<fts_aux_table_t*>(
			ib_vector_get(tables, i));

		table = dict_table_open_on_id(
			aux_table->id, TRUE, DICT_TABLE_OP_NORMAL);
		orig_parent_id = aux_table->parent_id;
		orig_index_id = aux_table->index_id;

		if (table == NULL
		    || strcmp(table->name.m_name, aux_table->name)) {

			bool	fake_aux = false;

			if (table != NULL) {
				dict_table_close(table, TRUE, FALSE);
			}

			if (i + 1 < ib_vector_size(tables)) {
				next_aux_table = static_cast<fts_aux_table_t*>(
						ib_vector_get(tables, i + 1));
			}

			/* To know whether aux table is fake fts or
			orphan fts table. */
			for (ulint count = 0;
			     count < ib_vector_size(valid_aux_tables);
			     count++) {
				fts_aux_table_t*	valid_aux;
				valid_aux = static_cast<fts_aux_table_t*>(
					ib_vector_get(valid_aux_tables, count));
				if (strcmp(valid_aux->name,
					   aux_table->name) == 0) {
					fake_aux = true;
					break;
				}
			}

			/* All aux tables of parent table, whose id is
			last_parent_id, have been checked, try to rename
			them if necessary. */
			if ((next_aux_table == NULL
			     || orig_parent_id != next_aux_table->parent_id)
			    && (!ib_vector_is_empty(aux_tables_to_rename))) {

					ib_id_t	parent_id = fts_fake_hex_to_dec(
							aux_table->parent_id);

					parent_table = dict_table_open_on_id(
						parent_id, TRUE,
						DICT_TABLE_OP_NORMAL);

					fts_rename_aux_tables_to_hex_format(
						aux_tables_to_rename, parent_table);

					dict_table_close(parent_table, TRUE,
							 FALSE);
			}

			/* If the aux table is fake aux table. Skip it. */
			if (!fake_aux) {
				ib_vector_push(invalid_aux_tables, aux_table);
			}

			continue;
		} else if (!DICT_TF2_FLAG_IS_SET(table,
						 DICT_TF2_FTS_AUX_HEX_NAME)) {

			aux_table->parent_id = fts_fake_hex_to_dec(
						aux_table->parent_id);

			if (aux_table->index_id != 0) {
				aux_table->index_id = fts_fake_hex_to_dec(
							aux_table->index_id);
			}

			ut_ad(aux_table->id > aux_table->parent_id);

			/* Check whether parent table id and index id
			are stored as decimal format. */
			if (fts_valid_parent_table(aux_table)) {

				parent_table = dict_table_open_on_id(
					aux_table->parent_id, true,
					DICT_TABLE_OP_NORMAL);

				ut_ad(parent_table != NULL);
				ut_ad(parent_table->fts != NULL);

				if (!DICT_TF2_FLAG_IS_SET(
					parent_table,
					DICT_TF2_FTS_AUX_HEX_NAME)) {
					rename = true;
				}

				dict_table_close(parent_table, TRUE, FALSE);
			}

			if (!rename) {
				/* Reassign the original value of
				aux table if it is not in decimal format */
				aux_table->parent_id = orig_parent_id;
				aux_table->index_id = orig_index_id;
			}
		}

		if (table != NULL) {
			dict_table_close(table, TRUE, FALSE);
		}

		if (!rename) {
			/* Check the validity of the parent table. */
			if (!fts_valid_parent_table(aux_table)) {
				drop = true;
			}
		}

		/* Filter out the fake aux table by comparing with the
		current valid auxiliary table name. */
		for (ulint count = 0;
		     count < ib_vector_size(invalid_aux_tables); count++) {
			fts_aux_table_t*	invalid_aux;
			invalid_aux = static_cast<fts_aux_table_t*>(
				ib_vector_get(invalid_aux_tables, count));
			if (strcmp(invalid_aux->name, aux_table->name) == 0) {
				ib_vector_remove(
					invalid_aux_tables,
					*reinterpret_cast<void**>(invalid_aux));
				break;
			}
		}

		ib_vector_push(valid_aux_tables, aux_table);

		/* If the index associated with aux table is corrupted,
		skip it. */
		if (fts_check_corrupt_index(aux_table) > 0) {

			if (i + 1 < ib_vector_size(tables)) {
				next_aux_table = static_cast<fts_aux_table_t*>(
					ib_vector_get(tables, i + 1));
			}

			if (next_aux_table == NULL
			    || orig_parent_id != next_aux_table->parent_id) {

				parent_table = dict_table_open_on_id(
					aux_table->parent_id, TRUE,
					DICT_TABLE_OP_NORMAL);

				if (!ib_vector_is_empty(aux_tables_to_rename)) {
					fts_rename_aux_tables_to_hex_format(
						aux_tables_to_rename, parent_table);
				} else {
					fts_set_parent_hex_format_flag(
						parent_table, trx);
				}

				dict_table_close(parent_table, TRUE, FALSE);
			}

			continue;
		}

		parent_table = dict_table_open_on_id(
			aux_table->parent_id, TRUE, DICT_TABLE_OP_NORMAL);

		if (drop) {
			ib_vector_push(drop_aux_tables, aux_table);
		} else {
			if (FTS_IS_OBSOLETE_AUX_TABLE(aux_table->name)) {
				ib_vector_push(obsolete_aux_tables, aux_table);
				continue;
			}
		}

		/* If the aux table is in decimal format, we should
		rename it, so push it to aux_tables_to_rename */
		if (!drop && rename) {
			bool	rename_table = true;
			for (ulint count = 0;
			     count < ib_vector_size(aux_tables_to_rename);
			     count++) {
				fts_aux_table_t*	rename_aux =
					static_cast<fts_aux_table_t*>(
					ib_vector_get(aux_tables_to_rename,
						      count));
					if (strcmp(rename_aux->name,
						   aux_table->name) == 0) {
						rename_table = false;
						break;
					}
			}

			if (rename_table) {
				ib_vector_push(aux_tables_to_rename,
					       aux_table);
			}
		}

		if (i + 1 < ib_vector_size(tables)) {
			next_aux_table = static_cast<fts_aux_table_t*>(
				ib_vector_get(tables, i + 1));
		}

		if ((next_aux_table == NULL
		     || orig_parent_id != next_aux_table->parent_id)
		    && !ib_vector_is_empty(aux_tables_to_rename)) {

			ut_ad(rename);
			ut_ad(!DICT_TF2_FLAG_IS_SET(
				parent_table, DICT_TF2_FTS_AUX_HEX_NAME));

			fts_rename_aux_tables_to_hex_format(
				aux_tables_to_rename,parent_table);
		}

		/* The IDs are already in correct hex format. */
		if (!drop && !rename) {
			dict_table_t*	table;

			table = dict_table_open_on_id(
				aux_table->id, TRUE, DICT_TABLE_OP_NORMAL);

			if (table != NULL
			    && strcmp(table->name.m_name, aux_table->name)) {
				dict_table_close(table, TRUE, FALSE);
				table = NULL;
			}

			if (table != NULL
			    && !DICT_TF2_FLAG_IS_SET(
					table,
					DICT_TF2_FTS_AUX_HEX_NAME)) {

				DBUG_EXECUTE_IF("aux_table_flag_fail",
					ib::warn() << "Setting aux table "
						<< table->name << " to hex "
						"format failed.";
					fts_set_index_corrupt(
						trx, aux_table->index_id,
						parent_table);
						goto table_exit;);

				dberr_t err = fts_update_hex_format_flag(
						trx, table->id, true);

				if (err != DB_SUCCESS) {
					ib::warn() << "Setting aux table "
						<< table->name << " to hex "
						"format failed.";

					fts_set_index_corrupt(
						trx, aux_table->index_id,
						parent_table);
				} else {
					DICT_TF2_FLAG_SET(table,
						DICT_TF2_FTS_AUX_HEX_NAME);
				}
			}
#ifndef DBUG_OFF
table_exit:
#endif	/* !DBUG_OFF */

			if (table != NULL) {
				dict_table_close(table, TRUE, FALSE);
			}

			ut_ad(parent_table != NULL);

			fts_set_parent_hex_format_flag(
				parent_table, trx);
		}

		if (parent_table != NULL) {
			dict_table_close(parent_table, TRUE, FALSE);
		}
	}

	fts_drop_aux_table_from_vector(trx, invalid_aux_tables);
	fts_drop_aux_table_from_vector(trx, drop_aux_tables);
	fts_sql_commit(trx);

	fts_drop_obsolete_aux_table_from_vector(obsolete_aux_tables);

	/* Free the memory allocated at the beginning */
	if (heap != NULL) {
		mem_heap_free(heap);
	}
}

/**********************************************************************//**
Drop all orphaned FTS auxiliary tables, those that don't have a parent
table or FTS index defined on them. */
void
fts_drop_orphaned_tables(void)
/*==========================*/
{
	trx_t*			trx;
	pars_info_t*		info;
	mem_heap_t*		heap;
	que_t*			graph;
	ib_vector_t*		tables;
	ib_alloc_t*		heap_alloc;
	space_name_list_t	space_name_list;
	dberr_t			error = DB_SUCCESS;

	/* Note: We have to free the memory after we are done with the list. */
	error = fil_get_space_names(space_name_list);

	if (error == DB_OUT_OF_MEMORY) {
		ib::fatal() << "Out of memory";
	}

	heap = mem_heap_create(1024);
	heap_alloc = ib_heap_allocator_create(heap);

	/* We store the table ids of all the FTS indexes that were found. */
	tables = ib_vector_create(heap_alloc, sizeof(fts_aux_table_t), 128);

	/* Get the list of all known .ibd files and check for orphaned
	FTS auxiliary files in that list. We need to remove them because
	users can't map them back to table names and this will create
	unnecessary clutter. */

	for (space_name_list_t::iterator it = space_name_list.begin();
	     it != space_name_list.end();
	     ++it) {

		fts_aux_table_t*	fts_aux_table;

		fts_aux_table = static_cast<fts_aux_table_t*>(
			ib_vector_push(tables, NULL));

		memset(fts_aux_table, 0x0, sizeof(*fts_aux_table));

		if (!fts_is_aux_table_name(fts_aux_table, *it, strlen(*it))) {
			ib_vector_pop(tables);
		} else {
			ulint	len = strlen(*it);

			fts_aux_table->id = fil_space_get_id_by_name(*it);

			/* We got this list from fil0fil.cc. The tablespace
			with this name must exist. */
			ut_a(fts_aux_table->id != ULINT_UNDEFINED);

			fts_aux_table->name = static_cast<char*>(
				mem_heap_dup(heap, *it, len + 1));

			fts_aux_table->name[len] = 0;
		}
	}

	trx = trx_allocate_for_background();
	trx->op_info = "dropping orphaned FTS tables";
	row_mysql_lock_data_dictionary(trx);

	info = pars_info_create();

	pars_info_bind_function(info, "my_func", fts_read_tables, tables);

	graph = fts_parse_sql_no_dict_lock(
		NULL,
		info,
		"DECLARE FUNCTION my_func;\n"
		"DECLARE CURSOR c IS"
		" SELECT NAME, ID"
		" FROM SYS_TABLES;\n"
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
			fts_check_and_drop_orphaned_tables(trx, tables);
			break;				/* Exit the loop. */
		} else {
			ib_vector_reset(tables);

			fts_sql_rollback(trx);

			if (error == DB_LOCK_WAIT_TIMEOUT) {
				ib::warn() << "lock wait timeout reading"
					" SYS_TABLES. Retrying!";

				trx->error_state = DB_SUCCESS;
			} else {
				ib::error() << "(" << ut_strerr(error)
					<< ") while reading SYS_TABLES.";

				break;			/* Exit the loop. */
			}
		}
	}

	que_graph_free(graph);

	row_mysql_unlock_data_dictionary(trx);

	trx_free_for_background(trx);

	if (heap != NULL) {
		mem_heap_free(heap);
	}

	/** Free the memory allocated to store the .ibd names. */
	for (space_name_list_t::iterator it = space_name_list.begin();
	     it != space_name_list.end();
	     ++it) {

		UT_DELETE_ARRAY(*it);
	}
}

/**********************************************************************//**
Check whether user supplied stopword table is of the right format.
Caller is responsible to hold dictionary locks.
@return the stopword column charset if qualifies */
CHARSET_INFO*
fts_valid_stopword_table(
/*=====================*/
	const char*	stopword_table_name)	/*!< in: Stopword table
						name */
{
	dict_table_t*	table;
	dict_col_t*     col = NULL;

	if (!stopword_table_name) {
		return(NULL);
	}

	table = dict_table_get_low(stopword_table_name);

	if (!table) {
		ib::error() << "User stopword table " << stopword_table_name
			<< " does not exist.";

		return(NULL);
	} else {
		const char*     col_name;

		col_name = dict_table_get_col_name(table, 0);

		if (ut_strcmp(col_name, "value")) {
			ib::error() << "Invalid column name for stopword"
				" table " << stopword_table_name << ". Its"
				" first column must be named as 'value'.";

			return(NULL);
		}

		col = dict_table_get_nth_col(table, 0);

		if (col->mtype != DATA_VARCHAR
		    && col->mtype != DATA_VARMYSQL) {
			ib::error() << "Invalid column type for stopword"
				" table " << stopword_table_name << ". Its"
				" first column must be of varchar type";

			return(NULL);
		}
	}

	ut_ad(col);

	return(fts_get_charset(col->prtype));
}

/**********************************************************************//**
This function loads the stopword into the FTS cache. It also
records/fetches stopword configuration to/from FTS configure
table, depending on whether we are creating or reloading the
FTS.
@return TRUE if load operation is successful */
ibool
fts_load_stopword(
/*==============*/
	const dict_table_t*
			table,			/*!< in: Table with FTS */
	trx_t*		trx,			/*!< in: Transactions */
	const char*	global_stopword_table,	/*!< in: Global stopword table
						name */
	const char*	session_stopword_table,	/*!< in: Session stopword table
						name */
	ibool		stopword_is_on,		/*!< in: Whether stopword
						option is turned on/off */
	ibool		reload)			/*!< in: Whether it is
						for reloading FTS table */
{
	fts_table_t	fts_table;
	fts_string_t	str;
	dberr_t		error = DB_SUCCESS;
	ulint		use_stopword;
	fts_cache_t*	cache;
	const char*	stopword_to_use = NULL;
	ibool		new_trx = FALSE;
	byte		str_buffer[MAX_FULL_NAME_LEN + 1];

	FTS_INIT_FTS_TABLE(&fts_table, "CONFIG", FTS_COMMON_TABLE, table);

	cache = table->fts->cache;

	if (!reload && !(cache->stopword_info.status
			 & STOPWORD_NOT_INIT)) {
		return(TRUE);
	}

	if (!trx) {
		trx = trx_allocate_for_background();
		trx->op_info = "upload FTS stopword";
		new_trx = TRUE;
	}

	/* First check whether stopword filtering is turned off */
	if (reload) {
		error = fts_config_get_ulint(
			trx, &fts_table, FTS_USE_STOPWORD, &use_stopword);
	} else {
		use_stopword = (ulint) stopword_is_on;

		error = fts_config_set_ulint(
			trx, &fts_table, FTS_USE_STOPWORD, use_stopword);
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

		error = fts_config_get_value(
			trx, &fts_table, FTS_STOPWORD_TABLE_NAME, &str);

		if (error != DB_SUCCESS) {
			goto cleanup;
		}

		if (strlen((char*) str.f_str) > 0) {
			stopword_to_use = (const char*) str.f_str;
		}
	} else {
		stopword_to_use = (session_stopword_table)
			? session_stopword_table : global_stopword_table;
	}

	if (stopword_to_use
	    && fts_load_user_stopword(table->fts, stopword_to_use,
				      &cache->stopword_info)) {
		/* Save the stopword table name to the configure
		table */
		if (!reload) {
			str.f_n_char = 0;
			str.f_str = (byte*) stopword_to_use;
			str.f_len = ut_strlen(stopword_to_use);

			error = fts_config_set_value(
				trx, &fts_table, FTS_STOPWORD_TABLE_NAME, &str);
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
		cache->stopword_info.cached_stopword = rbt_create_arg_cmp(
			sizeof(fts_tokenizer_word_t), innobase_fts_text_cmp,
			&my_charset_latin1);
	}

	return(error == DB_SUCCESS);
}

/**********************************************************************//**
Callback function when we initialize the FTS at the start up
time. It recovers the maximum Doc IDs presented in the current table.
@return: always returns TRUE */
static
ibool
fts_init_get_doc_id(
/*================*/
	void*	row,			/*!< in: sel_node_t* */
	void*	user_arg)		/*!< in: fts cache */
{
	doc_id_t	doc_id = FTS_NULL_DOC_ID;
	sel_node_t*	node = static_cast<sel_node_t*>(row);
	que_node_t*	exp = node->select_list;
	fts_cache_t*    cache = static_cast<fts_cache_t*>(user_arg);

	ut_ad(ib_vector_is_empty(cache->get_docs));

	/* Copy each indexed column content into doc->text.f_str */
	if (exp) {
		dfield_t*	dfield = que_node_get_val(exp);
		dtype_t*        type = dfield_get_type(dfield);
		void*           data = dfield_get_data(dfield);

		ut_a(dtype_get_mtype(type) == DATA_INT);

		doc_id = static_cast<doc_id_t>(mach_read_from_8(
			static_cast<const byte*>(data)));

		if (doc_id >= cache->next_doc_id) {
			cache->next_doc_id = doc_id + 1;
		}
	}

	return(TRUE);
}

/**********************************************************************//**
Callback function when we initialize the FTS at the start up
time. It recovers Doc IDs that have not sync-ed to the auxiliary
table, and require to bring them back into FTS index.
@return: always returns TRUE */
static
ibool
fts_init_recover_doc(
/*=================*/
	void*	row,			/*!< in: sel_node_t* */
	void*	user_arg)		/*!< in: fts cache */
{

	fts_doc_t       doc;
	ulint		doc_len = 0;
	ulint		field_no = 0;
	fts_get_doc_t*  get_doc = static_cast<fts_get_doc_t*>(user_arg);
	doc_id_t	doc_id = FTS_NULL_DOC_ID;
	sel_node_t*	node = static_cast<sel_node_t*>(row);
	que_node_t*	exp = node->select_list;
	fts_cache_t*	cache = get_doc->cache;
	st_mysql_ftparser*	parser = get_doc->index_cache->index->parser;

	fts_doc_init(&doc);
	doc.found = TRUE;

	ut_ad(cache);

	/* Copy each indexed column content into doc->text.f_str */
	while (exp) {
		dfield_t*	dfield = que_node_get_val(exp);
		ulint		len = dfield_get_len(dfield);

		if (field_no == 0) {
			dtype_t*        type = dfield_get_type(dfield);
			void*           data = dfield_get_data(dfield);

			ut_a(dtype_get_mtype(type) == DATA_INT);

			doc_id = static_cast<doc_id_t>(mach_read_from_8(
				static_cast<const byte*>(data)));

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
			get_doc->index_cache->charset = fts_get_charset(
				dfield->type.prtype);
		}

		doc.charset = get_doc->index_cache->charset;
		doc.is_ngram = get_doc->index_cache->index->is_ngram;

		if (dfield_is_ext(dfield)) {
			dict_table_t*	table = cache->sync->table;

			doc.text.f_str = btr_copy_externally_stored_field(
				&doc.text.f_len,
				static_cast<byte*>(dfield_get_data(dfield)),
				dict_table_page_size(table), len,
				static_cast<mem_heap_t*>(doc.self_heap->arg));
		} else {
			doc.text.f_str = static_cast<byte*>(
				dfield_get_data(dfield));

			doc.text.f_len = len;
		}

		if (field_no == 1) {
			fts_tokenize_document(&doc, NULL, parser);
		} else {
			fts_tokenize_document_next(&doc, doc_len, NULL, parser);
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

	return(TRUE);
}

/**********************************************************************//**
This function brings FTS index in sync when FTS index is first
used. There are documents that have not yet sync-ed to auxiliary
tables from last server abnormally shutdown, we will need to bring
such document into FTS cache before any further operations
@return TRUE if all OK */
ibool
fts_init_index(
/*===========*/
	dict_table_t*	table,		/*!< in: Table with FTS */
	ibool		has_cache_lock)	/*!< in: Whether we already have
					cache lock */
{
	dict_index_t*   index;
	doc_id_t        start_doc;
	fts_get_doc_t*  get_doc = NULL;
	fts_cache_t*    cache = table->fts->cache;
	bool		need_init = false;

	ut_ad(!mutex_own(&dict_sys->mutex));

	/* First check cache->get_docs is initialized */
	if (!has_cache_lock) {
		rw_lock_x_lock(&cache->lock);
	}

	rw_lock_x_lock(&cache->init_lock);
	if (cache->get_docs == NULL) {
		cache->get_docs = fts_get_docs_create(cache);
	}
	rw_lock_x_unlock(&cache->init_lock);

	if (table->fts->fts_status & ADDED_TABLE_SYNCED) {
		goto func_exit;
	}

	need_init = true;

	start_doc = cache->synced_doc_id;

	if (!start_doc) {
		fts_cmp_set_sync_doc_id(table, 0, TRUE, &start_doc);
		cache->synced_doc_id = start_doc;
	}

	/* No FTS index, this is the case when previous FTS index
	dropped, and we re-initialize the Doc ID system for subsequent
	insertion */
	if (ib_vector_is_empty(cache->get_docs)) {
		index = table->fts_doc_id_index;

		ut_a(index);

		fts_doc_fetch_by_doc_id(NULL, start_doc, index,
					FTS_FETCH_DOC_BY_ID_LARGE,
					fts_init_get_doc_id, cache);
	} else {
		if (table->fts->cache->stopword_info.status
		    & STOPWORD_NOT_INIT) {
			fts_load_stopword(table, NULL, NULL, NULL, TRUE, TRUE);
		}

		for (ulint i = 0; i < ib_vector_size(cache->get_docs); ++i) {
			get_doc = static_cast<fts_get_doc_t*>(
				ib_vector_get(cache->get_docs, i));

			index = get_doc->index_cache->index;

			fts_doc_fetch_by_doc_id(NULL, start_doc, index,
						FTS_FETCH_DOC_BY_ID_LARGE,
						fts_init_recover_doc, get_doc);
		}
	}

	table->fts->fts_status |= ADDED_TABLE_SYNCED;

	fts_get_docs_clear(cache->get_docs);

func_exit:
	if (!has_cache_lock) {
		rw_lock_x_unlock(&cache->lock);
	}

	if (need_init) {
		mutex_enter(&dict_sys->mutex);
		/* Register the table with the optimize thread. */
		fts_optimize_add_table(table);
		mutex_exit(&dict_sys->mutex);
	}

	return(TRUE);
}

/** Check if the all the auxillary tables associated with FTS index are in
consistent state. For now consistency is check only by ensuring
index->page_no != FIL_NULL
@param[out]	base_table	table has host fts index
@param[in,out]	trx		trx handler */
void
fts_check_corrupt(
	dict_table_t*	base_table,
	trx_t*		trx)
{
	bool		sane = true;
	fts_table_t	fts_table;

	/* Iterate over the common table and check for their sanity. */
	FTS_INIT_FTS_TABLE(&fts_table, NULL, FTS_COMMON_TABLE, base_table);

	for (ulint i = 0; fts_common_tables[i] != NULL && sane; ++i) {

		char	table_name[MAX_FULL_NAME_LEN];

		fts_table.suffix = fts_common_tables[i];
		fts_get_table_name(&fts_table, table_name);

		dict_table_t*	aux_table = dict_table_open_on_name(
			table_name, true, FALSE, DICT_ERR_IGNORE_NONE);

		if (aux_table == NULL) {
			dict_set_corrupted(
				dict_table_get_first_index(base_table),
				trx, "FTS_SANITY_CHECK");
			ut_ad(base_table->corrupted == TRUE);
			sane = false;
			continue;
		}

		for (dict_index_t*	aux_table_index =
			UT_LIST_GET_FIRST(aux_table->indexes);
		     aux_table_index != NULL;
		     aux_table_index =
			UT_LIST_GET_NEXT(indexes, aux_table_index)) {

			/* Check if auxillary table needed for FTS is sane. */
			if (aux_table_index->page == FIL_NULL) {
				dict_set_corrupted(
					dict_table_get_first_index(base_table),
					trx, "FTS_SANITY_CHECK");
				ut_ad(base_table->corrupted == TRUE);
				sane = false;
			}
		}

		dict_table_close(aux_table, FALSE, FALSE);
	}
}
