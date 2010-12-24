/*****************************************************************************

Copyright (c) 2010,  Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA

*****************************************************************************/

/**************************************************//**
@file fut/fut0fut.c
Full Text Search implementation

Created 2/15/2006 Osku Salerma
Completed by Sunny Bains and Jimmy Yang
*******************************************************/

#include "trx0roll.h"
#include "row0mysql.h"
#include "row0upd.h"
#include "dict0types.h"
#include "row0sel.h"
#include "fut0fut.h"

#include "fts0fts.h"
#include "fts0priv.h"
#include "fts0types.h"

#include "fts0types.ic"
#include "fts0vlc.ic"

#ifdef UNIV_NONINL
#include "fut0fut.ic"
#endif

#define FTS_MAX_ID_LEN	32

/* Column name from the FTS config table */
#define FTS_MAX_CACHE_SIZE_IN_MB	"cache_size_in_mb"

/* This is maximum FTS cache for each table and would be
a configurable variable */
UNIV_INTERN ulint	fts_max_cache_size = 50000000;

// FIXME: testing
ib_time_t elapsed_time = 0;
ulint n_nodes = 0;

typedef struct fts_schema_struct fts_schema_t;
typedef struct fts_sys_table_struct fts_sys_table_t;

/* Error condition reported by fts_utf8_decode() */
const ulint UTF8_ERROR = 0xFFFFFFFF;

/* The minimum length of token that is supported */
static const ulint FTS_MIN_TOKEN_LENGTH = 0;

/* The number of doc ids to reserve */
static const ulint FTS_DOC_ID_STEP = 100;

/* The cache size permissible lower limit (1K) */
static const ulint FTS_CACHE_SIZE_LOWER_LIMIT_IN_MB = 1;

/* The cache size permissible upper limit (1G) */
static const ulint FTS_CACHE_SIZE_UPPER_LIMIT_IN_MB = 1024;

/* Signal an optimize when the number of added documents
exceeds this threshold. */
static const ulint FTS_OPTIMIZE_ADD_THRESHOLD = 100;

/* Signal an optimize when the number of deleted documents
exceeds this threshold. */
static const ulint FTS_OPTIMIZE_DEL_THRESHOLD = 100;

/* Time to sleep after DEADLOCK error before retrying operation. */
static const ulint FTS_DEADLOCK_RETRY_WAIT = 100000;

#ifdef UNIV_PFS_RWLOCK
UNIV_INTERN mysql_pfs_key_t	fts_cache_rw_lock_key;
#endif /* UNIV_PFS_RWLOCK */

#ifdef UNIV_PFS_MUTEX
UNIV_INTERN mysql_pfs_key_t	fts_delete_mutex_key;
UNIV_INTERN mysql_pfs_key_t	fts_optimize_mutex_key;
UNIV_INTERN mysql_pfs_key_t	fts_bg_threads_mutex_key;
#endif /* UNIV_PFS_MUTEX */

/** variable to record innodb_fts_internal_tbl_name for information
schema table INNODB_FTS_INSERTED etc. */
UNIV_INTERN char* fts_internal_tbl_name		= NULL;

/* InnoDB default stopword list:
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

/* For storing table info when checking for orphaned tables. */
struct fts_sys_table_struct {
	table_id_t	id;		/* Table id */
	table_id_t	parent_id;	/* Parent table id */
	table_id_t	index_id;	/* Table FT index id */
	char*		name;		/* Name of the table */
};

/* SQL statements for creating the ancillary common FTS tables. */
static const char* fts_create_common_tables_sql = {
	"BEGIN\n"
	""
	"CREATE TABLE %s_ADDED (\n"
	"  doc_id BIGINT UNSIGNED\n"
	") COMPACT;\n"
	"CREATE UNIQUE CLUSTERED INDEX IND ON %s_ADDED(doc_id);\n"
	""
	"CREATE TABLE %s_DELETED (\n"
	"  doc_id BIGINT UNSIGNED\n"
	") COMPACT;\n"
	"CREATE UNIQUE CLUSTERED INDEX IND ON %s_DELETED(doc_id);\n"
	""
	"CREATE TABLE %s_DELETED_CACHE (\n"
	"  doc_id BIGINT UNSIGNED\n"
	") COMPACT;\n"
	"CREATE UNIQUE CLUSTERED INDEX IND "
		"ON %s_DELETED_CACHE(doc_id);\n"
	""
	"CREATE TABLE %s_BEING_DELETED (\n"
	"  doc_id BIGINT UNSIGNED\n"
	") COMPACT;\n"
	"CREATE UNIQUE CLUSTERED INDEX IND "
		"ON %s_BEING_DELETED(doc_id);\n"
	""
	"CREATE TABLE %s_BEING_DELETED_CACHE (\n"
	"  doc_id BIGINT UNSIGNED\n"
	") COMPACT;\n"
	"CREATE UNIQUE CLUSTERED INDEX IND "
		"ON %s_BEING_DELETED_CACHE(doc_id);\n"
	""
	"CREATE TABLE %s_CONFIG (\n"
	"  key CHAR,\n"
	"  value CHAR NOT NULL\n"
	") COMPACT;\n"
	"CREATE UNIQUE CLUSTERED INDEX IND ON %s_CONFIG(key);\n"
	""
	"CREATE TABLE %s_STOPWORDS (\n"
	"  word CHAR\n"
	") COMPACT;\n"
	"CREATE UNIQUE CLUSTERED INDEX IND ON %s_STOPWORDS(word);\n",
};

/* Template for creating the FTS auxiliary index specific tables. */
static const char* fts_create_index_tables_sql = {
	"BEGIN\n"
	""
	"CREATE TABLE %s_DOC_ID (\n"
	"   doc_id BIGINT UNSIGNED,\n"
	"   word_count INTEGER UNSIGNED NOT NULL\n"
	") COMPACT;\n"
	"CREATE UNIQUE CLUSTERED INDEX IND ON %s_DOC_ID(doc_id);\n"
};

/* Template for creating the ancillary FTS tables word index tables. */
static const char* fts_create_index_sql = {
	"BEGIN\n"
	""
	"CREATE TABLE %s (\n"
	"   word CHAR,\n"
	"   first_doc_id BIGINT UNSIGNED NOT NULL,\n"
	"   last_doc_id BIGINT UNSIGNED NOT NULL,\n"
	"   doc_count INT UNSIGNED NOT NULL,\n"
	"   ilist BLOB NOT NULL\n"
	") COMPACT;\n"
	"CREATE UNIQUE CLUSTERED INDEX IND "
		"ON %s(word, first_doc_id);\n"
};

/* FTS auxiliary table suffixes that are common to all FT indexes. */
static const char* fts_common_tables[] = {
	"ADDED",
	"BEING_DELETED",
	"BEING_DELETED_CACHE",
	"CONFIG",
	"DELETED",
	"DELETED_CACHE",
	"STOPWORDS",
	NULL
};

// FIXME: Make this UTF-8 conformant
/* FTS auxiliary INDEX split intervals. */
const  fts_index_selector_t fts_index_selector[] = {
	{ '9', "INDEX_1" },
	{ 'a', "INDEX_2" },
	{ 'o', "INDEX_3" },
	{ 'z', "INDEX_4" },
	{  0 , NULL	 }
};

/* Default config values for FTS indexes on a table. */
static const char* fts_config_table_insert_values_sql =
	"BEGIN\n"
	"\n"
	"INSERT INTO %s VALUES('"
		FTS_MAX_CACHE_SIZE_IN_MB "', '256');\n"
	""
	"INSERT INTO %s VALUES('"
		FTS_OPTIMIZE_LIMIT_IN_SECS  "', '180');\n"
	""
	"INSERT INTO %s VALUES ('"
		FTS_NEXT_DOC_ID "', '1');\n"
	""
	"INSERT INTO %s VALUES ('"
		FTS_TOTAL_DELETED_COUNT "', '0');\n"
	"" /* Note: 0 == FTS_TABLE_STATE_RUNNING */
	"INSERT INTO %s VALUES ('"
		FTS_TABLE_STATE "', '0');\n";

/****************************************************************//**
Run SYNC on the table, i.e., write out data from the cache to the
FTS auxiliary INDEX table and clear the cache at the end.
@return DB_SUCCESS if all OK */
static
ulint
fts_sync(
/*=====*/
	fts_sync_t*	sync);		/*!< in: sync state */
/****************************************************************//**
Add the document with the given id to the table's cache, and run
SYNC if the cache grows too big. */
static
void
fts_add_doc(
/*========*/
	fts_get_doc_t*	get_doc,	/*!< in: state */
	doc_id_t	doc_id);	/*!< in: document id of document
					to add */
/****************************************************************//**
Read the max cache size parameter from the config table. */
static
void
fts_update_max_cache_size(
/*======================*/
	fts_sync_t*	sync);		/*!< in: sync state */
/****************************************************************//**
Check whether a particular word (term) exists in the FTS index.
@return DB_SUCCESS if all went fine */
static
ulint
fts_is_word_in_index(
/*=================*/
	trx_t*		trx,		/*!< in: FTS query state */
	que_t**		graph,		/*!< out: Query graph */
	fts_table_t*	fts_table,	/*!< in: table instance */
	const fts_string_t* word,	/*!< in: the word to check */
	ibool*		found);		/*!< out: TRUE if exists */
/********************************************************************
Check if we should stop. */
UNIV_INLINE
ibool
fts_is_stop_signalled(
/*==================*/
	fts_t*		fts)			/* in: fts instance */
{
	ibool		stop_signalled = FALSE;

	mutex_enter(&fts->bg_threads_mutex);

	if (fts->fts_status & BG_THREAD_STOP) {

		stop_signalled = TRUE;
	}

	mutex_exit(&fts->bg_threads_mutex);

	return(stop_signalled);
}

/****************************************************************//**
This function loads the default InnoDB stopword list */
static
void
fts_load_default_stopword(
/*======================*/
	fts_stopword_t*		stopword_info)	/*!< in: stopword info */
{
	ib_alloc_t*		allocator;
	fts_tokenizer_word_t	new_word;
	mem_heap_t*		heap;
	fts_string_t		str;
	ulint			ix = 0;
	ib_rbt_t*		stop_words;

	allocator = stopword_info->heap;
	heap = allocator->arg;
	stop_words = stopword_info->cached_stopword;

	while (fts_default_stopword[ix]) {
		new_word.nodes = ib_vector_create(allocator,
						  sizeof(fts_node_t), 4);

		str.utf8 = (byte*) fts_default_stopword[ix];
		str.len = ut_strlen((char*) fts_default_stopword[ix]);

		fts_utf8_string_dup(&new_word.text, &str, heap);

		rbt_insert(stop_words, &new_word, &new_word);

		ix++;
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
	void*		row,		/* in: sel_node_t* */
	void*		user_arg)	/* in: pointer to ib_vector_t */
{
	ib_alloc_t*		allocator;
	fts_stopword_t*		stopword_info = user_arg;
	sel_node_t*		sel_node = row;
	que_node_t*		exp;
	ib_rbt_t*		stop_words;
	dfield_t*		dfield;
	fts_string_t		str;
	fts_tokenizer_word_t	new_word;
	mem_heap_t*		heap;
	ib_rbt_bound_t		parent;

	allocator = stopword_info->heap;
	heap = allocator->arg;
	stop_words = stopword_info->cached_stopword;

	exp = sel_node->select_list;

	/* We only need to read the first column */
	dfield = que_node_get_val(exp);
	str.utf8 = dfield_get_data(dfield);
	str.len = dfield_get_len(dfield);

	/* Only create new node if it is a value not already existed */
	if (str.len != UNIV_SQL_NULL
	    && rbt_search(stop_words, &parent, &str) != 0) {
		new_word.nodes = ib_vector_create(allocator,
						  sizeof(fts_node_t), 4);

		fts_utf8_string_dup(&new_word.text, &str, heap);

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
	const char*	stopword_table_name,	/*!< in: Stopword table
						name */
	fts_stopword_t*	stopword_info)		/*!< in: Stopword info */
{
	pars_info_t*	info;
	que_t*		graph;
	ulint		error = DB_SUCCESS;
	ibool		ret = TRUE;
	trx_t*		trx;

	trx = trx_allocate_for_background();
	trx->op_info = "Load user stopword table into FTS cache";

	row_mysql_lock_data_dictionary(trx);

	/* Validate the user table existence and in the right
	format */
	if (!fts_valid_stopword_table(stopword_table_name)) {
		ret = FALSE;
		goto cleanup;
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
		" SELECT value "
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

			ut_print_timestamp(stderr);

			if (error == DB_LOCK_WAIT_TIMEOUT) {
				fprintf(stderr, "  InnoDB: Warning: lock wait "
					"timeout reading user stopword table. "
					"Retrying!\n");

				trx->error_state = DB_SUCCESS;
			} else {
				fprintf(stderr, "  InnoDB: Error: %lu "
					"while reading user stopword table.\n",
					error);
				ret = FALSE;
				break;
			}
		}
	}

	que_graph_free(graph);

cleanup:
	row_mysql_unlock_data_dictionary(trx);

	trx_free_for_background(trx);
	return(ret);
}
/********************************************************************
Initialize the index cache. */
static
void
fts_index_cache_init(
/*=================*/
	ib_alloc_t*		allocator,	/* in: the allocator to use */
	fts_index_cache_t*	index_cache)	/* in: index cache */
{
	ulint			i;

	ut_a(index_cache->words == NULL);

	index_cache->words = rbt_create(
		sizeof(fts_tokenizer_word_t), fts_utf8_string_cmp);

	ut_a(index_cache->doc_stats == NULL);

	index_cache->doc_stats = ib_vector_create(
		allocator, sizeof(fts_doc_stats_t), 4);

	for (i = 0; fts_index_selector[i].ch; ++i) {
		ut_a(index_cache->ins_graph[i] == NULL);
		ut_a(index_cache->sel_graph[i] == NULL);
	}
}

/********************************************************************
Initialize things in cache. */
static
void
fts_cache_init(
/*===========*/
	fts_cache_t*	cache)			/* in: cache */
{
	ulint		i;

	/* Just to make sure */
	ut_a(cache->sync_heap->arg == NULL);

	cache->sync_heap->arg = mem_heap_create(1024);

	cache->total_size = 0;

	cache->deleted_doc_ids = ib_vector_create(
		cache->sync_heap, sizeof(fts_update_t), 4);

	/* Reset the cache data for all the FTS indexes. */
	for (i = 0; i < ib_vector_size(cache->indexes); ++i) {
		fts_index_cache_t*	index_cache;

		index_cache = ib_vector_get(cache->indexes, i);

		fts_index_cache_init(cache->sync_heap, index_cache);
	}
}

/****************************************************************//**
Create a FTS cache. */
UNIV_INTERN
fts_cache_t*
fts_cache_create(
/*=============*/
	dict_table_t*		table)	/*!< table owns the FTS cache */
{
	mem_heap_t*	heap = table->heap;
	fts_cache_t*	cache = mem_heap_alloc(heap, sizeof(*cache));

	memset(cache, 0, sizeof(*cache));

	rw_lock_create(fts_cache_rw_lock_key, &cache->lock, SYNC_FTS_CACHE);

	mutex_create(fts_delete_mutex_key, &cache->deleted_lock,
		     SYNC_FTS_OPTIMIZE);
	mutex_create(fts_optimize_mutex_key, &cache->optimize_lock,
		     SYNC_FTS_OPTIMIZE);

	/* This is the heap used to create the cache itself. */
	cache->self_heap = ib_heap_allocator_create(heap);

	/* This is a transient heap, used for storing sync data. */
	cache->sync_heap = ib_heap_allocator_create(heap);
	cache->sync_heap->arg = NULL;
	cache->sync = (fts_sync_t*) mem_heap_alloc(heap, sizeof(fts_sync_t));
	memset(cache->sync, 0, sizeof(fts_sync_t));
	cache->sync->table = table;

	/* Create the index cache vector that will hold the inverted indexes. */
	cache->indexes = ib_vector_create(
		cache->self_heap, sizeof(fts_index_cache_t), 2);

	fts_cache_init(cache);

	/* Create stopword RB tree. The stopword tree will
	remain in cache for the duration of FTS cache's lifetime */
	cache->stopword_info.cached_stopword = rbt_create(
		sizeof(fts_tokenizer_word_t), fts_utf8_string_cmp);

	cache->stopword_info.heap = cache->self_heap;

	cache->stopword_info.status = STOPWORD_NOT_INIT;

	return(cache);
}

/****************************************************************//**
Create an FTS index cache. */
UNIV_INTERN
void
fts_cache_index_cache_create(
/*=========================*/
	dict_table_t*		table,		/* in: table with FTS index */
	dict_index_t*		index)		/* in: FTS index */
{
	ulint			n_bytes;
	fts_index_cache_t*	index_cache;
	fts_cache_t*		cache = table->fts->cache;

	ut_a(cache != NULL);

	rw_lock_x_lock(&cache->lock);

	/* Must not already exist in the cache vector. */
	ut_a(fts_find_index_cache(cache, index) == NULL);

	index_cache = ib_vector_push(cache->indexes, NULL);

	memset(index_cache, 0x0, sizeof(*index_cache));

	index_cache->index = index;

	n_bytes = sizeof(que_t*) * sizeof(fts_index_selector);

	index_cache->ins_graph = mem_heap_alloc(cache->self_heap->arg, n_bytes);
	index_cache->sel_graph = mem_heap_alloc(cache->self_heap->arg, n_bytes);

	memset(index_cache->ins_graph, 0x0, n_bytes);
	memset(index_cache->sel_graph, 0x0, n_bytes);

	fts_index_cache_init(cache->sync_heap, index_cache);

	rw_lock_x_unlock(&cache->lock);
}

/********************************************************************
Release all resoruces help by the words rb tree e.g., the node ilist. */
static
void
fts_words_free(
/*===========*/
	ib_rbt_t*	words)			/* in: rb tree of words */
{
	const ib_rbt_node_t*	rbt_node;

	/* Free the resources held by a word. */
	for (rbt_node = rbt_first(words);
	     rbt_node;
	     rbt_node = rbt_first(words)) {

		ulint			i;
		fts_tokenizer_word_t*	word;

		word = rbt_value(fts_tokenizer_word_t, rbt_node);

		/* Free the ilists of this word. */
		for (i = 0; i < ib_vector_size(word->nodes); ++i) {

			fts_node_t* fts_node = ib_vector_get(word->nodes, i);

			ut_free(fts_node->ilist);
			fts_node->ilist = NULL;
		}

		/* NOTE: We are responsible for free'ing the node */
		ut_free(rbt_remove_node(words, rbt_node));
	}
}

/********************************************************************
Clear cache. If the shutdown flag is TRUE then the cache can contain
data that needs to be freed. For regular clear as part of normal
working we assume the caller has freed all resources. */
static
void
fts_cache_clear(
/*============*/
	fts_cache_t*	cache,			/* in: cache */
	ibool		shutdown)		/* in: TRUE if shutdown of
						add thread. */
{
	ulint		i;

#ifdef UNIV_SYNC_DEBUG50
	ut_ad(rw_lock_own(&cache->lock, RW_LOCK_EX));
#endif

	for (i = 0; i < ib_vector_size(cache->indexes); ++i) {
		ulint			j;
		fts_index_cache_t*	index_cache;

		index_cache = ib_vector_get(cache->indexes, i);

		if (shutdown) {
			fts_words_free(index_cache->words);
		}

		ut_a(rbt_empty(index_cache->words));

		rbt_free(index_cache->words);

		index_cache->words = NULL;

		for (j = 0; fts_index_selector[j].ch; ++j) {

			if (index_cache->ins_graph[j] != NULL) {

				que_graph_free(index_cache->ins_graph[j]);
				index_cache->ins_graph[j] = NULL;
			}

			if (index_cache->sel_graph[j] != NULL) {

				que_graph_free(index_cache->sel_graph[j]);
				index_cache->sel_graph[j] = NULL;
			}
		}

		index_cache->doc_stats = NULL;
	}

	mem_heap_free(cache->sync_heap->arg);
	cache->sync_heap->arg = NULL;

	cache->total_size = 0;
	cache->deleted_doc_ids = NULL;
}

/********************************************************************
Search the index specific cache for a particular FTS index. */
UNIV_INLINE
fts_index_cache_t*
fts_get_index_cache(
/*================*/
						/* out: the index specific
						cache else NULL */
	fts_cache_t*		cache,		/* in: cache to search */
	const dict_index_t*	index)		/* in: index to search for */
{
	ulint			i;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(rw_lock_own((rw_lock_t*) &cache->lock, RW_LOCK_EX));
#endif

	for (i = 0; i < ib_vector_size(cache->indexes); ++i) {
		fts_index_cache_t*	index_cache;

		index_cache = ib_vector_get(cache->indexes, i);

		if (index_cache->index == index) {

			return(index_cache);
		}
	}

	return(NULL);
}

/********************************************************************
Sync the cache contensts and then free the cache. */
static
void
fts_cache_sync_and_free(
/*====================*/
	fts_cache_t*	cache)			/* in: cache*/
{
	fts_cache_clear(cache, TRUE);

	rw_lock_free(&cache->lock);
	mutex_free(&cache->optimize_lock);
	mutex_free(&cache->deleted_lock);
}

/********************************************************************
Find an existing word, or if not found, create one and return it. */
static
fts_tokenizer_word_t*
fts_tokenizer_word_get(
/*===================*/
						/* out: node */
	fts_cache_t*	cache,			/* in: cache */
	fts_index_cache_t*
			index_cache,		/* in: index cache */
	fts_string_t*	text)			/* in: node text */
{
	fts_tokenizer_word_t*	word;
	ib_rbt_bound_t		parent;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(rw_lock_own(&cache->lock, RW_LOCK_EX));
#endif

	/* If it is a stopword, do not index it */
	if (rbt_search(cache->stopword_info.cached_stopword,
            &parent, text) == 0) {
		return NULL;
	}

	/* Check if we found a match, if not then add word to tree. */
	if (rbt_search(index_cache->words, &parent, text) != 0) {
		fts_tokenizer_word_t	new_word;
		mem_heap_t*		heap = cache->sync_heap->arg;

		memset(&new_word, 0, sizeof(new_word));

		new_word.nodes = ib_vector_create(
			cache->sync_heap, sizeof(fts_node_t), 4);

		fts_utf8_string_dup(&new_word.text, text, heap);

		parent.last = rbt_add_node(
			index_cache->words, &parent, &new_word);

		/* Take into account the RB tree memory use and the vector. */
		cache->total_size += sizeof(new_word)
			+ sizeof(ib_rbt_node_t)
			+ text->len
			+ (sizeof(fts_node_t) * 4)
			+ sizeof(*new_word.nodes);

		ut_ad(rbt_validate(index_cache->words));
	}

	word = rbt_value(fts_tokenizer_word_t, parent.last);

	return(word);
}

/****************************************************************//**
Add the given doc_id/word positions to the given node's ilist. */
UNIV_INTERN
void
fts_cache_node_add_positions(
/*=========================*/
	fts_cache_t*	cache,			/* in: cache */
	fts_node_t*	node,			/* in: word node */
	doc_id_t	doc_id,			/* in: doc id */
	ib_vector_t*	positions)		/* in: fts_token_t::positions */
{
	ulint		i;
	byte*		ptr;
	byte*		ilist;
	ulint		enc_len;
	ulint		last_pos;
	byte*		ptr_start;
	ulint		doc_id_delta;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(rw_lock_own(&cache->lock, RW_LOCK_EX));
#endif
	ut_ad(doc_id > node->last_doc_id);

	/* Calculate the space required to store the ilist. */
	doc_id_delta = (ulint)(doc_id - node->last_doc_id);
	enc_len = fts_get_encoded_len(doc_id_delta);

	last_pos = 0;
	for (i = 0; i < ib_vector_size(positions); i++) {
		ulint	pos = *(ulint*) ib_vector_get(positions, i);

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

		ilist = ut_malloc(new_size);
		ptr = ilist + node->ilist_size;

		node->ilist_size_alloc = new_size;
	}

	ptr_start = ptr;

	/* Encode the new fragment. */
	ptr += fts_encode_int(doc_id_delta, ptr);

	last_pos = 0;
	for (i = 0; i < ib_vector_size(positions); i++) {
		ulint	pos = *(ulint*) ib_vector_get(positions, i);

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
	cache->total_size += enc_len;

	if (node->first_doc_id == 0) {
		node->first_doc_id = doc_id;
	}

	node->last_doc_id = doc_id;
	++node->doc_count;
}

/****************************************************************//**
Add document to the cache. */
UNIV_INTERN
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

	rw_lock_x_lock(&cache->lock);

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
			fts_node = ib_vector_last(word->nodes);
		}

		if (fts_node == NULL
		    || fts_node->ilist_size > FTS_ILIST_MAX_SIZE) {

			fts_node = ib_vector_push(word->nodes, NULL);

			memset(fts_node, 0x0, sizeof(*fts_node));

			cache->total_size += sizeof(*fts_node);
		}

		fts_cache_node_add_positions(
			cache, fts_node, doc_id, token->positions);

		ut_free(rbt_remove_node(tokens, node));
	}

	ut_a(rbt_empty(tokens));

	/* Add to doc ids processed so far. */
	doc_stats = ib_vector_push(index_cache->doc_stats, NULL);
	doc_stats->doc_id = doc_id;
	doc_stats->word_count = n_words;

	/* Add the doc stats memory usage too. */
	cache->total_size += sizeof(*doc_stats);

	if (cache->total_size > fts_max_cache_size) {
		fts_sync(cache->sync);
	}
 
	rw_lock_x_unlock(&cache->lock);
}

/********************************************************************
Drops a table. If the table can't be found we return a SUCCESS code. */
static
ulint
fts_drop_table(
/*===========*/
						/* out: DB_SUCCESS
						or error code */
	trx_t*		trx,			/* in: transaction */
	const char*	table_name)		/* in: table to drop */
{
	ulint		error = DB_SUCCESS;

	/* Check that the table exists in our data dictionary. */
	if (dict_table_get_low(table_name)) {

		ut_print_timestamp(stderr);
		fprintf(stderr, "  InnoDB: Dropping %s\n", table_name);

		error = row_drop_table_for_mysql(table_name, trx, TRUE);

		/* We only return the status of the last error. */
		if (error != DB_SUCCESS) {
			ut_print_timestamp(stderr);
			fprintf(stderr, "  InnoDB: Error: (%lu) dropping "
				"FTS index table %s\n", error, table_name);
		}
	} else {
		ut_print_timestamp(stderr);

		/* FIXME: Should provide appropriate error return code
		rather than printing message indiscriminately. */
		fprintf(stderr, "  InnoDB: %s not found.\n",
			table_name);
	}

	return(error);
}

/********************************************************************
Drops the common ancillary tables needed for supporting an FTS index
on the given table. row_mysql_lock_data_dictionary must have been called
before this. */
static
ulint
fts_drop_common_tables(
/*===================*/
						/* out: DB_SUCCESS
						or error code */
	trx_t*		trx,			/* in: transaction */
	fts_table_t*	fts_table)		/* in: table with an FTS
						index */
{
	ulint		i;
	ulint		error = DB_SUCCESS;

	for (i = 0; fts_common_tables[i] != NULL; ++i) {
		ulint	err;
		char*	table_name;

		fts_table->suffix = fts_common_tables[i];

		table_name = fts_get_table_name(fts_table);

		err = fts_drop_table(trx, table_name);

		/* We only return the status of the last error. */
		if (err != DB_SUCCESS) {
			error = err;
		}

		mem_free(table_name);
	}

	return(error);
}

/********************************************************************
Since we do a horizontal split on the index table, we need to drop the
all the split tables. */
static
ulint
fts_drop_index_split_tables(
/*========================*/
						/* out: DB_SUCCESS
						or error code */
	trx_t*		trx,			/* in: transaction */
	dict_index_t*	index)			/* in: fts instance */

{
	ulint		i;
	fts_table_t	fts_table;
	ulint		error = DB_SUCCESS;

	fts_table.suffix = NULL;
	fts_table.type = FTS_INDEX_TABLE;
	fts_table.index_id = index->id;
	fts_table.table_id = index->table->id;
	fts_table.parent = index->table->name;

	for (i = 0; fts_index_selector[i].ch; ++i) {
		ulint	err;
		char*	table_name;

		fts_table.suffix = fts_get_suffix(i);

		table_name = fts_get_table_name(&fts_table);

		err = fts_drop_table(trx, table_name);

		/* We only return the status of the last error. */
		if (err != DB_SUCCESS) {
			error = err;
		}

		mem_free(table_name);
	}

	return(error);
}

/********************************************************************
Drops the index ancillary tables needed for supporting an FTS index
on the given table. row_mysql_lock_data_dictionary must have been called
before this. */
static
ulint
fts_drop_index_tables(
/*==================*/
						/* out: DB_SUCCESS
						or error code */
	trx_t*		trx,			/* in: transaction */
	fts_t*		fts)			/* in: fts instance */
{
	ulint		i;
	fts_table_t	fts_table;
	ulint		error = DB_SUCCESS;

	static const char*	index_tables[] = {
		"DOC_ID",
		NULL
	};

	fts_table.suffix = NULL;
	fts_table.type = FTS_INDEX_TABLE;

	for (i = 0; i < ib_vector_size(fts->indexes); ++i) {
		ulint		j;
		ulint		err;
		dict_index_t*	index;

		index = ib_vector_getp(fts->indexes, i);

		err = fts_drop_index_split_tables(trx, index);

		/* We only return the status of the last error. */
		if (err != DB_SUCCESS) {
			error = err;
		}

		fts_table.index_id = index->id;
		fts_table.table_id = index->table->id;
		fts_table.parent = index->table->name;

		for (j = 0; index_tables[j] != NULL; ++j) {
			ulint	err;
			char*	table_name;

			fts_table.suffix = index_tables[j];

			table_name = fts_get_table_name(&fts_table);

			err = fts_drop_table(trx, table_name);

			/* We only return the status of the last error. */
			if (err != DB_SUCCESS) {
				error = err;
			}

			mem_free(table_name);
		}
	}

	return(error);
}

/********************************************************************
Drops the ancillary tables needed for supporting an FTS indexe.
row_mysql_lock_data_dictionary must have been called before this.

Precondition: The add thread must not be running. The table must
be deregistered from the optimize queue. */

ulint
fts_drop_tables(
/*============*/
						/* out: DB_SUCCESS
						or error code */
	trx_t*		trx,			/* in: transaction */
	fts_t*		fts,			/* in: fts instance */
	fts_table_t*	fts_table)		/* in: table with an FTS
						index */
{
	ulint		error;

	error = fts_drop_common_tables(trx, fts_table);

	if (error == DB_SUCCESS) {
		error = fts_drop_index_tables(trx, fts);
	}

	return(error);
}

/********************************************************************
Prepare the SQL, so that all '%s' are replaced by the common prefix. */
static
char*
fts_prepare_sql(
/*============*/
						/* out, own: use mem_free()
						to free the memory. */
	fts_table_t*	fts_table,		/* in: table name info */
	const char*	template)		/* in: sql template */
{
	char*		sql;
	char*		name_prefix;

	name_prefix = fts_get_table_name_prefix(fts_table);
	sql = ut_strreplace(template, "%s", name_prefix);
	mem_free(name_prefix);

	return(sql);
}

/********************************************************************
Creates the common ancillary tables needed for supporting an FTS index
on the given table. row_mysql_lock_data_dictionary must have been called
before this. */

ulint
fts_create_common_tables(
/*=====================*/
						/* out: DB_SUCCESS if OK */
	trx_t*		trx,			/* in: transaction */
	const dict_table_t*
			table,			/* in: table with FTS index */
	const char*	name,			/* in: table name normalized.*/
	ibool		skip_doc_id_index)	/* in: Skip index on doc id */

{
	char*		sql;
	ulint		error;
	que_t*		graph;
	fts_table_t	fts_table;
	mem_heap_t*	heap = mem_heap_create(1024);

	fts_table.suffix = NULL;
	fts_table.parent = table->name;
	fts_table.table_id = table->id;
	fts_table.parent = table->name;
	fts_table.type = FTS_COMMON_TABLE;

	error = fts_drop_common_tables(trx, &fts_table);

	if (error != DB_SUCCESS) {

		goto func_exit;
	}

	/* Create the FTS tables that are common to an FTS index. */
	sql = fts_prepare_sql(&fts_table, fts_create_common_tables_sql);
	graph = fts_parse_sql_no_dict_lock(NULL, NULL, sql);
	mem_free(sql);

	error = fts_eval_sql(trx, graph);

	que_graph_free(graph);

	if (error != DB_SUCCESS) {

		goto func_exit;
	}

	/* Write the default settings to the config table. */
	fts_table.suffix = "CONFIG";
	graph = fts_parse_sql_no_dict_lock(
		&fts_table, NULL, fts_config_table_insert_values_sql);

	error = fts_eval_sql( trx, graph);

	que_graph_free(graph);

	if (error != DB_SUCCESS || skip_doc_id_index) {

		goto func_exit;
	}

	/* Create the FTS DOC_ID index on the hidden column. Currently this
	is common for any FT index created on the table. */
	graph = fts_parse_sql_no_dict_lock(
		NULL,
		NULL,
		mem_heap_printf(
			heap,
			"BEGIN\n"
			""
			"CREATE UNIQUE INDEX %s ON %s(%s);\n",
			FTS_DOC_ID_INDEX_NAME, name, FTS_DOC_ID_COL_NAME));

	error = fts_eval_sql(trx, graph);
	que_graph_free(graph);

func_exit:
	if (error != DB_SUCCESS) {
		/* We have special error handling here */

		trx->error_state = DB_SUCCESS;

		trx_general_rollback_for_mysql(trx, NULL);

		row_drop_table_for_mysql(table->name, trx, FALSE);

		trx->error_state = DB_SUCCESS;
	}

	mem_heap_free(heap);

	return(error);
}

/********************************************************************
Creates the column specific ancillary tables needed for supporting an
FTS index on the given table. row_mysql_lock_data_dictionary must have
been called before this. */

ulint
fts_create_index_tables(
/*====================*/
					/* out: DB_SUCCESS or error code */
	trx_t*		trx,		/* in: transaction */
	const dict_index_t*
			index)		/* in: the index instance */
{
	ulint		i;
	char*		sql;
	que_t*		graph;
	dict_table_t*	table;
	fts_table_t	fts_table;
	ulint		error = DB_SUCCESS;
	mem_heap_t*	heap = mem_heap_create(1024);

	table = dict_table_get_low(index->table_name);
	ut_a(table != NULL);

	fts_table.type = FTS_INDEX_TABLE;
	fts_table.index_id = index->id;
	fts_table.table_id = table->id;
	fts_table.parent = table->name;

	/* Create the FTS auxiliary tables that are specific
	to an FTS index. */
	sql = fts_prepare_sql(&fts_table, fts_create_index_tables_sql);
	graph = fts_parse_sql_no_dict_lock(NULL, NULL, sql);
	mem_free(sql);

	error = fts_eval_sql(trx, graph);
	que_graph_free(graph);

	for (i = 0; fts_index_selector[i].ch && error == DB_SUCCESS; ++i) {

		/* Create the FTS auxiliary tables that are specific
		to an FTS index. We need to preserve the table_id %s
		which fts_parse_sql_no_dict_lock() will fill in for us. */
		fts_table.suffix = fts_get_suffix(i);

		graph = fts_parse_sql_no_dict_lock(
			&fts_table, NULL, fts_create_index_sql);

		error = fts_eval_sql(trx, graph);
		que_graph_free(graph);
	}

	if (error == DB_SUCCESS) {

		// FIXME: This causes a crash later, since commit will reset
		// trx->mysql_query_str to NULL.
		//error = fts_sql_commit(trx);

	} else {
		/* We have special error handling here */

		trx->error_state = DB_SUCCESS;

		trx_general_rollback_for_mysql(trx, NULL);

		row_drop_table_for_mysql(index->table->name, trx, FALSE);

		trx->error_state = DB_SUCCESS;
	}

	mem_heap_free(heap);

	return(error);
}

#if 0
/********************************************************************
Return string representation of state. */
static
const char*
fts_get_state_str(
/*==============*/
				/* out: string representation of state */
	fts_row_state	state)	/* in: state */
{
	switch (state) {
	case FTS_INSERT:
		return "INSERT";

	case FTS_MODIFY:
		return "MODIFY";

	case FTS_DELETE:
		return "DELETE";

	case FTS_NOTHING:
		return "NOTHING";

	case FTS_INVALID:
		return "INVALID";

	default:
		return "UNKNOWN";
	}
}
#endif

/********************************************************************
Calculate the new state of a row given the existing state and a new event. */
static
fts_row_state
fts_trx_row_get_new_state(
/*======================*/
						/* out: new state of row*/
	fts_row_state	old_state,		/* in: existing state of row */
	fts_row_state	event)			/* in: new event */
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

	result = table[(int)old_state][(int)event];
	ut_a(result != FTS_INVALID);

	return(result);
}

/********************************************************************
Create a savepoint instamce. */
static
fts_savepoint_t*
fts_savepoint_create(
/*=================*/
						/* out: savepoint instance */
	ib_vector_t*	savepoints,		/* out: InnoDB transaction */
	const char*	name,			/* in: savepoint name */
	mem_heap_t*	heap)			/* in: heap */
{
	fts_savepoint_t*	savepoint;

	savepoint = ib_vector_push(savepoints, NULL);

	memset(savepoint, 0x0, sizeof(*savepoint));

	if (name) {
		savepoint->name = mem_heap_strdup(heap, name);
	}

	savepoint->tables = rbt_create(
		sizeof(fts_trx_table_t*), fts_trx_table_cmp);

	return(savepoint);
}

/********************************************************************
Create an FTS trx. */
static
fts_trx_t*
fts_trx_create(
/*===========*/
						/* out, own: FTS trx */
	trx_t*	trx)				/* in: InnoDB transaction */
{
	fts_trx_t*	ftt;
	ib_alloc_t*	heap_alloc;
	mem_heap_t*	heap = mem_heap_create(1024);

	ftt = mem_heap_alloc(heap, sizeof(fts_trx_t));
	ftt->trx = trx;
	ftt->heap = heap;

	heap_alloc = ib_heap_allocator_create(heap);

	ftt->savepoints = ib_vector_create(
		heap_alloc, sizeof(fts_savepoint_t), 4);

	/* Default instance has no name and no heap. */
	fts_savepoint_create(ftt->savepoints, NULL, NULL);

	return(ftt);
}

/********************************************************************
Create an FTS trx table. */
static
fts_trx_table_t*
fts_trx_table_create(
/*=================*/
						/* out, own: FTS trx table */
	fts_trx_t*	fts_trx,		/* in: FTS trx */
	dict_table_t*	table)			/* in: table */
{
	fts_trx_table_t*	ftt;

	ftt = mem_heap_alloc(fts_trx->heap, sizeof(*ftt));

	memset(ftt, 0x0, sizeof(*ftt));

	ftt->table = table;
	ftt->fts_trx = fts_trx;

	ftt->rows = rbt_create(sizeof(fts_trx_row_t), fts_trx_row_doc_id_cmp);

	return(ftt);
}

/********************************************************************
Clone an FTS trx table. */
static
fts_trx_table_t*
fts_trx_table_clone(
/*=================*/
						/* out, own: FTS trx table */
	const fts_trx_table_t*	ftt_src)	/* in: FTS trx */
{
	fts_trx_table_t*	ftt;

	ftt = mem_heap_alloc(ftt_src->fts_trx->heap, sizeof(*ftt));

	memset(ftt, 0x0, sizeof(*ftt));

	ftt->table = ftt_src->table;
	ftt->fts_trx = ftt_src->fts_trx;

	ftt->rows = rbt_create(sizeof(fts_trx_row_t), fts_trx_row_doc_id_cmp);

	/* Copy the rb tree values to the new savepoint. */
	rbt_merge_uniq(ftt_src->rows, ftt->rows);

	/* These are only added on commit. At this stage we only have
	the updated row state. */
	ut_a(ftt_src->added_doc_ids == NULL);

	return(ftt);
}

/********************************************************************
Initialize the FTS trx instance. */
static
fts_trx_table_t*
fts_trx_init(
/*=========*/
	trx_t*			trx,		/* in: transaction */
	dict_table_t*		table)		/* in: FTS table instance */
{
	fts_trx_table_t*	ftt;
	ib_rbt_bound_t		parent;
	ib_rbt_t*		tables;
	fts_savepoint_t*	savepoint;

	/* Row id found update state, and if new state is FTS_NOTHING,
	we delete the row from our tree.*/
	if (!trx->fts_trx) {
		trx->fts_trx = fts_trx_create(trx);
	}

	savepoint = ib_vector_last(trx->fts_trx->savepoints);

	tables = savepoint->tables;
	rbt_search_cmp(tables, &parent, &table->id, fts_trx_table_id_cmp);

	if (parent.result == 0) {
		ftt = *rbt_value(fts_trx_table_t*, parent.last);
	} else {
		ftt = fts_trx_table_create(trx->fts_trx, table);
		rbt_add_node(tables, &parent, &ftt);
	}

	ut_a(ftt);
	ut_a(ftt->table == table);

	return(ftt);
}

/********************************************************************
Notify the FTS system about an operation on an FTS-indexed table. */
static
void
fts_trx_table_add_op(
/*=================*/
	fts_trx_table_t*ftt,			/* in: FTS trx table */
	doc_id_t	doc_id,			/* in: doc id */
	fts_row_state	state,			/* in: state of the row */
	ib_vector_t*	fts_indexes)		/* in: FTS indexes affected */
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
			ib_vector_free(row->fts_indexes);
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

/********************************************************************
Notify the FTS system about an operation on an FTS-indexed table. */

void
fts_trx_add_op(
/*===========*/
	trx_t*		trx,			/* in: InnoDB transaction */
	dict_table_t*	table,			/* in: table */
	doc_id_t	doc_id,			/* in: new doc id */
	fts_row_state	state,			/* in: state of the row */
	ib_vector_t*	fts_indexes)		/* in: FTS indexes affected */
{
	fts_trx_table_t*	ftt = fts_trx_init(trx, table);

	fts_trx_table_add_op(ftt, doc_id, state, fts_indexes);
}

/********************************************************************
Fetch callback that converts a textual document id to a binary value and
stores it in the given place. */
static
ibool
fts_fetch_store_doc_id(
/*===================*/
						/* out: always returns NULL */
	void*		row,			/* in: sel_node_t* */
	void*		user_arg)		/* in: doc_id_t* to store
						doc_id in */
{
	int		n_parsed;
	sel_node_t*	node = row;
	doc_id_t*	doc_id = user_arg;
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

/********************************************************************
Get the max cache size in bytes. If there is an error reading the
value we simply print an error message here and return the default
value to the caller. */
static
ulint
fts_get_max_cache_size(
/*===================*/
						/* out: max cache size in
						bytes. */
	trx_t*		trx,			/* in: transaction */
	fts_table_t*	fts_table)		/* in: table instance */
{
	ulint		error;
	fts_string_t	value;
	ulint		cache_size_in_mb;

	/* Set to the default value. */
	cache_size_in_mb = FTS_CACHE_SIZE_LOWER_LIMIT_IN_MB;

	/* We set the length of value to the max bytes it can hold. This
	information is used by the callback that reads the value. */
	value.len = FTS_MAX_CONFIG_VALUE_LEN;
	value.utf8 = ut_malloc(value.len + 1);

	error = fts_config_get_value(
		trx, fts_table, FTS_MAX_CACHE_SIZE_IN_MB, &value);

	if (error == DB_SUCCESS) {

		value.utf8[value.len] = 0;
		cache_size_in_mb = strtoul((char*) value.utf8, NULL, 10);

		if (cache_size_in_mb > FTS_CACHE_SIZE_UPPER_LIMIT_IN_MB) {

			ut_print_timestamp(stderr);
			fprintf(stderr, "  InnoDB: Warning: FTS max cache size "
				" (%lu) out of range. Minimum value is "
				"%luMB and the maximum values is %luMB, "
				"setting cache size to upper limit\n",
				cache_size_in_mb,
				FTS_CACHE_SIZE_LOWER_LIMIT_IN_MB,
				FTS_CACHE_SIZE_UPPER_LIMIT_IN_MB);

			cache_size_in_mb = FTS_CACHE_SIZE_UPPER_LIMIT_IN_MB;

		} else if  (cache_size_in_mb
			    < FTS_CACHE_SIZE_LOWER_LIMIT_IN_MB) {

			ut_print_timestamp(stderr);
			fprintf(stderr, "  InnoDB: Warning: FTS max cache size "
				" (%lu) out of range. Minimum value is "
				"%luMB and the maximum values is %luMB, "
				"setting cache size to lower limit\n",
				cache_size_in_mb,
				FTS_CACHE_SIZE_LOWER_LIMIT_IN_MB,
				FTS_CACHE_SIZE_UPPER_LIMIT_IN_MB);

			cache_size_in_mb = FTS_CACHE_SIZE_LOWER_LIMIT_IN_MB;
		}
	} else {
		ut_print_timestamp(stderr);
		fprintf(stderr, "InnoDB: Error: (%lu) reading max cache "
			"config value from config table\n", error);
	}

	ut_free(value.utf8);

	return(cache_size_in_mb * 1024 * 1024);
}

/*********************************************************************//**
Get the total number of documents in the FTS.
@return estimated number of rows in the table */
UNIV_INTERN
ulint
fts_get_total_document_count(
/*=========================*/
	dict_table_t*   table)		/*!< in: table instance */
{
	if (!table->stat_initialized) {
		dict_update_statistics(table);
	}

	return(table->stat_n_rows);
}

/********************************************************************
Get the total number of words in the FTS for a particular FTS index. */

ulint
fts_get_total_word_count(
/*=====================*/
						/* out: DB_SUCCESS if all OK
						else error code */
	trx_t*		trx,			/* in: transaction */
	dict_index_t*	index,			/* in: for this index */
	ulint*		total)			/* out: total words */
{
	ulint		error;
	fts_string_t	value;

	*total = 0;

	/* We set the length of value to the max bytes it can hold. This
	information is used by the callback that reads the value. */
	value.len = FTS_MAX_CONFIG_VALUE_LEN;
	value.utf8 = ut_malloc(value.len + 1);

	error = fts_config_get_index_value(
		trx, index, FTS_TOTAL_WORD_COUNT, &value);

	if (error == DB_SUCCESS) {

		value.utf8[value.len] = 0;
		*total = strtoul((char*) value.utf8, NULL, 10);
	} else {
		ut_print_timestamp(stderr);
		fprintf(stderr, "  InnoDB: Error: (%lu) reading total words "
			"value from config table\n", error);
	}

	ut_free(value.utf8);

	return(error);
}

/********************************************************************
Get the next available document id. This function creates a new
transaction to generate the document id. */

ulint
fts_get_next_doc_id(
/*================*/
						/* out: DB_SUCCESS if OK */
	dict_table_t*	table,			/* in: table */
	doc_id_t*	doc_id)			/* out: new document id */
{
	trx_t*		trx;
	pars_info_t*	info;
	ulint		error;
	fts_table_t	fts_table;
	que_t*		graph = NULL;
	fts_cache_t*	cache = table->fts->cache;;

retry:
	ut_a(table->fts->doc_col != ULINT_UNDEFINED);

	fts_table.suffix = "CONFIG";
	fts_table.table_id = table->id;
	fts_table.type = FTS_COMMON_TABLE;
	fts_table.parent = table->name;

	// FIXME: We will need a mutex here!
	/* Try and allocate from the reserved block. */
	if (cache->next_doc_id < cache->last_doc_id) {

		++cache->next_doc_id;
		*doc_id = cache->next_doc_id;

		return(DB_SUCCESS);
	}

	info = pars_info_create();

	pars_info_bind_function(
		info, "my_func", fts_fetch_store_doc_id, doc_id);

	graph = fts_parse_sql(
		&fts_table, info,
		"DECLARE FUNCTION my_func;\n"
		"DECLARE CURSOR c IS SELECT value FROM %s"
		" WHERE key = 'next_doc_id' FOR UPDATE;\n"
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

	trx = trx_allocate_for_background();

	trx->op_info = "getting next FTS document id";

	*doc_id = 0;
	error = fts_eval_sql(trx, graph);

	que_graph_free(graph);

	// FIXME: We need to retry deadlock errors
	if (error != DB_SUCCESS) {
		goto func_exit;
	}

	ut_a(*doc_id > 0);

	/* The column has to be stored in text format. */
	cache->next_doc_id = *doc_id;
	cache->last_doc_id = cache->next_doc_id + FTS_DOC_ID_STEP;

	error = fts_update_last_doc_id(table, cache->last_doc_id, trx);

func_exit:
	if (error == DB_SUCCESS) {
		fts_sql_commit(trx);
	} else {
		*doc_id = 0;

		ut_print_timestamp(stderr);
		fprintf(stderr, "  InnoDB: Error: (%lu) "
			"while getting next doc id.\n", error);

		fts_sql_rollback(trx);

		if (error == DB_DEADLOCK) {
			os_thread_sleep(FTS_DEADLOCK_RETRY_WAIT);
			goto retry;
		}
	}

	trx_free_for_background(trx);

	return(error);
}

/********************************************************************
Update the last document id. This function could create a new
transaction to update the last document id. */

ulint
fts_update_last_doc_id(
/*===================*/
						/* out: DB_SUCCESS if OK */
	dict_table_t*	table,			/* in: table */
	doc_id_t	doc_id,			/* in: last document id */
	trx_t*		trx)			/* in: update trx */
{
	byte		id[FTS_MAX_ID_LEN];
	pars_info_t*	info;
	fts_table_t	fts_table;
	ulint		id_len;
	que_t*		graph = NULL;
	ulint		error;
	ibool		local_trx = FALSE;
	fts_cache_t*	cache = table->fts->cache;;

	fts_table.suffix = "CONFIG";
	fts_table.table_id = table->id;
	fts_table.type = FTS_COMMON_TABLE;
	fts_table.parent = table->name;

	if (!trx) {
		trx = trx_allocate_for_background();

		trx->op_info = "setting last FTS document id";
		local_trx = TRUE;
	}

	info = pars_info_create();

	// FIXME: Get rid of snprintf
	id_len = snprintf(
		(char*) id, sizeof(id), FTS_DOC_ID_FORMAT, doc_id + 1);

	pars_info_bind_varchar_literal(info, "doc_id", id, id_len);

	graph = fts_parse_sql(
		&fts_table, info,
		"BEGIN "
		"UPDATE %s SET value = :doc_id"
		" WHERE key = 'next_doc_id';");

	error = fts_eval_sql(trx, graph);

	que_graph_free(graph);

	if (local_trx) {
		if (error == DB_SUCCESS) {
			fts_sql_commit(trx);
			cache->last_doc_id = doc_id;
		} else {
			ut_print_timestamp(stderr);
			fprintf(stderr, "  InnoDB: Error: (%lu) "
				"while updating last doc id.\n", error);

			fts_sql_rollback(trx);
		}
		trx_free_for_background(trx);
	}

	return(error);
}

/********************************************************************
Create a new fts_doc_ids_t. */

fts_doc_ids_t*
fts_doc_ids_create(void)
/*====================*/
						/* out: new fts_doc_ids_t */
{
	fts_doc_ids_t*	fts_doc_ids;
	mem_heap_t*	heap = mem_heap_create(512);

	fts_doc_ids = mem_heap_alloc(heap, sizeof(*fts_doc_ids));

	fts_doc_ids->self_heap = ib_heap_allocator_create(heap);

	fts_doc_ids->doc_ids = ib_vector_create(
		fts_doc_ids->self_heap, sizeof(fts_update_t), 32);

	return(fts_doc_ids);
}

/********************************************************************
Free a fts_doc_ids_t. */

void
fts_doc_ids_free(
/*=============*/
	fts_doc_ids_t*	fts_doc_ids)
{
	mem_heap_t*	heap = fts_doc_ids->self_heap->arg;

	memset(fts_doc_ids, 0, sizeof(*fts_doc_ids));

	mem_heap_free(heap);
}

/********************************************************************
Add the document id to the transaction's list of added document ids. */
static
void
fts_add_doc_id(
/*===========*/
	fts_trx_table_t*ftt,			/* in: FTS trx table */
	doc_id_t	doc_id,			/* in: doc id */
	ib_vector_t*	fts_indexes)		/* in: affected fts indexes */
{
	fts_cache_t*    	cache = ftt->table->fts->cache;
	ulint			i;

	if (cache->get_docs == NULL) {
		cache->get_docs = fts_get_docs_create(cache);
	}

	/* Get the document, parse them and add to FTS ADD table
	and FTS cache */
	for (i = 0; i < ib_vector_size(cache->get_docs); ++i) {
		fts_get_doc_t*  get_doc;
		get_doc = ib_vector_get(cache->get_docs, i);

		fts_add_doc(get_doc, doc_id);
	}

}

/********************************************************************
Do commit-phase steps necessary for the insertion of a new row. */
static
ulint
fts_add(
/*====*/
						/* out: DB_SUCCESS or error
						code */
	fts_trx_table_t*ftt,			/* in: FTS trx table */
	fts_trx_row_t*	row)			/* in: row */
{
	pars_info_t*	info;
	que_t*		graph;
	doc_id_t	write_doc_id;
	dict_table_t*	table = ftt->table;
	ulint		error = DB_SUCCESS;
	doc_id_t	doc_id = row->doc_id;

	ut_a(row->state == FTS_INSERT || row->state == FTS_MODIFY);

	fts_add_doc_id(ftt, doc_id, row->fts_indexes);

	graph = ftt->docs_added_graph;

	if (graph) {
		info = graph->info;
	} else {
		info = pars_info_create();
	}

	/* Convert to "storage" byte order. */
	fts_write_doc_id((byte*) &write_doc_id, doc_id);
	fts_bind_doc_id(info, "doc_id", &write_doc_id);

	if  (!graph) {
		fts_table_t	fts_table;

		fts_table.suffix = "ADDED";
		fts_table.type = FTS_COMMON_TABLE;
		fts_table.table_id = ftt->table->id;
		fts_table.parent = ftt->table->name;

		graph = fts_parse_sql(
			&fts_table,
			info,
			"BEGIN INSERT INTO %s VALUES (:doc_id);");

		ftt->docs_added_graph = graph;
	}

	ut_a(graph == ftt->docs_added_graph);

	error = fts_eval_sql(ftt->fts_trx->trx, graph);

	if (error == DB_SUCCESS) {
		mutex_enter(&table->fts->cache->deleted_lock);
		++table->fts->cache->added;
		mutex_exit(&table->fts->cache->deleted_lock);
	}

	return(error);
}

/********************************************************************
Do commit-phase steps necessary for the deletion of a row. */
static
ulint
fts_delete(
/*=======*/
						/* out: DB_SUCCESS or error
						code */
	fts_trx_table_t*ftt,			/* in: FTS trx table */
	fts_trx_row_t*	row)			/* in: row */
{
	que_t*		graph;
	ulint		error;
	undo_no_t	undo_no;
	fts_table_t	fts_table;
	doc_id_t	write_doc_id;
	dict_table_t*	table = ftt->table;
	ulint		n_rows_updated = 0;
	doc_id_t	doc_id = row->doc_id;
	trx_t*		trx = ftt->fts_trx->trx;
	pars_info_t*	info = pars_info_create();

	ut_a(doc_id != 0);
	ut_a(row->state == FTS_DELETE || row->state == FTS_MODIFY);

	trx->op_info = "deleting doc id from FTS ADDED";

	fts_table.suffix = "ADDED";
	fts_table.table_id = table->id;
	fts_table.type = FTS_COMMON_TABLE;
	fts_table.parent = table->name;

	/* Convert to "storage" byte order. */
	fts_write_doc_id((byte*) &write_doc_id, doc_id);
	fts_bind_doc_id(info, "doc_id", &write_doc_id);

	/* We want to reuse info */
	info->graph_owns_us = FALSE;

	graph = fts_parse_sql(
		&fts_table,
		info,
		"BEGIN DELETE FROM %s WHERE doc_id = :doc_id;\n");

	undo_no = trx->undo_no;

	error = fts_eval_sql(trx, graph);

	que_graph_free(graph);

	n_rows_updated = trx->undo_no -undo_no;

	/* If the row was deleted in FTS ADDED then the cache
	needs to know, */
	if (error == DB_SUCCESS && n_rows_updated > 0) {

		fts_update_t*	update;
		fts_cache_t*	cache = table->fts->cache;

		mutex_enter(&table->fts->cache->deleted_lock);
		ut_a(table->fts->cache->added > 0);

		--table->fts->cache->added;
		mutex_exit(&table->fts->cache->deleted_lock);

		/* Only if the row was really deleted. */
		ut_a(row->state == FTS_DELETE);

		mutex_enter(&cache->deleted_lock);

		/* There must be exactly one row */
		ut_a(n_rows_updated == 1);

		/* Add the doc id to the cache deleted doc id vector. */
		update = ib_vector_push(cache->deleted_doc_ids, NULL);

		update->doc_id = doc_id;
		update->fts_indexes = row->fts_indexes;

		mutex_exit(&cache->deleted_lock);
	}

	/* Note the deleted document for OPTIMIZE to purge. */
	if (error == DB_SUCCESS) {

		trx->op_info = "adding doc id to FTS DELETED";

		info->graph_owns_us = TRUE;

		fts_table.suffix = "DELETED";

		graph = fts_parse_sql(
			&fts_table,
			info,
			"BEGIN INSERT INTO %s VALUES (:doc_id);");

		error = fts_eval_sql(trx, graph);

		que_graph_free(graph);
	} else {
		pars_info_free(info);
	}

	/* Increment the total deleted count, this is used to calculate the
	number of documents indexed. */
	if (error == DB_SUCCESS) {

		error = fts_config_increment_value(
			trx,
			&fts_table,
			FTS_TOTAL_DELETED_COUNT,
			1);

		if (error == DB_SUCCESS) {
			mutex_enter(&table->fts->cache->deleted_lock);

			++table->fts->cache->deleted;

			mutex_exit(&table->fts->cache->deleted_lock);
		}
	}

	return(error);
}

/********************************************************************
Do commit-phase steps necessary for the modification of a row. */
static
ulint
fts_modify(
/*=======*/
						/* out: DB_SUCCESS or error
						code */
	fts_trx_table_t*	ftt,		/* in: FTS trx table */
	fts_trx_row_t*		row)		/* in: row */
{
	ulint			error;

	ut_a(row->state == FTS_MODIFY);

	error = fts_delete(ftt, row);

	if (error == DB_SUCCESS) {
		error = fts_add(ftt, row);
	}

	return(error);
}

/********************************************************************
Create a new document id .*/

ulint
fts_create_doc_id(
/*==============*/

					/* out: DB_SUCCESS if all went well
					else error */
	dict_table_t*	table,		/* in: row is of this table. */
	dtuple_t*	row,		/* in/out: add doc id value to this
					row. This is the current row that is
					being inserted. */
	mem_heap_t*	heap)		/* in: heap */
{
	ulint		error;
	doc_id_t	doc_id = 0;

	ut_a(table->fts->doc_col != ULINT_UNDEFINED);

	error = fts_get_next_doc_id(table, &doc_id);

	if (error == DB_SUCCESS) {
		dfield_t*	dfield;
		doc_id_t*	write_doc_id;

		ut_a(doc_id > 0);

		dfield = dtuple_get_nth_field(row, table->fts->doc_col);
		write_doc_id = mem_heap_alloc(heap, sizeof(*write_doc_id));

		ut_a(sizeof(doc_id) == dfield->type.len);
		fts_write_doc_id((byte*) write_doc_id, doc_id);
		dfield_set_data(dfield, write_doc_id, sizeof(*write_doc_id));
	}

	return(error);
}

/********************************************************************
The given transaction is about to be committed; do whatever is necessary
from the FTS system's POV. */
static
ulint
fts_commit_table(
/*=============*/
						/* out: DB_SUCCESS or error
						code */
	fts_trx_table_t*	ftt)		/* in: FTS table to commit*/
{
	const ib_rbt_node_t*	node;
	ib_rbt_t*		rows;
	ulint			error = DB_SUCCESS;

	rows = ftt->rows;

	for (node = rbt_first(rows);
	     node && error == DB_SUCCESS;
	     node = rbt_next(rows, node)) {

		fts_trx_row_t*	row = rbt_value(fts_trx_row_t, node);

		switch (row->state) {
		case FTS_INSERT:
			error = fts_add(ftt, row);
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

	return(error);
}

/********************************************************************
The given transaction is about to be committed; do whatever is necessary
from the FTS system's POV. */

ulint
fts_commit(
/*=======*/
						/* out: DB_SUCCESS or error
						code */
	trx_t*	trx)				/* in: transaction */
{
	const ib_rbt_node_t*	node;
	ulint			error;
	ib_rbt_t*		tables;
	fts_savepoint_t*	savepoint;

	savepoint = ib_vector_last(trx->fts_trx->savepoints);
	tables = savepoint->tables;

	for (node = rbt_first(tables), error = DB_SUCCESS;
	     node && error == DB_SUCCESS;
	     node = rbt_next(tables, node)) {

		fts_trx_table_t*	ftt;

		ftt = *rbt_value(fts_trx_table_t*, node);

		error = fts_commit_table(ftt);
	}

	return(error);
}

/********************************************************************
Create a new empty document. */

fts_doc_t*
fts_doc_init(
/*=========*/
						/* out, own: new document */
	fts_doc_t*	doc)			/* in: doc to initialize */
{
	mem_heap_t*	heap = mem_heap_create(32);

	memset(doc, 0, sizeof(*doc));

	doc->self_heap = ib_heap_allocator_create(heap);

	return(doc);
}

/********************************************************************
Free document. */

void
fts_doc_free(
/*=========*/
	fts_doc_t*	doc)			/* in: document */
{
	mem_heap_t*	heap = doc->self_heap->arg;

	if (doc->tokens) {
		rbt_free(doc->tokens);
	}

#ifdef UNIV_DEBUG
	memset(doc, 0, sizeof(*doc));
#endif /* UNIV_DEBUG */

	mem_heap_free(heap);
}

/********************************************************************
Callback function for fetch that stores a row id to the location pointed.
The column's type must be DATA_FIXBINARY, DATA_BINARY_TYPE, length = 8. */

void*
fts_fetch_row_id(
/*=============*/
						/* out: always returns NULL */
	void*	row,				/* in: sel_node_t* */
	void*	user_arg)			/* in: data pointer */
{
	sel_node_t*	node = row;

	dfield_t*	dfield = que_node_get_val(node->select_list);
	dtype_t*	type = dfield_get_type(dfield);
	ulint		len = dfield_get_len(dfield);

	ut_a(dtype_get_mtype(type) == DATA_FIXBINARY);
	ut_a(dtype_get_prtype(type) & DATA_BINARY_TYPE);
	ut_a(len == 8);

	memcpy(user_arg, dfield_get_data(dfield), 8);

	return(NULL);
}

/*************************************************************//**
Callback function for fetch that stores the text of an FTS document,
converting each column to UTF-16.
@return: always returns NULL */
UNIV_INTERN
ibool
fts_add_fetch_document(
/*===================*/
	void*	row,				/* in: sel_node_t* */
	void*	user_arg)			/* in: fts_doc_t* */
{

	que_node_t*	exp;
	sel_node_t*	node = row;
	fts_doc_t*	doc = user_arg;
	dfield_t*	dfield;
	ulint		len;
	ulint		doc_len;

	len = 0;

	exp = node->select_list;

	doc->found = TRUE;

	/* First to get the total length of doc for all columns */
	while (exp) {
		dfield = que_node_get_val(exp);
		len += (dfield_get_len(dfield) + 1);
		exp = que_node_get_next(exp);
	}

	doc->text.utf8 = ib_heap_malloc(doc->self_heap, len + 1);

	exp = node->select_list;
	doc_len = 0;

	/* Copy each indexed column content into doc->text.utf8 */
	while (exp) {
		dfield = que_node_get_val(exp);
		len = dfield_get_len(dfield);

		memcpy(doc->text.utf8 + doc_len, dfield_get_data(dfield), len);

		doc->text.utf8[doc_len + len] = 0;

		exp = que_node_get_next(exp);

		doc_len += (exp) ? len + 1 : len;
	}

	doc->text.utf8[doc_len] = 0;
	doc->text.len = doc_len;
	return(FALSE);
}


/*************************************************************//**
This function fetches the document just inserted right before
we commit the transaction, and tokenize the inserted text data
and insert into FTS auxiliary table and its cache. 
@return TRUE if successful */
static
ulint
fts_fetch_doc_by_id(
/*================*/
	fts_get_doc_t*	get_doc,	/*!< in: state */
	doc_id_t	doc_id,		/*!< in: id of document to
					fetch */
	fts_doc_t*	doc)		/*!< out: Document fetched */
{
	mtr_t		mtr;
	dict_index_t*   clust_index;
	const rec_t*    clust_rec;
	dict_table_t*	table = get_doc->index_cache->index->table;
	dict_index_t*	index = get_doc->index_cache->index;
	dtuple_t*	tuple; 
	doc_id_t        temp_doc_id;
	dfield_t*       dfield;
	mem_heap_t*	heap = get_doc->index_cache->index->heap;
	btr_pcur_t	pcur;
	
	clust_index = dict_table_get_first_index(table);

	mtr_start(&mtr);

	/* Search based on Doc ID. Here, we'll need to consider the case
	there is no primary index on Doc ID */
	tuple = dtuple_create(heap, 1);
	dfield = dtuple_get_nth_field(tuple, 0);
	dfield->type.mtype = DATA_INT;
	dfield->type.prtype = DATA_NOT_NULL | DATA_UNSIGNED | DATA_BINARY_TYPE;
	dfield->len = sizeof(doc_id_t);
	mach_write_to_8((byte*) &temp_doc_id, doc_id);
	dfield_set_data(dfield, &temp_doc_id, 8);

	btr_pcur_open_with_no_init(clust_index, tuple, PAGE_CUR_LE,
				   BTR_SEARCH_LEAF,
			  	   &pcur, 0, &mtr);

	/* If we have a match, add the data to doc structure */
	if (btr_pcur_get_low_match(&pcur) == 1) {
		ulint			len;
		const byte*		data;
		ulint			offsets_[REC_OFFS_NORMAL_SIZE];
		ulint*			offsets = offsets_;
		ulint			doc_len = 0;
		ulint			num_field;
		const dict_field_t*	ifield;
		const dict_col_t*	col;
		ulint			clust_pos;
		ulint			i;

		clust_rec = btr_pcur_get_rec(&pcur);

		/* This row should not be deleted */
		if (rec_get_deleted_flag(
			clust_rec, dict_table_is_comp(table))) {
				 ut_error;
		}

		offsets = rec_get_offsets(clust_rec, clust_index,
					  offsets, ULINT_UNDEFINED, &heap);

		num_field = dict_index_get_n_fields(index);

		for (i = 0; i < num_field; i++) {
			ifield = dict_index_get_nth_field(index, i);
			col = dict_field_get_col(ifield);
			clust_pos = dict_col_get_clust_pos(col, clust_index);
			data = rec_get_nth_field(clust_rec, offsets,
						 clust_pos, &len);
			doc_len += len;
		}

		doc->text.utf8 = ib_heap_malloc(doc->self_heap, doc_len + 1);
		doc_len = 0;

		for (i = 0; i < num_field; i++) {
			ifield = dict_index_get_nth_field(index, i);
			col = dict_field_get_col(ifield);
			clust_pos = dict_col_get_clust_pos(col, clust_index);
			data = rec_get_nth_field(clust_rec, offsets,
						 clust_pos, &len);
			memcpy(doc->text.utf8 + doc_len, data, len);
			doc_len += len;
		}

		doc->text.len = doc_len;
		doc->found = TRUE;
	}

	mtr_commit(&mtr);

	return(TRUE);
}

/*************************************************************//**
Fetch document (=a single row's indexed text) with the given
document id.
@return: DB_SUCCESS if OK else error */
UNIV_INTERN
ulint
fts_doc_fetch_by_doc_id(
/*====================*/
	fts_get_doc_t*	get_doc,	/*!< in: state */
	doc_id_t	doc_id,		/*!< in: id of document to
					fetch */
	dict_index_t*	index_to_use,	/*!< in: caller supplied FTS index */
	fts_sql_callback
			callback,	/*!< in: callback to read */
	void*		arg)		/*!< in: callback arg */
{
	pars_info_t*	info;
	ulint		error;
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

	if (!get_doc || !get_doc->get_document_graph) {
		graph = fts_parse_sql(
			NULL,
			info,
			mem_heap_printf(info->heap,
				"DECLARE FUNCTION my_func;\n"
				"DECLARE CURSOR c IS"
				" SELECT %s FROM %s"
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
				select_str, index->table_name,
				FTS_DOC_ID_COL_NAME));

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

	return(error);
}

/********************************************************************
Write out a single word's data as new entry/entries in the INDEX table. */

ulint
fts_write_node(
/*===========*/
	trx_t*		trx,			/* in: transaction */
	que_t**		graph,			/* in: query graph */
	fts_table_t*	fts_table,		/* in: aux table */
	fts_string_t*	word,			/* in: word in UTF-8 */
	fts_node_t*	node)			/* in: node columns */
{
	pars_info_t*	info;
	ulint		error;
	ib_uint32_t	doc_count;
	ib_time_t	start_time;
	doc_id_t	last_doc_id;
	doc_id_t	first_doc_id;

	if (*graph) {
		info = (*graph)->info;
	} else {
		info = pars_info_create();
	}

	ut_a(word->len <= FTS_MAX_UTF8_WORD_LEN);

	pars_info_bind_varchar_literal(info, "token", word->utf8, word->len);

	/* Convert to "storage" byte order. */
	fts_write_doc_id((byte*) &first_doc_id, node->first_doc_id);
	fts_bind_doc_id(info, "first_doc_id", &first_doc_id);

	/* Convert to "storage" byte order. */
	fts_write_doc_id((byte*) &last_doc_id, node->last_doc_id);
	fts_bind_doc_id(info, "last_doc_id", &last_doc_id);

	ut_a(node->last_doc_id >= node->first_doc_id);

	/* Convert to "storage" byte order. */
	mach_write_to_4((byte*) &doc_count, node->doc_count);
	pars_info_bind_int4_literal(info, "doc_count",
				    (const ib_uint32_t*) &doc_count);

	/* Set copy_name to FALSE since it's a static. */
	pars_info_bind_literal(
		info, "ilist", node->ilist, node->ilist_size,
		DATA_BLOB, DATA_BINARY_TYPE);

	if (!*graph) {
		*graph = fts_parse_sql(
			fts_table,
			info,
			"BEGIN\n"
			"INSERT INTO %s VALUES "
			"(:token, :first_doc_id,"
			" :last_doc_id, :doc_count, :ilist);");
	}

	start_time = ut_time();
	error = fts_eval_sql(trx, *graph);
	elapsed_time += ut_time() - start_time;
	++n_nodes;

	return(error);
}

/********************************************************************
Delete rows from the ADDED table that are indexed in the cache. */
static
ulint
fts_sync_delete_from_added(
/*=======================*/
						/* out: DB_SUCCESS if all OK */
	fts_sync_t*	sync)			/* in: sync state */
{
	pars_info_t*	info;
	que_t*		graph;
	ulint		error;
	fts_table_t	fts_table;
	doc_id_t	write_last;
	doc_id_t	write_first;

	ut_a(sync->max_doc_id >= sync->min_doc_id);

	info = pars_info_create();

	/* Convert to "storage" byte order. */
	fts_write_doc_id((byte*) &write_first, sync->min_doc_id);
	fts_bind_doc_id(info, "first", &write_first);

	/* Convert to "storage" byte order. */
	fts_write_doc_id((byte*) &write_last, sync->max_doc_id);
	fts_bind_doc_id(info, "last", &write_last);

	fts_table.suffix = "ADDED";
	fts_table.type = FTS_COMMON_TABLE;
	fts_table.table_id = sync->table->id;
	fts_table.parent = sync->table->name;

	/*
	printf("Deleting %lu -> %lu\n",
	       (ulint) sync->min_doc_id, (ulint) sync->max_doc_id);
	*/

	graph = fts_parse_sql(
		&fts_table,
		info,
		"BEGIN\n"
		"DELETE FROM %s WHERE doc_id >= :first AND doc_id <= :last;");

	error = fts_eval_sql(sync->trx, graph);
	que_graph_free(graph);

	return(error);
}

/********************************************************************
Add rows to the DELETED_CACHE table.*/
static
ulint
fts_sync_add_deleted_cache(
/*=======================*/
						/* out: DB_SUCCESS if all
						went well else error code */
	fts_sync_t*	sync,			/* in: sync state */
	ib_vector_t*	doc_ids)		/* in: doc ids to add */
{
	ulint		i;
	pars_info_t*	info;
	que_t*		graph;
	fts_table_t	fts_table;
	doc_id_t	dummy = 0;
	ulint		error = DB_SUCCESS;
	ulint		n_elems = ib_vector_size(doc_ids);

	ut_a(ib_vector_size(doc_ids) > 0);

	ib_vector_sort(doc_ids, fts_update_doc_id_cmp);

	info = pars_info_create();

	fts_bind_doc_id(info, "doc_id", &dummy);

	fts_table.type = FTS_COMMON_TABLE;
	fts_table.suffix = "DELETED_CACHE";
	fts_table.table_id = sync->table->id;
	fts_table.parent = sync->table->name;

	graph = fts_parse_sql(
		&fts_table,
		info,
		"INSERT INTO %s VALUES (:doc_id)");

	for (i = 0; i < n_elems && error == DB_SUCCESS; ++i) {
		fts_update_t*	update;
		doc_id_t	write_doc_id;

		update = ib_vector_get(doc_ids, i);

		/* Convert to "storage" byte order. */
		fts_write_doc_id((byte*) &write_doc_id, update->doc_id);
		fts_bind_doc_id(info, "doc_id", &write_doc_id);

		error = fts_eval_sql(sync->trx, graph);
	}

	que_graph_free(graph);

	return(error);
}

/********************************************************************
Write the words and ilist to disk.*/
static
ulint
fts_sync_write_words(
/*=================*/
	trx_t*		trx,			/* in: transaction */
	fts_index_cache_t*
			index_cache)		/* in: index cache */
{
	fts_table_t	fts_table;
	ulint		n_nodes = 0;
	ulint		n_words = 0;
	const ib_rbt_node_t* rbt_node;
	ulint		n_new_words = 0;
	ulint		error = DB_SUCCESS;
	ibool		print_error = FALSE;
	dict_table_t*	table = index_cache->index->table;

	fts_table.type = FTS_INDEX_TABLE;
	fts_table.index_id = index_cache->index->id;
	fts_table.table_id = index_cache->index->table->id;
	fts_table.parent = index_cache->index->table->name;

	n_words = rbt_size(index_cache->words);

	/* We iterate over the entire tree, even if there is an error,
	since we want to free the memory used during caching. */
	for (rbt_node = rbt_first(index_cache->words);
	     rbt_node;
	     rbt_node = rbt_first(index_cache->words)) {

		ulint	i;
		ulint	selected;
		fts_tokenizer_word_t* word;

		word = rbt_value(fts_tokenizer_word_t, rbt_node);

		selected = fts_select_index(*word->text.utf8);

		fts_table.suffix = fts_get_suffix(selected);

		/* Check if the word exists in the FTS index and if not
		then we need to increment the total word count stats. */
		if (error == DB_SUCCESS) {
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

		n_nodes += ib_vector_size(word->nodes);

		/* We iterate over all the nodes even if there was an error,
		this is to free the memory of the fts_node_t elements. */
		for (i = 0; i < ib_vector_size(word->nodes); ++i) {

			fts_node_t* fts_node = ib_vector_get(word->nodes, i);

			if (error == DB_SUCCESS) {

				error = fts_write_node(
					trx,
					&index_cache->ins_graph[selected],
					&fts_table, &word->text, fts_node);
			}

			ut_free(fts_node->ilist);
			fts_node->ilist = NULL;
		}

		if (error != DB_SUCCESS && !print_error) {
			ut_print_timestamp(stderr);
			fprintf(stderr, "  InnoDB: Error (%lu) writing "
				"word node to FTS auxiliary index "
				"table.\n", error);

			print_error = TRUE;
		}

		/* NOTE: We are responsible for free'ing the node */
		ut_free(rbt_remove_node(index_cache->words, rbt_node));
	}

	if (error == DB_SUCCESS) {
		fts_table_t	fts_table;

		fts_table.suffix = NULL;
		fts_table.table_id = table->id;
		fts_table.type = FTS_COMMON_TABLE;
		fts_table.parent = table->name;

		/* Increment the total number of words in the FTS index */
		error = fts_config_increment_index_value(
			trx,
			index_cache->index,
			FTS_TOTAL_WORD_COUNT,
			n_new_words);
	}

	printf("Avg number of nodes: %lf\n",
	       (double) n_nodes / (double) (n_words > 1 ? n_words : 1));

	return(error);
}

/********************************************************************
Write a single documents statistics to disk.*/
static
ulint
fts_sync_write_doc_stat(
/*====================*/
						/* out: DB_SUCCESS if all went
						well else error code */
	trx_t*			trx,		/* in: transaction */
	dict_index_t*		index,		/* in: index */
	que_t**			graph,		/* out: query graph */
	const fts_doc_stats_t*	doc_stat)	/* in: doc stats to write */
{
	pars_info_t*	info;
	doc_id_t	doc_id;
	ulint		error = DB_SUCCESS;

	if (*graph) {
		info = (*graph)->info;
	} else {
		info = pars_info_create();
	}

	/* Convert to "storage" byte order. */
	pars_info_bind_int4_literal(info, "count",
				    (const ib_uint32_t*) &doc_stat->word_count);

	/* Convert to "storage" byte order. */
	fts_write_doc_id((byte*) &doc_id, doc_stat->doc_id);
	fts_bind_doc_id(info, "doc_id", &doc_id);

	if (!*graph) {
		fts_table_t	fts_table;

		fts_table.suffix = "DOC_ID";
		fts_table.index_id = index->id;
		fts_table.type = FTS_INDEX_TABLE;
		fts_table.table_id = index->table->id;
		fts_table.parent = index->table->name;

		*graph = fts_parse_sql(
			&fts_table,
			info,
			"BEGIN INSERT INTO %s VALUES (:doc_id, :count);");
	}

	for (;;) {
		error = fts_eval_sql(trx, *graph);

		if (error == DB_SUCCESS) {

			break;				/* Exit the loop. */
		} else {
			ut_print_timestamp(stderr);

			if (error == DB_LOCK_WAIT_TIMEOUT) {
				fprintf(stderr, "  InnoDB: Warning: lock wait "
					"timeout writing to FTS doc_id. "
					"Retrying!\n");

				trx->error_state = DB_SUCCESS;
			} else {
				fprintf(stderr, "  InnoDB: Error: %lu "
					"while writing to FTS doc_id.\n",
					error);

				break;			/* Exit the loop. */
			}
		}
	}

	return(error);
}

/********************************************************************
Write document statistics to disk.*/
static
ulint
fts_sync_write_doc_stats(
/*=====================*/
						/* out: DB_SUCCESS if all OK */
	trx_t*			trx,		/* in: transaction */
	const fts_index_cache_t*index_cache)	/* in: index cache */
{
	ulint		i;
	que_t*		graph = NULL;
	ulint		error = DB_SUCCESS;

	for (i = 0; i < ib_vector_size(index_cache->doc_stats); ++i) {
		const fts_doc_stats_t*	doc_stat;

		doc_stat = ib_vector_get(index_cache->doc_stats, i);

		error = fts_sync_write_doc_stat(
			trx, index_cache->index, &graph, doc_stat);

		if (error != DB_SUCCESS) {
			break;
		}
	}

	if (graph != NULL) {
		que_graph_free(graph);
	}

	return(error);
}

/********************************************************************
Callback to check the existince of a word. */
static
ibool
fts_lookup_word(
/*============*/
						/* out: always returns NULL */
	void*	row,				/* in:  sel_node_t* */
	void*	user_arg)			/* in:  fts_doc_t* */
{

	que_node_t*	exp;
	sel_node_t*	node = row;
	ibool*		found = user_arg;

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

/********************************************************************
Check whether a particular word (term) exists in the FTS index. */
static
ulint
fts_is_word_in_index(
/*=================*/
						/* out: DB_SUCCESS if all went
						well else error code */
	trx_t*		trx,			/* in: FTS query state */
	que_t**		graph,			/* out: Query graph */
	fts_table_t*	fts_table,		/* in: table instance */
	const fts_string_t*
			word,			/* in: the word to check */
	ibool*		found)			/* out: TRUE if exists */
{
	pars_info_t*	info;
	ulint		error;

	trx->op_info = "looking up word in FTS index";

	if (*graph) {
		info = (*graph)->info;
	} else {
		info = pars_info_create();
	}

	pars_info_bind_function(info, "my_func", fts_lookup_word, found);
	pars_info_bind_varchar_literal(info, "word", word->utf8, word->len);

	if (*graph == NULL) {
		*graph = fts_parse_sql(
			fts_table,
			info,
			"DECLARE FUNCTION my_func;\n"
			"DECLARE CURSOR c IS"
			" SELECT doc_count\n"
			" FROM %s\n"
			" WHERE word = :word "
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
			ut_print_timestamp(stderr);

			if (error == DB_LOCK_WAIT_TIMEOUT) {
				fprintf(stderr, "  InnoDB: Warning: lock wait "
					"timeout reading FTS index. "
					"Retrying!\n");

				trx->error_state = DB_SUCCESS;
			} else {
				fprintf(stderr, "  InnoDB: Error: %lu "
					"while reading FTS index.\n", error);

				break;			/* Exit the loop. */
			}
		}
	}

	return(error);
}

/********************************************************************
Begin Sync, create transaction, acquire locks, etc. */
static
void
fts_sync_begin(
/*===========*/
	fts_sync_t*	sync)			/* in: sync state */
{
	fts_cache_t*	cache = sync->table->fts->cache;

	n_nodes = 0;
	elapsed_time = 0;

	sync->start_time = ut_time();

	sync->trx = trx_allocate_for_background();

	/* TODO: use upgrade-lock mode when we have such a lock implemented
	rwu_lock_u_lock(&cache->lock); */
	rw_lock_x_lock(&sync->table->fts->cache->lock);

	ut_print_timestamp(stderr);
	fprintf(stderr, "  SYNC deleted count: %ld size: %lu bytes\n",
		ib_vector_size(cache->deleted_doc_ids), cache->total_size);
}

/********************************************************************
Run SYNC on the table, i.e., write out data from the index specific
cache to the FTS aux INDEX table and FTS aux doc id stats table. */
static
ulint
fts_sync_index(
/*===========*/
						/* out: DB_SUCCESS if all OK */
	fts_sync_t*		sync,		/* in: sync state */
	fts_index_cache_t*	index_cache)	/* in: index cache */
{
	trx_t*		trx = sync->trx;
	ulint		error = DB_SUCCESS;

	trx->op_info = "doing SYNC index";

	ut_print_timestamp(stderr);
	fprintf(stderr, "  SYNC words: %ld\n", rbt_size(index_cache->words));

	ut_ad(rbt_validate(index_cache->words));

	error = fts_sync_write_words(trx, index_cache);

	/* Write the per doc statistics that will be used for ranking. */
	if (error == DB_SUCCESS) {

		error = fts_sync_write_doc_stats(trx, index_cache);
	}

	return(error);
}

/********************************************************************
Commit the SYNC, release the locks, change state of processed doc
ids etc. */
static
ulint
fts_sync_commit(
/*============*/
						/* out: DB_SUCCESS if all OK */
	fts_sync_t*	sync)			/* in: sync state */
{
	ulint		error;
	trx_t*		trx = sync->trx;
	fts_cache_t*	cache = sync->table->fts->cache;

	trx->op_info = "doing SYNC commit";

	/* TODO: uncomment when we have such a lock implemented
	rwu_lock_u_upgrade(&cache->lock); */

	/* Delete deleted Dod ID from ADD table */
	error = fts_sync_delete_from_added(sync);

	/* Get the list of deleted documents that are either in the
	cache or were headed there but were deleted before the add
	thread got to them. */
	mutex_enter(&cache->deleted_lock);

	if (error == DB_SUCCESS && ib_vector_size(cache->deleted_doc_ids) > 0) {

		error = fts_sync_add_deleted_cache(
			sync, cache->deleted_doc_ids);
	}

	/* We need to do this within the deleted lock since fts_delete() can
	attempt to add a deleted doc id to the cache deleted id array. Set
	the shutdown flag to FALSE, signifying that we don't want to release
	all resources. */
	fts_cache_clear(cache, FALSE);
	fts_cache_init(cache);

	mutex_exit(&cache->deleted_lock);

	if (error == DB_SUCCESS) {

		fts_sql_commit(trx);

	} else if (error != DB_SUCCESS) {

		fts_sql_rollback(trx);

		ut_print_timestamp(stderr);
		fprintf(stderr, "  InnoDB: Error: (%lu) during SYNC.\n", error);
	}

	ut_print_timestamp(stderr);
	fprintf(stderr, "  InnoDB: SYNC time : %ldsecs: elapsed %lf ins/sec\n",
		ut_time() - sync->start_time,
		(double) n_nodes/ (double) elapsed_time);

	trx_free_for_background(trx);
	sync->trx = NULL;

	/* TODO: use upgrade-lock when we have such a lock implemented
	rwu_lock_x_unlock(&cache->lock); */
	rw_lock_x_unlock(&cache->lock);

	return(error);
}

/********************************************************************
Run SYNC on the table, i.e., write out data from the cache to the
FTS auxiliary INDEX table and clear the cache at the end. */
static
ulint
fts_sync(
/*=====*/
						/* out: DB_SUCCESS if all OK */
	fts_sync_t*	sync)			/* in: sync state */
{
	ulint		i;
	ulint		total;
	ulint		added;
	ulint		deleted;
	ulint		threshold;
	ulint		error = DB_SUCCESS;
	fts_cache_t*	cache = sync->table->fts->cache;

	fts_sync_begin(sync);

	for (i = 0; i < ib_vector_size(cache->indexes); ++i) {
		fts_index_cache_t*	index_cache;

		index_cache = ib_vector_get(cache->indexes, i);

		error = fts_sync_index(sync, index_cache);

		if (error != DB_SUCCESS && !sync->interrupted) {

			break;
		}
	}

	if (error == DB_SUCCESS && !sync->interrupted) {
		error = fts_sync_commit(sync);
	}

	/* We need to check whether an optimize is required, for that
	we make copies of the two variables that control the trigger. These
	variables can change behind our back and we don't want to hold the
	lock for longer than is needed. */
	mutex_enter(&cache->deleted_lock);

	added = cache->added;
	deleted = cache->deleted;
	total = added + deleted;

	mutex_exit(&cache->deleted_lock);

	threshold = FTS_OPTIMIZE_ADD_THRESHOLD + FTS_OPTIMIZE_DEL_THRESHOLD;

	if (error == DB_SUCCESS && !sync->interrupted && total >= threshold) {

		fts_optimize_do_table(sync->table);

		mutex_enter(&cache->deleted_lock);

		ut_a(cache->added >= added);
		cache->added -= added;

		ut_a(cache->deleted >= deleted);
		cache->deleted -= deleted;

		mutex_exit(&cache->deleted_lock);
	}

	return(error);
}

/********************************************************************
Get the next token from the given string and store it in *token. If no token
was found, token->len is set to 0. */
UNIV_INTERN
ulint
fts_get_next_token(
/*===============*/
						/* out: number of characters
						handled in this call */
	byte*		start,			/* in: start of text */
	byte*		end,			/* in: one character past end of
						text */
	fts_string_t*	token,			/* out: token's text */
	ulint*		offset)			/* out: offset to token,
						measured as characters from
						'start' */
{
	const byte*	s;
	ulint		len = 0;
	ulint		prev_ch = 0;
	const byte*	word_start = NULL;
	ibool		in_number = FALSE;

	token->len = 0;

	/* Find the start of the token. */
	for (s = start; s < end; /* No op */) {
		ulint		ch;
		const byte*	ptr = s;

		ch = fts_utf8_decode(&ptr);

		if (ch != UTF8_ERROR) {
			in_number = fts_utf8_isdigit(ch);

			if (ch == '_' || fts_utf8_isalpha(ch) || in_number) {
				prev_ch = ch;
				word_start = s;
				*offset = word_start - start;
				s = ptr;
				break;
			}
		} else {
			ut_print_timestamp(stderr);
			fprintf(stderr, "  InnoDB: Error: decoding UTF-8 "
				"text\n");
		}

		s = ptr;
		prev_ch = ch;
	}

	if (!word_start) {
		/* Ingore the text read so for */
		goto end;
	}

	len = 1;

	/* Find the end of the token. We accept letters, digits
	and single ' characters. */
	while (s < end) {
		ulint		ch;
		const byte*	ptr = s;

		ch = fts_utf8_decode(&ptr);

		if (ch == UTF8_ERROR) {
			/* Skip */
			fprintf(stderr, "InnoDB: Error decoding UTF-8 text\n");
		} else if (in_number && ch == '.') {
			/* ut_ad(fts_utf8_isdigit(prev_ch)); */
		} else if (fts_utf8_isdigit(ch)) {
			/* Process digit. */
		} else if (fts_utf8_isalpha(ch)
			   || ch == '_'
			   || (ch == '\'' && prev_ch != '\'')) {

			/* In this case treat '.' as punctiation. */
			if (in_number && prev_ch == '.') {
				break;
			}

			in_number = FALSE;
		} else {
			break;
		}

		s = ptr;
		prev_ch = ch;
		++len;		/* For counting the number of characters. */
	}

	if (len <= FTS_MAX_WORD_LEN) {
		token->len = ut_min(FTS_MAX_UTF8_WORD_LEN, s - word_start);
		memcpy(token->utf8, word_start, token->len);

		/* The string can't end on a ' character. */
		if (token->utf8[token->len - 1] == '\'') {
			--token->len;
		}

		token->utf8[token->len] = 0;

		if (!in_number) {
			fts_utf8_tolower(token);
		}
	} else {
		ut_a(token->len == 0);
	}

	token->utf8[token->len] = 0;
end:
	return(s - start);
}

/********************************************************************
Process next token from document starting at the given position, i.e., add
the token's start position to the token's list of positions. */
static
ulint
fts_process_token(
/*==============*/
						/* out: number of characters
						handled in this call */
	fts_doc_t*	doc,			/* in/out: document to
						tokenize */
	fts_doc_t*	result,			/* out: if provided, save
						result here */
	ulint		start_pos)		/* in: start position in text */
{
	ulint		ret;
	fts_string_t	str;
	ulint		offset = 0;
	byte		buf[FTS_MAX_UTF8_WORD_LEN + 1];
	fts_doc_t*	result_doc;

	str.utf8 = buf;

	/* Determine where to save the result. */
	result_doc = (result) ? result : doc;

	ret = fts_get_next_token(
		doc->text.utf8 + start_pos,
		doc->text.utf8 + doc->text.len, &str, &offset);

	if (str.len > FTS_MIN_TOKEN_LENGTH) {
		fts_token_t*	token;
		ib_rbt_bound_t	parent;

		ut_a(str.len <= FTS_MAX_UTF8_WORD_LEN);

		/* Add the word to the document statistics. If the word
		hasn't been seen before we create a new entry for it. */
		if (rbt_search(result_doc->tokens, &parent, &str) != 0) {
			fts_token_t	new_token;
			mem_heap_t*	heap = result_doc->self_heap->arg;

			fts_utf8_string_dup(&new_token.text, &str, heap);

			new_token.positions = ib_vector_create(
				result_doc->self_heap, sizeof(ulint), 32);

			parent.last = rbt_add_node(
				result_doc->tokens, &parent, &new_token);

			ut_ad(rbt_validate(result_doc->tokens));
		}

		offset += start_pos;
		token = rbt_value(fts_token_t, parent.last);
		ib_vector_push(token->positions, &offset);
	}

	return(ret);
}

/******************************************************************//**
Tokenize a document. */
UNIV_INTERN
void
fts_tokenize_document(
/*==================*/
	fts_doc_t*	doc,		/* in/out: document to
					tokenize */
	fts_doc_t*	result)		/* out: if provided, save
					the result token here */
{
	ulint		i;
	ulint		inc;

	ut_a(!doc->tokens);

	doc->tokens = rbt_create(sizeof(fts_token_t), fts_utf8_string_cmp);

	for (i = 0; i < doc->text.len; i += inc) {

		inc = fts_process_token(doc, result, i);

		ut_a(inc > 0);
	}
}

/********************************************************************
Add the document with the given id to the table's cache, and run
SYNC if the cache grows too big. */
static
void
fts_add_doc(
/*========*/
	fts_get_doc_t*	get_doc,		/* in: state */
	doc_id_t	doc_id)			/* in: document id of document
						to add */
{
	fts_doc_t	doc;
	dict_table_t*	table = get_doc->index_cache->index->table;

	fts_doc_init(&doc);

	fts_fetch_doc_by_id(get_doc, doc_id, &doc);

	if (doc.found) {
		fts_tokenize_document(&doc, NULL);

		fts_cache_add_doc(
			table->fts->cache,
			get_doc->index_cache, doc_id, doc.tokens);
	} else {
		/* This can happen where the transaction that added/updated
		the row was rolled back. */
		ut_print_timestamp(stderr);
		fprintf(stderr, "  InnoDB: Warning: doc id (%lu) not found\n",
			(ulint) doc_id);
	}

	fts_doc_free(&doc);
}

/********************************************************************
Callback function for fetch that stores document ids from ADDED table to an
ib_vector_t. */
static
ibool
fts_fetch_store_doc_ids(
/*====================*/
						/* out: always returns
						non-NULL */
	void*	row,				/* in: sel_node_t* */
	void*	user_arg)			/* in: pointer to ib_vector_t */
{
	sel_node_t*	node = row;
	ib_vector_t*	vec = user_arg;	/* fts_update_t vector */

	dfield_t*	dfield = que_node_get_val(node->select_list);
	dtype_t*	type = dfield_get_type(dfield);
	ulint		len = dfield_get_len(dfield);
	void*		data = dfield_get_data(dfield);
	fts_update_t*	update = ib_vector_push(vec, NULL);

	ut_a(len == sizeof(doc_id_t));
	ut_a(dtype_get_mtype(type) == DATA_INT);
	ut_a(dtype_get_prtype(type) & DATA_UNSIGNED);

	memset(update, 0x0, sizeof(*update));
	update->doc_id = (doc_id_t) mach_read_from_8(data);

	return(TRUE);
}

/********************************************************************
Create the vector of fts_get_doc_t instances. */
UNIV_INTERN
ib_vector_t*
fts_get_docs_create(
/*================*/
						/* out: vector of
						fts_get_doc_t instances */
	fts_cache_t*	cache)			/* in: fts cache */
{
	ulint		i;
	ib_vector_t*	get_docs;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(rw_lock_own(&cache->lock, RW_LOCK_EX));
#endif
	/* We need one instance of fts_get_doc_t per index. */
	get_docs = ib_vector_create(
		cache->self_heap, sizeof(fts_get_doc_t), 4);

	/* Create the get_doc instance, we need one of these
	per FTS index. */
	for (i = 0; i < ib_vector_size(cache->indexes); ++i) {

		dict_index_t**	index;
		fts_get_doc_t*	get_doc;

		index = ib_vector_get(cache->indexes, i);

		get_doc = ib_vector_push(get_docs, NULL);

		memset(get_doc, 0x0, sizeof(*get_doc));

		get_doc->index_cache = fts_get_index_cache(cache, *index);

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
	ib_vector_t*	get_docs)		/* in: Doc retrieval vector */
{
	ulint		i;

	/* Release the get doc graphs if any. */
	for (i = 0; i < ib_vector_size(get_docs); ++i) {

		fts_get_doc_t*	get_doc = ib_vector_get(get_docs, i);

		if (get_doc->get_document_graph != NULL) {

			ut_a(get_doc->index_cache);

			que_graph_free(get_doc->get_document_graph);
			get_doc->get_document_graph = NULL;
		}
	}
}

/********************************************************************
Read the doc ids that are pending in the added table. */
static
ulint
fts_pending_read_doc_ids(
/*=====================*/
						/* out: DB_SUCCESS for OK else
						error code */
	fts_table_t*	fts_table,		/* in: aux table */
	ib_vector_t*	doc_ids)		/* out: pending doc ids */
{
	que_t*		graph;
	ibool		docs_read = FALSE;
	ulint		error = DB_SUCCESS;
	pars_info_t*	info = pars_info_create();
	trx_t*		trx = trx_allocate_for_background();

	trx->op_info = "fetching added document ids";

	pars_info_bind_function(
		info, "my_func", fts_fetch_store_doc_ids, doc_ids);

	fts_table->suffix = "ADDED";

	graph = fts_parse_sql(
		fts_table,
		info,
		"DECLARE FUNCTION my_func;\n"
		"DECLARE CURSOR c IS SELECT doc_id FROM %s"
		" ORDER BY doc_id;\n"
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

	while (!docs_read) {
		error = fts_eval_sql(trx, graph);

		if (error == DB_SUCCESS) {
			fts_sql_commit(trx);
			docs_read = TRUE;		/* Exit the loop. */
		} else {
			fts_sql_rollback(trx);

			ut_print_timestamp(stderr);

			if (error == DB_LOCK_WAIT_TIMEOUT) {
				fprintf(stderr, "  InnoDB: Warning: lock wait "
					"timeout reading added doc ids. "
					"Retrying!\n");

				trx->error_state = DB_SUCCESS;
			} else {
				fprintf(stderr, "  InnoDB: Error: (%lu) "
					"while reading added doc ids.\n",
					error);
				break;
			}
		}
	}

	que_graph_free(graph);

	trx_free_for_background(trx);

	return(error);
}

/********************************************************************
Check if the index is in the affected set. */
static
ibool
fts_is_index_updated(
/*=================*/
	const ib_vector_t*	fts_indexes,	/* in: affected FTS indexes */
	const fts_get_doc_t*	get_doc)	/* in: info for reading
						document */
{
	ulint		i;
	dict_index_t*	index = get_doc->index_cache->index;

	for (i = 0; i < ib_vector_size(fts_indexes); ++i) {
		const dict_index_t*	updated_fts_index;

		updated_fts_index = ib_vector_getp_const(fts_indexes, i);

		ut_a(updated_fts_index != NULL);

		//printf("%s,%s\n", updated_fts_index->name, index->name);

		if (updated_fts_index == index) {
			return(TRUE);
		}
	}

	return(FALSE);
}

/********************************************************************
Add the doc ids to the cache. */
static
void
fts_cache_add_doc_ids(
/*==================*/
	fts_sync_t*	sync,		/* in: sync state */
	fts_get_doc_t*	get_doc,	/* in: info for reading document */
	const ib_vector_t*
			doc_ids)	/* in: doc ids to add, they must
					be sorted in ASC order */
{
	ulint		i;
	dict_table_t*	table = sync->table;
	fts_cache_t*	cache = table->fts->cache;

	ut_a(sync->lower_index < sync->upper_index);

	/* Process the doc ids that were added, add them to the cache,
	until we detect that the cache is full. */
	for (i = sync->lower_index; i < sync->upper_index; ++i) {
		const fts_update_t*	update;

		update = ib_vector_get_const(doc_ids, i);

		//printf("ADDING doc id: %lu\n", (ulint) update->doc_id);

		/* Add the document id only if we don't know which FTS
		indexes were affected or the current index matches one
		of the fts_indexes. */
		if (update->fts_indexes == NULL
		    || fts_is_index_updated(update->fts_indexes, get_doc)) {

			fts_add_doc(get_doc, update->doc_id);

			/* Free the memory that is no longer required. This
			vector is not allocated on the heap and so must
			be freed explicitly. */
			if (update->fts_indexes != NULL) {
				ib_vector_free(update->fts_indexes);
			}
		}

		/* If the cache is full then note we have to SYNC to disk. */
		if (cache->total_size > sync->max_cache_size) {

			if (sync->max_doc_id == 0) {
				sync->cache_full = TRUE;
				sync->max_doc_id = update->doc_id;
				sync->upper_index = i + 1;
			} else {
				ut_a(sync->cache_full == TRUE);
			}
		}
	}
}

/********************************************************************
Add the doc ids to the cache for all the FTS indexes on a table,
when the cache is full then write cache contents to disk. */
static
ulint
fts_sync_doc_ids(
/*=============*/
						/* out: DB_SUCCESS if all OK */
	fts_sync_t*		sync,		/* in: the sync state */
	const ib_vector_t*	doc_ids)	/* in: the doc ids to add */
{
	ulint		error = DB_SUCCESS;
	fts_cache_t*	cache = sync->table->fts->cache;

	ut_a(ib_vector_size(doc_ids) > 0);

	/* Setup the SYNC state, we will attempt to add all
	the doc ids in the vector. */
	sync->max_doc_id  = 0;
	sync->interrupted = FALSE;
	sync->lower_index = 0;
	sync->upper_index = ib_vector_size(doc_ids);

	/* We need the lower bound of the doc ids that we are
	adding to the cache. */
	if (sync->min_doc_id == 0) {

		sync->min_doc_id  = *(doc_id_t*) ib_vector_get_const(
			doc_ids, sync->lower_index);
	}

	/* As long as there are no database errors and we are not
	interrupted while adding the doc ids to the cache. When the
	cache fills up, sync the cache contents to disk. */
	while (sync->lower_index < sync->upper_index) {

		ulint		i;

		// FIXME: For the case of FTS_MODIFY we need to update
		// only the updated indexes.

		/* Parse and add the resultant data to our FTS cache. */
		for (i = 0; i < ib_vector_size(cache->get_docs); ++i) {

			fts_get_doc_t*	get_doc;

			get_doc = ib_vector_get(cache->get_docs, i);

			/* Add the doc ids that are in the ADDED table
			but weren't processed to the cache. */
			fts_cache_add_doc_ids(sync, get_doc, doc_ids);
		}

		ut_a(!sync->interrupted);

		/* Received a shutdown signal or all the documents fit
		in the cache. */
		if (!sync->cache_full) {

			/* Note that all doc ids have been processed. */
			sync->lower_index = sync->upper_index;

			ut_a(sync->max_doc_id == 0);

			sync->max_doc_id =
				*(doc_id_t*) ib_vector_last_const(doc_ids);

			/* FIXME: Still requires synchronizing word info to
			disk at this stage.
			error = fts_sync(sync); */
			break;
		}

		/* These must hold! */
		ut_a(sync->min_doc_id > 0);
		ut_a(sync->min_doc_id <= sync->max_doc_id);
		ut_a(sync->upper_index <= ib_vector_size(doc_ids));

		/* SYNC the contents of the cache to disk. */
		error = fts_sync(sync);

		/* Problem SYNCing or we received a shutdown signal. */
		if (sync->interrupted || error != DB_SUCCESS) {

			break;
		}

		ut_a(cache->total_size == 0);

		/* SYNC'ed the cache to disk now do any remaining doc ids
		that were missed because the cache filled up. */

		sync->min_doc_id  = 0;
		sync->max_doc_id  = 0;
		sync->cache_full  = FALSE;
		sync->lower_index = sync->upper_index;
		sync->upper_index = ib_vector_size(doc_ids);

		if (sync->lower_index < sync->upper_index) {

			sync->min_doc_id = *(doc_id_t*) ib_vector_get_const(
				doc_ids, sync->lower_index);
		}
	}

	/* If all went well then this must hold. */
	if (error == DB_SUCCESS && !sync->interrupted) {

		ut_a(sync->lower_index == ib_vector_size(doc_ids));
	}

	return(error);
}

/********************************************************************
Callback function to read a single ulint column. */
static
ibool
fts_read_ulint(
/*===========*/
					/* out: always returns non-NULL */
	void*		row,		/* in: sel_node_t* */
	void*		user_arg)	/* in: pointer to ulint */
{
	sel_node_t*	sel_node = row;
	ulint*		value = user_arg;
	que_node_t*	exp = sel_node->select_list;
	dfield_t*	dfield = que_node_get_val(exp);
	void*		data = dfield_get_data(dfield);

	*value = mach_read_from_4(data);

	return(TRUE);
}

/********************************************************************
Fetch COUNT(*) from specified table. */
static
ulint
fts_get_rows_count(
/*===============*/
					/* out: the number of rows in
					the table */
	fts_table_t*	fts_table)	/* in: fts table to read */
{
	trx_t*		trx;
	pars_info_t*	info;
	que_t*		graph;
	ulint		error;
	ulint		count = 0;

	trx = trx_allocate_for_background();

	trx->op_info = "fetching FT table rows count";

	info = pars_info_create();

	pars_info_bind_function(info, "my_func", fts_read_ulint, &count);

	graph = fts_parse_sql(
		fts_table,
		info,
		"DECLARE FUNCTION my_func;\n"
		"DECLARE CURSOR c IS"
		" SELECT COUNT(*) "
		" FROM %s;\n"
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

			ut_print_timestamp(stderr);

			if (error == DB_LOCK_WAIT_TIMEOUT) {
				fprintf(stderr, "  InnoDB: Warning: lock wait "
					"timeout reading FTS table. "
					"Retrying!\n");

				trx->error_state = DB_SUCCESS;
			} else {
				fprintf(stderr, "  InnoDB: Error: %lu "
					"while reading FTS table.\n",
					error);

				break;			/* Exit the loop. */
			}
		}
	}

	que_graph_free(graph);

	trx_free_for_background(trx);

	return(count);
}

/********************************************************************
Read and sync the pending doc ids in the FTS auxiliary ADDED table. */
static
ulint
fts_load_from_added(
/*================*/
						/* out: DB_SUCCESS if all OK */
	fts_sync_t*	sync)			/* in: sync state */
{
	ulint		error;
	ib_vector_t*	doc_ids;
	fts_table_t	fts_table;
	ib_alloc_t*	heap_alloc;
	mem_heap_t*	heap = mem_heap_create(1024);

	heap_alloc = ib_heap_allocator_create(heap);

	/* For collecting doc ids read from ADDED table. */
	doc_ids = ib_vector_create(heap_alloc, sizeof(fts_update_t), 256);

	/* Read the doc ids that have not been parsed and added to our
	internal auxiliary ADDED table. */

	fts_table.type = FTS_COMMON_TABLE;
	fts_table.table_id = sync->table->id;
	fts_table.parent = sync->table->name;

	/* Since we will be creating a transaction, we piggy back reading
	of the config value, max_cache_size. */
	error = fts_pending_read_doc_ids(&fts_table, doc_ids);

	/* Set the state of the FTS subsystem for this table to READY. */
	if (error == DB_SUCCESS) {
		fts_t*	fts = sync->table->fts;

		mutex_enter(&fts->bg_threads_mutex);
		fts->fts_status |= BG_THREAD_READY;
		mutex_exit(&fts->bg_threads_mutex);
	}

	/* SYNC the pending doc ids to disk */
	if (error == DB_SUCCESS && ib_vector_size(doc_ids) > 0) {

		ulint		count;
		fts_cache_t*	cache = sync->table->fts->cache;

		fts_table.suffix = "DELETED";
		count = fts_get_rows_count(&fts_table);

		/* Read the information that we will use to trigger
		optimizations of this table. */
		mutex_enter(&cache->deleted_lock);
		cache->added += ib_vector_size(doc_ids);
		cache->deleted += count;
		mutex_exit(&cache->deleted_lock);

		ut_print_timestamp(stderr);
		fprintf(stderr, "  InnoDB: Added %lu deleted %lu doc ids\n",
			cache->added, cache->deleted);

		rw_lock_x_lock(&cache->lock);

		ut_a(cache->get_docs == NULL);

		/* We need one instance of fts_get_doc_t per index. */
		cache->get_docs = fts_get_docs_create(cache);

		rw_lock_x_unlock(&cache->lock);

		error = fts_sync_doc_ids(sync, doc_ids);

		/* Force any trailing data in the cache to disk. */
		if (error == DB_SUCCESS && !sync->interrupted) {

			error = fts_sync(sync);
		}

		fts_get_docs_clear(cache->get_docs);
	}

	mem_heap_free(heap);

	return(error);
}

/********************************************************************
Read the max cache size parameter from the config table. */
static
void
fts_update_max_cache_size(
/*======================*/
	fts_sync_t*	sync)			/* in: sync state */
{
	trx_t*		trx;
	fts_table_t	fts_table;

	trx = trx_allocate_for_background();

	fts_table.suffix = "CONFIG";
	fts_table.type = FTS_COMMON_TABLE;
	fts_table.table_id = sync->table->id;
	fts_table.parent = sync->table->name;

	/* The size returned is in bytes. */
	sync->max_cache_size = fts_get_max_cache_size(trx, &fts_table);

	fts_sql_commit(trx);

	trx_free_for_background(trx);
}

/********************************************************************
Process the doc ids as they arrive via our queue, add the doc ids to
the FTS cache and SYNC when the cache gets full. */
static
void
fts_process_doc_ids(
/*=================*/
	fts_sync_t*	sync)			/* in: sync state */
{
	ulint		error = DB_SUCCESS;
	dict_table_t*	table = sync->table;
	fts_cache_t*	cache = table->fts->cache;

	/* Init the SYNC state */
	sync->min_doc_id  = 0;
	sync->max_doc_id  = 0;
	sync->lower_index = 0;
	sync->upper_index = 0;
	sync->interrupted = FALSE;

	/* Process the doc ids as they are added. */
	while (error == DB_SUCCESS) {
		fts_doc_ids_t*	doc_ids_queued;
		fts_t*		fts = table->fts;

		doc_ids_queued = ib_wqueue_wait(fts->add_wq);

		if (fts_is_stop_signalled(fts)) {

			fts_doc_ids_free(doc_ids_queued);

			sync->interrupted = TRUE;

			break;
		}

		/* Only check and update the cache size at the
		start of the sync cycle. */
		if (cache->total_size == 0) {

			fts_update_max_cache_size(sync);
		}

		ut_a(sync->max_cache_size > 0);

		error = fts_sync_doc_ids(sync, doc_ids_queued->doc_ids);

		/* Free the doc ids that were just added to the cache. */
		fts_doc_ids_free(doc_ids_queued);
	}
}

/********************************************************************
Start function for the background 'Add' threads. */

os_thread_ret_t
fts_add_thread(
/*===========*/
						/* out: a dummy parameter */
	void*		arg)			/* in: dict_table_t* */
{
	fts_sync_t	sync;
	ulint		error;
	dict_table_t*	table = arg;

	memset(&sync, 0x0, sizeof(sync));

	/* The table that this thread is responsible for. */
	sync.table = table;

	fts_update_max_cache_size(&sync);

	/* Register the table with the optimize thread. */
	fts_optimize_add_table(table);

	/* Read and sync the pending doc ids. */
	error = fts_load_from_added(&sync);

	if (error == DB_SUCCESS) {
		fts_cache_t*	cache = sync.table->fts->cache;

		rw_lock_x_lock(&cache->lock);

		if (cache->get_docs == NULL) {
			cache->get_docs = fts_get_docs_create(cache);
		}

		/* Load the stopword if it has not been loaded */
		if (cache->stopword_info.status & STOPWORD_NOT_INIT) {
			fts_load_stopword(table, NULL, NULL, TRUE, TRUE);
		}

		rw_lock_x_unlock(&cache->lock);

		/* Process doc ids as they arrive. */
		fts_process_doc_ids(&sync);

		fts_get_docs_clear(cache->get_docs);
	}

	ut_print_timestamp(stderr);
	fprintf(stderr, "  InnoDB: FTS Add thread deregister %s\n",
		table->name);

	/* Inform the optimize thread that it should stop
	OPTIMIZING this table and remove it from its list. */
	fts_optimize_remove_table(table);

	mutex_enter(&table->fts->bg_threads_mutex);
	--table->fts->bg_threads;
	mutex_exit(&table->fts->bg_threads_mutex);

	ut_print_timestamp(stderr);
	fprintf(stderr, "  InnoDB: FTS Add thread for %s exiting\n",
		table->name);

	/* We count the number of threads in os_thread_exit(). A created
	thread should always use that to exit and not use return() to exit. */
	os_thread_exit(NULL);

	OS_THREAD_DUMMY_RETURN;
}

/********************************************************************
Free the modified rows of a table. */
UNIV_INLINE
void
fts_trx_table_rows_free(
/*====================*/
	ib_rbt_t*	rows)			/* in: rbt of rows to free */
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
}

/********************************************************************
Free an FTS savepoint instance. */
UNIV_INLINE
void
fts_savepoint_free(
/*===============*/
	fts_savepoint_t*	savepoint)	/* in: savepoint instance */
{
	const ib_rbt_node_t*	node;
	ib_rbt_t*		tables = savepoint->tables;

	/* Nothing to free! */
	if (tables == NULL) {
		return;
	}

	for (node = rbt_first(tables); node; node = rbt_first(tables)) {
		fts_trx_table_t*	ftt;

		ftt = *rbt_value(fts_trx_table_t*, node);

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
			que_graph_free(ftt->docs_added_graph);
		}

		/* NOTE: We are responsible for free'ing the node */
		ut_free(rbt_remove_node(tables, node));
	}

	ut_a(rbt_empty(tables));
	rbt_free(tables);
	savepoint->tables = NULL;
}

/********************************************************************
Free an FTS trx. */

void
fts_trx_free(
/*=========*/
	fts_trx_t*	fts_trx)		/* in, own: FTS trx */
{
	ulint		i;

	for (i = 0; i < ib_vector_size(fts_trx->savepoints); ++i) {
		fts_savepoint_t*	savepoint;

		savepoint = ib_vector_get(fts_trx->savepoints, i);

		/* The default savepoint name must be NULL. */
		if (i == 0) {
			ut_a(savepoint->name == NULL);
		}

		fts_savepoint_free(savepoint);
	}

#ifdef UNIV_DEBUG
	memset(fts_trx, 0, sizeof(*fts_trx));
#endif /* UNIV_DEBUG */

	mem_heap_free(fts_trx->heap);
}

/********************************************************************
Extract the doc id from the FTS hidden column.*/

doc_id_t
fts_get_doc_id_from_row(
/*====================*/
	dict_table_t*	table,			/* in: table */
	dtuple_t*	row)			/* in: row whose FTS doc id we
						want to extract.*/
{
	dfield_t*	field;
	doc_id_t	doc_id = 0;

	ut_a(table->fts->doc_col != ULINT_UNDEFINED);

	field = dtuple_get_nth_field(row, table->fts->doc_col);

	ut_a(dfield_get_len(field) == sizeof(doc_id));
	ut_a(dfield_get_type(field)->mtype == DATA_INT);

	doc_id = fts_read_doc_id(dfield_get_data(field));

	/* Must not be 0. */
	ut_a(doc_id > 0);

	return(doc_id);
}

/********************************************************************
Extract the doc id from the FTS hidden column.*/

doc_id_t
fts_get_doc_id_from_rec(
/*====================*/
						/* out: doc id that was
						extracted from rec */
	dict_table_t*	table,			/* in: table */
	const rec_t*	rec,			/* in: rec */
	mem_heap_t*	heap)			/* in: heap */
{
	ulint		len;
	const byte*	data;
	ulint		col_no;
	ulint*		offsets;
	doc_id_t	doc_id = 0;
	dict_index_t*	clust_index;
	ulint		offsets_[REC_OFFS_NORMAL_SIZE];

	ut_a(table->fts->doc_col != ULINT_UNDEFINED);

	offsets	= offsets_;
	clust_index = dict_table_get_first_index(table);

	offsets_[0] = UT_ARR_SIZE(offsets_);

	offsets = rec_get_offsets(
		rec, clust_index, offsets, ULINT_UNDEFINED, &heap);

	col_no = dict_col_get_clust_pos(
		&table->cols[table->fts->doc_col], clust_index);

	/* We have no choice but to cast rec here :-( */
	data = rec_get_nth_field((rec_t*) rec, offsets, col_no, &len);

	ut_a(len == 8);
	ut_a(len == sizeof(doc_id));
	doc_id = (doc_id_t) mach_read_from_8(data);

	/* Must not be 0. */
	ut_a(doc_id > 0);

	return(doc_id);
}

/********************************************************************
Search the index specific cache for a particular FTS index. */

const fts_index_cache_t*
fts_find_index_cache(
/*=================*/
						/* out: the index specific
						cache else NULL */
	const fts_cache_t*	cache,		/* in: cache to search */
	const dict_index_t*	index)		/* in: index to search for */
{
	/* We cast away the const because our internal function, takes
	non-const cache arg and returns a non-const pointer. */
	return(fts_get_index_cache((fts_cache_t*) cache, index));
}

/********************************************************************
Search cache for word. */

const ib_vector_t*
fts_cache_find_word(
/*================*/
						/* out: the word node vector
						if found else NULL */
	const fts_index_cache_t*index_cache,	/* in: cache to search */
	const fts_string_t*	text)		/* in: word to search for */
{
	ib_rbt_bound_t		parent;
	const ib_vector_t*	nodes = NULL;
#ifdef UNIV_SYNC_DEBUG
	dict_table_t*		table = index_cache->index->table;
	fts_cache_t*		cache = table->fts->cache;

	ut_ad(rw_lock_own((rw_lock_t*)&cache->lock, RW_LOCK_EX));
#endif

	/* Lookup the word in the rb tree */
	if (rbt_search(index_cache->words, &parent, text) == 0) {
		const fts_tokenizer_word_t*	word;

		word = rbt_value(fts_tokenizer_word_t, parent.last);

		nodes = word->nodes;
	}

	return(nodes);
}

/********************************************************************
Check cache for deleted doc id. */

ibool
fts_cache_is_deleted_doc_id(
/*========================*/
						/* out: TRUE if deleted */
	const fts_cache_t*	cache,		/* in: cache ito search */
	doc_id_t		doc_id)		/* in: doc id to search for */
{
	ulint			i;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&cache->deleted_lock));
#endif

	for (i = 0; i < ib_vector_size(cache->deleted_doc_ids); ++i) {
		const fts_update_t*	update;

		update = ib_vector_get_const(cache->deleted_doc_ids, i);

		if (doc_id == update->doc_id) {

			return(TRUE);
		}
	}

	return(FALSE);
}

/********************************************************************
Append deleted doc ids to vector. */

void
fts_cache_append_deleted_doc_ids(
/*=============================*/
	const fts_cache_t*	cache,		/* in: cache to use */
	ib_vector_t*		vector)		/* in: append to this vector */
{
	ulint			i;

	mutex_enter((mutex_t*) &cache->deleted_lock);

	for (i = 0; i < ib_vector_size(cache->deleted_doc_ids); ++i) {
		fts_update_t*	update;

		update = ib_vector_get(cache->deleted_doc_ids, i);

		ib_vector_push(vector, &update->doc_id);
	}

	mutex_exit((mutex_t*) &cache->deleted_lock);
}

/********************************************************************
Wait for the background thread to start. We poll to detect change
of state, which is acceptable, since the wait should happen only
once during startup. */

ibool
fts_wait_for_background_thread_to_start(
/*====================================*/
						/* out: true if the thread
						started else FALSE (i.e timed
						out) */
	dict_table_t*		table,		/* in: table to which the thread
						is attached */
	ulint			max_wait)	/* in: time in microseconds, if
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
			ut_print_timestamp(stderr);
			fprintf(stderr, " InnoDB: Error the background thread "
				"for the FTS table %s refuses to start\n",
				table->name);

			count = 0;
		}
	}

	return(done);
}

/********************************************************************
Add the FTS document id hidden column. */

void
fts_add_doc_id_column(
/*==================*/
	dict_table_t*	table)		/* in/out: Table with FTS index */
{
	dict_mem_table_add_col(
		table,
		table->heap,
		FTS_DOC_ID_COL_NAME,
		DATA_INT,
		// FIXME: Hacked the value of 603
		dtype_form_prtype(0x603, 0),
		sizeof(doc_id_t));
}

/********************************************************************
Update the query graph with a new document id. */

ulint
fts_update_doc_id(
/*==============*/
					/* out: DB_SUCCESS or error code */
	dict_table_t*	table,		/* in: table */
	upd_field_t*	ufield,		/* out: update node */
	doc_id_t*	next_doc_id)	/* out: buffer for writing */
{
	ulint		error;
	doc_id_t	doc_id;

	/* Get the new document id that will be added. */
	error = fts_get_next_doc_id(table, &doc_id);

	if (error == DB_SUCCESS) {
		dict_index_t*	clust_index;

		ufield->exp = NULL;

		ufield->new_val.len = sizeof(doc_id_t);

		ufield->field_no = table->fts->doc_col;

		clust_index = dict_table_get_first_index(table);

		ufield->field_no = dict_col_get_clust_pos(
			&table->cols[table->fts->doc_col], clust_index);

		// FIXME: Testing/debugging (Need to address endian issues).
		/* Convert to storage byte order. */
		fts_write_doc_id((byte*) next_doc_id, doc_id);
		ufield->new_val.data = next_doc_id;
	}

	return(error);
}

/************************************************************************
Check if the table has an FTS index. This is the non-inline version
of dict_table_has_fts_index(). FIXME: Rename using normal convention. */

ibool
fts_dict_table_has_fts_index(
/*=========================*/
				/* out: TRUE if table has an FTS index */
	dict_table_t*	table)	/* in: table */
{
	return(dict_table_has_fts_index(table));
}

/************************************************************************
Create an instance of fts_t. */

fts_t*
fts_create(
/*=======*/
				/* out: instance of fts_t */
	dict_table_t*	table)	/* out: table with FTS indexes */
{
	fts_t*		fts;
	ib_alloc_t*	heap_alloc;

	fts = mem_heap_alloc(table->heap, sizeof(*fts));

	memset(fts, 0x0, sizeof(*fts));

	fts->doc_col = ULINT_UNDEFINED;

	mutex_create(fts_bg_threads_mutex_key, &fts->bg_threads_mutex,
		     SYNC_DICT_BG_THREADS_MUTEX);

	heap_alloc = ib_heap_allocator_create(table->heap);
	fts->indexes = ib_vector_create(heap_alloc, sizeof(dict_index_t*), 4);
	dict_table_get_all_fts_indexes(table, fts->indexes);

	return(fts);
}

/************************************************************************
Free the FTS resources. */

void
fts_free(
/*=====*/
	fts_t*		fts)	/* out: fts_t instance */
{
	mutex_free(&fts->bg_threads_mutex);

	if (fts->add_wq) {
		/* We need to free the items in the work queue. */
		ib_list_node_t*	node = ib_list_get_first(fts->add_wq->items);

		while (node != NULL) {

			/* Since the node is allocated from the same heap
			as the fts_doc_ids_t, we first remove the node from
			the list then free the heap. */
			ib_list_remove(fts->add_wq->items, node);

			fts_doc_ids_free(node->data);

			node = ib_list_get_first(fts->add_wq->items);
		}

		ib_wqueue_free(fts->add_wq);
	}

	if (fts->cache) {
		fts_cache_sync_and_free(fts->cache);
	}
}

/********************************************************************
Signal FTS threads to initiate shutdown. */

void
fts_start_shutdown(
/*===============*/
	dict_table_t*	table,		/* in: table with FTS indexes */
	fts_t*		fts)		/* in: fts instance that needs
					to be informed about shutdown */
{
	mutex_enter(&fts->bg_threads_mutex);

	fts->fts_status |= BG_THREAD_STOP;

	if (fts->add_wq) {
		dict_table_wakeup_bg_threads(table);
	}

	mutex_exit(&fts->bg_threads_mutex);
}

/********************************************************************
Wait for FTS threads to shutdown. */

void
fts_shutdown(
/*=========*/
	dict_table_t*	table,		/* in: table with FTS indexes */
	fts_t*		fts)		/* in: fts instance to shutdown */
{
	mutex_enter(&fts->bg_threads_mutex);

	ut_a(fts->fts_status & BG_THREAD_STOP);

	dict_table_wait_for_bg_threads_to_exit(table, 20000);

	mutex_exit(&fts->bg_threads_mutex);
}

/********************************************************************
Take a FTS savepoint. */
UNIV_INLINE
void
fts_savepoint_copy(
/*===============*/
	const fts_savepoint_t*	src,	/* in: source savepoint */
	fts_savepoint_t*	dst)	/* out: destination savepoint */
{
	const ib_rbt_node_t*	node;
	const ib_rbt_t*		tables;

	tables = src->tables;

	for (node = rbt_first(tables); node; node = rbt_next(tables, node)) {

		fts_trx_table_t*	ftt_dst;
		const fts_trx_table_t*	ftt_src;

		ftt_src = *rbt_value(const fts_trx_table_t*, node);

		ftt_dst = fts_trx_table_clone(ftt_src);

		rbt_insert(dst->tables, &ftt_dst->table->id, &ftt_dst);
	}
}

/********************************************************************
Take a FTS savepoint. */

void
fts_savepoint_take(
/*===============*/
					/* out: DB_SUCCESS or error code */
	trx_t*		trx,		/* in: transaction */
	const char*	name)		/* in: savepoint name */
{
	mem_heap_t*		heap;
	fts_trx_t*		fts_trx;
	fts_savepoint_t*	savepoint;
	fts_savepoint_t*	last_savepoint;

	ut_a(name != NULL);

	fts_trx = trx->fts_trx;
	heap = fts_trx->heap;

	/* The implied savepoint must exist. */
	ut_a(ib_vector_size(fts_trx->savepoints) > 0);

	last_savepoint = ib_vector_last(fts_trx->savepoints);
	savepoint = fts_savepoint_create(fts_trx->savepoints, name, heap);

	if (last_savepoint->tables != NULL) {
		fts_savepoint_copy(last_savepoint, savepoint);
	}
}

/********************************************************************
Lookup a savepoint instance by name. */
UNIV_INLINE
ulint
fts_savepoint_lookup(
/*==================*/
					/* out: ULINT_UNDEFINED if not found */
	ib_vector_t*	savepoints,	/* in: savepoints */
	const char*	name)		/* in: savepoint name */
{
	ulint			i;

	ut_a(ib_vector_size(savepoints) > 0);

	for (i = 1; i < ib_vector_size(savepoints); ++i) {
		fts_savepoint_t*	savepoint;

		savepoint = ib_vector_get(savepoints, i);

		if (strcmp(name, savepoint->name) == 0) {
			return(i);
		}
	}

	return(ULINT_UNDEFINED);
}

/********************************************************************
Release the savepoint data identified by  name. All savepoints created
after the named savepoint are also released. */

void
fts_savepoint_release(
/*==================*/
					/* out: DB_SUCCESS or error code */
	trx_t*		trx,		/* in: transaction */
	const char*	name)		/* in: savepoint name */
{
	ulint			i;
	fts_savepoint_t*	prev;
	ib_vector_t*		savepoints;
	ulint			top_of_stack = 0;

	ut_a(name != NULL);

	savepoints = trx->fts_trx->savepoints;

	ut_a(ib_vector_size(savepoints) > 0);

	prev = ib_vector_get(savepoints, top_of_stack);

	/* Skip the implied savepoint (first element). */
	for (i = 1; i < ib_vector_size(savepoints); ++i) {
		fts_savepoint_t*	savepoint;

		savepoint = ib_vector_get(savepoints, i);

		/* Even though we release the resources that are part
		of the savepoint, we don't (always) actually delete the
		entry.  We simply set the savepoint name to NULL. Therefore
		we have to skip deleted/released entries. */
		if (savepoint->name != NULL
		    && strcmp(name, savepoint->name) == 0) {

			fts_savepoint_t*	last;
			fts_savepoint_t		temp;

			last = ib_vector_last(savepoints);

			/* Swap the entries. */
			memcpy(&temp, last, sizeof(temp));
			memcpy(last, prev, sizeof(*last));
			memcpy(prev, &temp, sizeof(prev));
			break;

		/* Track the previous savepoint instance that will
		be at the top of the stack after the release. */
		} else if (savepoint->name != NULL) {
			/* We need to delete all entries
			greater than this element. */
			top_of_stack = i;

			prev = savepoint;
		}
	}

	/* Only if we found and element to release. */
	if (i < ib_vector_size(savepoints)) {

		ut_a(top_of_stack < ib_vector_size(savepoints));

		/* Skip the implied savepoint. */
		for (i = ib_vector_size(savepoints) - 1;
		     i > top_of_stack;
		     --i) {

			fts_savepoint_t*	savepoint;

			savepoint = ib_vector_get(savepoints, i);

			/* Skip savepoints that were released earlier. */
			if (savepoint->name != NULL) {
				savepoint->name = NULL;
				fts_savepoint_free(savepoint);
			}

			ib_vector_pop(savepoints);
		}

		/* Make sure we don't delete the implied savepoint. */
		ut_a(ib_vector_size(savepoints) > 0);

		/* This must hold. */
		ut_a(ib_vector_size(savepoints) == (top_of_stack + 1));
	}
}

/********************************************************************
Rollback to savepoint indentified by name. */

void
fts_savepoint_rollback(
/*===================*/
					/* out: DB_SUCCESS or error code */
	trx_t*		trx,		/* in: transaction */
	const char*	name)		/* in: savepoint name */
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

			savepoint = ib_vector_pop(savepoints);

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

		savepoint = ib_vector_last(savepoints);

		while (ib_vector_size(savepoints) > 1
		       && savepoint->name == NULL) {

			ib_vector_pop(savepoints);

			savepoint = ib_vector_last(savepoints);
		}

		/* Make sure we don't delete the implied savepoint. */
		ut_a(ib_vector_size(savepoints) > 0);
	}
}

/********************************************************************
Check if a table is an FTS auxiliary table name. */
static
ibool
fts_is_aux_table_name(
/*==================*/
					/* out: TRUE if the name matches an
					auxiliary table name pattern */
	fts_sys_table_t*table,		/* out: table info */
	const char*	name,		/* in: table name */
	ulint		len)		/* in: length of table name */
{
	char*		ptr;
	const char*	end = name + len;

	ptr = memchr(name, '/', len);

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
			return(FALSE);
		}

		/* Skip the table id. */
		ptr = memchr(ptr, '_', len);

		if (ptr == NULL) {
			return(FALSE);
		}

		/* Skip the underscore. */
		++ptr;
		ut_a(end > ptr);
		len = end - ptr;

		/* First search the common table suffix array. */
		for (i = 0; fts_common_tables[i] != NULL; ++i) {

			if (strncmp(ptr, fts_common_tables[i], len) == 0) {
				return(TRUE);
			}
		}

		/* Try and read the index id. */
		if (!fts_read_object_id(&table->index_id, ptr)) {
			return(FALSE);
		}

		/* Skip the table id. */
		ptr = memchr(ptr, '_', len);

		if (ptr == NULL) {
			return(FALSE);
		}

		/* Skip the underscore. */
		++ptr;
		ut_a(end > ptr);
		len = end - ptr;

		/* Search the FT index specific array. */
		for (i = 0; fts_index_selector[i].ch; ++i) {

			if (strncmp(ptr, fts_get_suffix(i), len) == 0) {
				return(TRUE);
			}
		}

		/* Other FT index specific table(s). */
		if (strncmp(ptr, "DOC_ID", len) == 0) {
			return(TRUE);
		}
	}

	return(FALSE);
}

/********************************************************************
Callback function to read a single table ID column. */
static
ibool
fts_read_tables(
/*============*/
					/* out: Always return TRUE */
	void*		row,		/* in: sel_node_t* */
	void*		user_arg)	/* in: pointer to ib_vector_t */
{
	int		i;
	fts_sys_table_t*table;
	mem_heap_t*	heap;
	ibool		done = FALSE;
	ib_vector_t*	tables = user_arg;
	sel_node_t*	sel_node = row;
	que_node_t*	exp = sel_node->select_list;

	/* Must be a heap allocated vector. */
	ut_a(tables->allocator->arg != NULL);

	/* We will use this heap for allocating strings. */
	heap = tables->allocator->arg;
	table = ib_vector_push(tables, NULL);

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

			if (!fts_is_aux_table_name(table, data, len)) {
				ib_vector_pop(tables);
				done = TRUE;
				break;
			}

			table->name = mem_heap_dup(heap, data, len + 1);
			table->name[len] = '\0';
			printf("Found [%.*s]\n", (int) len, table->name);
			break;

		case 1: /* ID */
			ut_a(len == 8);
			table->id = mach_read_from_8(data);
			break;

		default:
			ut_error;
		}
	}

	return(TRUE);
}

/*************************************************************************
Check and drop all orphaned FTS auxiliary tables, those that don't have
a parent table or FTS index defined on them. */
static
ulint
fts_check_and_drop_orphaned_tables(
/*===============================*/
						/* out: DB_SUCCESS or error
						code */
	trx_t*		trx,			/* in: transaction */
	ib_vector_t*	tables)			/* in: tables to check */
{
	ulint		i;
	ulint		error = DB_SUCCESS;

	for (i = 0; i < ib_vector_size(tables); ++i) {
		const dict_table_t*	table;
		fts_sys_table_t*	sys_table;
		ibool			drop = FALSE;

		sys_table = ib_vector_get(tables, i);

		table = dict_table_get_on_id(sys_table->parent_id, trx);

		if (table == NULL || table->fts == NULL) {

			drop = TRUE;

		} else if (sys_table->index_id != 0) {
			ulint		j;
			index_id_t	id;
			fts_t*	fts;

			drop = TRUE;
			fts = table->fts;
			id = sys_table->index_id;

			/* Search for the FT index in the table's list. */
			for (j = 0; j < ib_vector_size(fts->indexes); ++j) {
				const dict_index_t*	index;

				index = ib_vector_getp_const(fts->indexes, j);

				if (index->id == id) {

					drop = FALSE;
					break;
				}
			}
		}

		if (drop) {
			ut_print_timestamp(stderr);
			fprintf(stderr, "  InnoDB: Warning: Parent table of "
				"FT auxiliary table %s not found.\n",
				sys_table->name);

			/* We ignore drop errors. */
			fts_drop_table(trx, sys_table->name);
		}
	}

	return(error);
}

/*************************************************************************
Drop all orphaned FTS auxiliary tables, those that don't have a parent
table or FTS index defined on them. */

void
fts_drop_orphaned_tables(void)
/*==========================*/
{
	trx_t*		trx;
	pars_info_t*	info;
	mem_heap_t*	heap;
	que_t*		graph;
	ib_vector_t*	tables;
	ib_alloc_t*	heap_alloc;
	ulint		error = DB_SUCCESS;

	heap = mem_heap_create(1024);
	heap_alloc = ib_heap_allocator_create(heap);

	/* We store the table ids of all the FTS indexes that were found. */
	tables = ib_vector_create(heap_alloc, sizeof(fts_sys_table_t), 128);

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
		" SELECT NAME, ID "
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
			error = fts_check_and_drop_orphaned_tables(trx, tables);
		}

		if (error == DB_SUCCESS) {
			fts_sql_commit(trx);
			break;				/* Exit the loop. */
		} else {
			ib_vector_reset(tables);

			fts_sql_rollback(trx);

			ut_print_timestamp(stderr);

			if (error == DB_LOCK_WAIT_TIMEOUT) {
				fprintf(stderr, "  InnoDB: Warning: lock wait "
					"timeout reading SYS_TABLES. "
					"Retrying!\n");

				trx->error_state = DB_SUCCESS;
			} else {
				fprintf(stderr, "  InnoDB: Error: %lu "
					"while reading SYS_TABLES.\n",
					error);

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
}

/******************************************************************//**
Check whether user supplied stopword table is of the right format.
Caller is responsible to hold dictionary locks.
@return TRUE if the table qualifies */
UNIV_INTERN
ibool
fts_valid_stopword_table(
/*=====================*/
        const char*	stopword_table_name)	/*!< in: Stopword table
						name */
{
	dict_table_t*	table;

	if (!stopword_table_name) {
		return(FALSE);
	}

	table = dict_table_get_low(stopword_table_name);

	if (!table) {
		fprintf(stderr,
			"InnoDB: user stopword table %s does not exist.\n",
			stopword_table_name);

		return(FALSE);
	} else {
		dict_col_t*     col;
		const char*     col_name;

		col_name = dict_table_get_col_name(table, 0);

		if (ut_strcmp(col_name, "value")) {
			fprintf(stderr,
				"InnoDB: invalid column name for stopword "
				"table %s. Its first column must be named as "
				"'value'.\n", stopword_table_name);

			return(FALSE);
		}

		col = dict_table_get_nth_col(table, 0);

		if (col->mtype != DATA_VARCHAR) {
			fprintf(stderr,
				"InnoDB: invalid column type for stopword "
				"table %s. Its first column must be of "
				"varchar type\n", stopword_table_name);

			return(FALSE);
		}
	}

	return(TRUE);
}

/****************************************************************//**
This function loads the stopword into the FTS cache. It also
records/fetches stopword configuration to/from FTS configure
table, depending on whether we are creating or reloading the
FTS.
@return TRUE if load operation is successful */
UNIV_INTERN
ibool
fts_load_stopword(
/*==============*/
	const dict_table_t*
			table,			/*!< in: Table with FTS */
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
	ulint		error = DB_SUCCESS;
	ulint		use_stopword;
	fts_cache_t*	cache;
	const char*	stopword_to_use;
	trx_t*		trx;
	byte		str_buffer[FTS_MAX_UTF8_WORD_LEN + 1];

        fts_table.suffix = "CONFIG";
        fts_table.type = FTS_COMMON_TABLE;
        fts_table.table_id = table->id;
        fts_table.parent = table->name;

	cache = table->fts->cache;

        trx = trx_allocate_for_background();
        trx->op_info = "upload FTS stopword";

	/* First check whether stopword filtering is turned off */
	if (reload) {
		error = fts_config_get_ulint(trx, &fts_table,
					     FTS_USE_STOPWORD,
					     &use_stopword);
	} else {
		use_stopword = (ulint)stopword_is_on;

		error = fts_config_set_ulint(trx, &fts_table, FTS_USE_STOPWORD,
					     use_stopword);
	}

	if (error != DB_SUCCESS) {
		goto cleanup;
	}

	/* If stopword is turned off, no need to continue to load the
	stopword into cache */
	if (!use_stopword) {
		cache->stopword_info.status = STOPWORD_OFF;
		goto cleanup;
	}

	if (reload) {
		/* Fetch the stopword table name from FTS config
		table */
		str.utf8 = str_buffer;
		str.len = sizeof(str_buffer) - 1;

		error = fts_config_get_value(trx, &fts_table,
					     FTS_STOPWORD_TABLE_NAME,
					     &str);
		if (error != DB_SUCCESS) {
			goto cleanup;
		}

		stopword_to_use = (const char*) str.utf8;

	} else {
		stopword_to_use = (session_stopword_table)
					? session_stopword_table
					: global_stopword_table;

	}

	if (stopword_to_use
	    && fts_load_user_stopword(stopword_to_use,
				      &cache->stopword_info)) {
		/* Save the stopword table name to the configure
		table */
		if (!reload) {
			str.utf8 = (byte*) stopword_to_use;
			str.len = ut_strlen(stopword_to_use);
			error = fts_config_set_value(trx, &fts_table,
						     FTS_STOPWORD_TABLE_NAME,
						     &str);
		}
	} else {
		/* Load system default stopword list */
		fts_load_default_stopword(&cache->stopword_info);
	}

cleanup:
	if (error == DB_SUCCESS) {
		fts_sql_commit(trx);
	} else {
		fts_sql_rollback(trx);
	}

	trx_free_for_background(trx);

	return(error == DB_SUCCESS);
}
/****************************************************************//**
This function loads the documents in "ADDED" table into FTS cache,
it also loads the stopword info into the FTS cache.
@return DB_SUCCESS if all OK */
UNIV_INTERN
ibool
fts_init_index(
/*===========*/
	dict_table_t*	table)		/*!< in: Table with FTS */
{
	fts_sync_t	sync;
	ulint		error;

	memset(&sync, 0, sizeof(sync));
	sync.table = table;

	fts_update_max_cache_size(&sync);

	/* Load Doc IDs in the ADDED table, parse them and add to
	index cache */	
	error = fts_load_from_added(&sync);

	if (error == DB_SUCCESS
	    && (table->fts->cache->stopword_info.status & STOPWORD_NOT_INIT)) {
		fts_load_stopword(table, NULL, NULL, TRUE, TRUE);
	}

	return(error);
}
