#ifndef INNOBASE_FTS0PRIV_H
#define INNOBASE_FTS0PRIV_H

#include "dict0dict.h"
#include "pars0pars.h"
#include "que0que.h"
#include "que0types.h"
#include "fts0types.h"

/* The various states of the FTS sub system pertaining to a table with
FTS indexes defined on it. */
enum fts_table_state_enum {
					/* This must be 0 since we insert
					a hard coded '0' at create time
					to the config table */
	FTS_TABLE_STATE_RUNNING = 0,	/* Auxiliary tables created OK */
	FTS_TABLE_STATE_OPTIMIZING,	/* This is a substate of RUNNING */
	FTS_TABLE_STATE_DELETED		/* All aux tables to be dropped when
					it's safe to do so */
};

typedef enum fts_table_state_enum fts_table_state_t;

/* The default time to wait for the background thread (in microsecnds). */
#define FTS_MAX_BACKGROUND_THREAD_WAIT		10000

/* Maximum number of iterations to wait before we complain */
#define FTS_BACKGROUND_THREAD_WAIT_COUNT	1000

/* The maximum length of the config table's value column in bytes */
#define FTS_MAX_CONFIG_NAME_LEN			64

/* The maximum length of the config table's value column in bytes */
#define FTS_MAX_CONFIG_VALUE_LEN		1024

/* Approx. upper limit of ilist length in bytes. */
#define FTS_ILIST_MAX_SIZE			(64 * 1024)

/* FTS config table name parameters */

/* The number of seconds after which an OPTIMIZE run will stop */
#define FTS_OPTIMIZE_LIMIT_IN_SECS	"optimize_checkpoint_limit"

/* The next doc id */
#define FTS_NEXT_DOC_ID			"next_doc_id"

/* The last word that was OPTIMIZED */
#define FTS_LAST_OPTIMIZED_WORD		"last_optimized_word"

/* Total number of documents that have been deleted. The next_doc_id
minus this count gives us the total number of documents. */
#define FTS_TOTAL_DELETED_COUNT		"deleted_doc_count"

/* Total number of words parsed from all documents */
#define FTS_TOTAL_WORD_COUNT		"total_word_count"

/* Start of optimize of an FTS index */
#define FTS_OPTIMIZE_START_TIME		"optimize_start_time"

/* End of optimize for an FTS index */
#define FTS_OPTIMIZE_END_TIME		"optimize_end_time"

/* User specified stopword table name */
#define	FTS_STOPWORD_TABLE_NAME		"stopword_table_name"

/* Whether to use (turn on/off) stopword */
#define	FTS_USE_STOPWORD		"use_stopword"

/* State of the FTS system for this table. It can be one of
 RUNNING, OPTIMIZING, DELETED. */
#define FTS_TABLE_STATE			"table_state"

/* The minimum length of an FTS auxiliary table names's id component
e.g., For an auxiliary table name

	FTS_<TABLE_ID>_SUFFIX

This constant is for the minimum length required to store the <TABLE_ID>
component.
*/
#define FTS_AUX_MIN_TABLE_ID_LENGTH	48

/* Maximum length of an integer stored in the config table value column. */
#define FTS_MAX_INT_LEN			32

/********************************************************************
Parse an SQL string. %s is replaced with the table's id. */

que_t*
fts_parse_sql(
/*==========*/
					/* out: DB_SUCCESS or error code */
	fts_table_t*	fts_table,	/* in: FTS aux table */
	pars_info_t*	info,		/* in: info struct, or NULL */
	const char*	sql);		/* in: SQL string to evaluate */
/********************************************************************
Evaluate a parsed SQL statement.*/

ulint
fts_eval_sql(
/*=========*/
					/* out: DB_SUCCESS or error code */
	trx_t*		trx,		/* in: transaction */
	que_t*		graph);		/* in: Parsed statement */
/********************************************************************
Construct the name of an ancillary FTS table for the given table. */

char*
fts_get_table_name(
/*===================*/
					/* out, own: table name,
					must be freed with mem_free() */
	const fts_table_t*
			fts_table);	/* in: FTS aux table info */
/********************************************************************
Construct the column specification part of the SQL string for selecting the
indexed FTS columns for the given table. Adds the necessary bound
ids to the given 'info' and returns the SQL string. Examples:

One indexed column named "text":

 "$sel0",
 info/ids: sel0 -> "text"

Two indexed columns named "subject" and "content":

 "$sel0, $sel1",
 info/ids: sel0 -> "subject", sel1 -> "content",
*/

const char*
fts_get_select_columns_str(
/*=======================*/
					/* out: heap-allocated WHERE string */
	dict_index_t*	index,		/* in: FTS index */
	pars_info_t*	info,		/* in/out: parser info */
	mem_heap_t*	heap);		/* in: memory heap */
/*************************************************************//**
Fetch document (= a single row's indexed text) with the given
document id.
@return: DB_SUCCESS if fetch is successful, else error */
UNIV_INTERN
ulint
fts_doc_fetch_by_doc_id(
/*====================*/
	fts_get_doc_t*	get_doc,	/*!< in: state */
	doc_id_t	doc_id,		/*!< in: id of document to fetch */
	dict_index_t*	index_to_use,	/*!< in: caller supplied FTS index */
	fts_sql_callback
			callback,	/*!< in: callback to read
					records */
	void*		arg);		/*!< in: callback arg */

/*******************************************************************//**
Callback function for fetch that stores the text of an FTS document,
converting each column to UTF-16.
@return: always returns NULL */
UNIV_INTERN
ibool
fts_add_fetch_document(
/*===================*/
	void*		row,		/* in: sel_node_t* */
	void*		user_arg);	/* in: fts_doc_t* */
/********************************************************************
Write out a single word's data as new entry/entries in the INDEX table. */

ulint
fts_write_node(
/*===========*/
	trx_t*		trx,		/* in: transaction */
	que_t**		graph,		/* in: query graph */
	fts_table_t*	fts_table,	/* in: the FTS aux index */
	fts_string_t*	word,		/* in: word in UTF-8 */
	fts_node_t*	node);		/* in: node columns */
/*******************************************************************//**
Tokenize a document. */
UNIV_INTERN
void
fts_tokenize_document(
/*==================*/
	fts_doc_t*	doc,		/*!< in/out: document to
					tokenize */
	fts_doc_t*	result);	/*!< out: if provided, save
					result tokens here */
/********************************************************************
Create a new empty document. */

fts_doc_t*
fts_doc_init(
/*=========*/
					/* out, own: new document */
	fts_doc_t*	doc);		/* in: doc to initialize */
/********************************************************************
Do a binary search for a doc id in the array.*/

int
fts_bsearch(
/*========*/
					/* out: +ve index if found -ve
					index where it should be
					inserted if not found */
	fts_update_t*	array,		/* in: array to sort */
	int		lower,		/* in: lower bound of array*/
	int		upper,		/* in: upper bound of array*/
	doc_id_t	doc_id);	/* in: doc id to lookup */
/********************************************************************
Free document. */

void
fts_doc_free(
/*=========*/
	fts_doc_t*	doc);		/* in: document */
/********************************************************************
Free fts_optimizer_word_t instanace.*/

void
fts_word_free(
/*==========*/
					/* out: void*/
	fts_word_t*	word);		/* in: instance to free.*/
/********************************************************************
Read the rows from the FTS index*/

ulint
fts_index_fetch_nodes(
/*==================*/
					/* out: vector of rows fetched*/
	trx_t*		trx,		/* in: transaction */
	que_t**		graph,		/* in: prepared statement */
	fts_table_t*	fts_table,	/* in: FTS aux table */
	const fts_string_t*
			word,		/* in: the word to fetch */
	fts_fetch_t*	fetch);		/* in: fetch callback.*/
/********************************************************************
Create a fts_optimizer_word_t instance. */

fts_word_t*
fts_word_init(
/*==========*/
					/* out: new instance */
	fts_word_t*	word,		/* in: word to initialize */
	byte*		utf8,		/* in: UTF-8 string */
	ulint		len);		/* in: length of string in bytes */
/********************************************************************
Compare two fts_trx_table_t instances, we actually compare the
table id's here. */
UNIV_INLINE
int
fts_trx_table_cmp(
/*==============*/
					/* out:
					< 0 if n1 < n2,
					0 if n1 == n2,
					> 0 if n1 > n2 */
	const void*	v1,		/* in: id1 */
	const void*	v2);		/* in: id2 */
/********************************************************************
Compare a table id with a trx_table_t table id. */
UNIV_INLINE
int
fts_trx_table_id_cmp(
/*=================*/
					/* out:
					< 0 if n1 < n2,
					0 if n1 == n2,
					> 0 if n1 > n2 */
	const void*	p1,		/* in: id1 */
	const void*	p2);		/* in: id2 */
/********************************************************************
Commit a transaction. */

ulint
fts_sql_commit(
/*===========*/
					/* out: DB_SUCCESS if all OK */
	trx_t*		trx);		/* in: transaction */
/********************************************************************
Rollback a transaction. */

ulint
fts_sql_rollback(
/*=============*/
					/* out: DB_SUCCESS if all OK */
	trx_t*		trx);		/* in: transaction */
/********************************************************************
Parse an SQL string. %s is replaced with the table's id. Don't acquire
the dict mutex.*/

que_t*
fts_parse_sql_no_dict_lock(
/*=======================*/
					/* out: query graph */
	fts_table_t*	fts_table,	/* in: table with FTS index */
	pars_info_t*	info,		/* in: parser info */
	const char*	sql);		/* in: SQL string to evaluate */
/********************************************************************
Get value from config table. The caller must ensure that enough
space is allocated for value to hold the column contents.*/

ulint
fts_config_get_value(
/*=================*/
					/* out: DB_SUCCESS or error code */
	trx_t*		trx,		/* transaction */
	fts_table_t*	fts_table,	/* in: the indexed FTS table */
	const char*	name,		/* in: get config value for
					this parameter name */
	fts_string_t*	value);		/* out: value read from config table */
/********************************************************************
Get value specific to an FTS index from the config table. The caller
must ensure that enough space is allocated for value to hold the
column contents. */

ulint
fts_config_get_index_value(
/*=======================*/
					/* out: DB_SUCCESS or error
					code */
	trx_t*		trx,		/* transaction */
	dict_index_t*	index,		/* in: index */
	const char*	param,		/* in: get config value for
					this parameter name */
	fts_string_t*	value);		/* out: value read from
					config table */
/********************************************************************
Set the value in the config table for name. */

ulint
fts_config_set_value(
/*=================*/
					/* out: DB_SUCCESS or error code */
	trx_t*		trx,		/* transaction */
	fts_table_t*	fts_table,	/* in: the indexed FTS table */
	const char*	name,		/* in: get config value for
					this parameter name */
	const fts_string_t*
			value);		/* in: value to update */
/****************************************************************//**
Set an ulint value in the config table.
@return DB_SUCCESS if all OK else error code */
UNIV_INTERN
ulint
fts_config_set_ulint(
/*=================*/
	trx_t*		trx,		/* in: transaction */
	fts_table_t*	fts_table,	/* in: the indexed FTS table */
	const char*	name,		/* in: param name */
	ulint		int_value);	/* in: value */

/********************************************************************
Set the value specific to an FTS index in the config table. */

ulint
fts_config_set_index_value(
/*=======================*/
					/* out: DB_SUCCESS or error
					code */
	trx_t*		trx,		/* transaction */
	dict_index_t*	index,		/* in: index */
	const char*	param,		/* in: get config value for
					this parameter name */
	fts_string_t*	value);		/* out: value read from
					config table */
/********************************************************************
Increment the value in the config table for column name. */

ulint
fts_config_increment_value(
/*=======================*/
					/* out: DB_SUCCESS or error
					code */
	trx_t*		trx,		/* transaction */
	fts_table_t*	fts_table,	/* in: the indexed FTS table */
	const char*	name,		/* in: increment config value
					for this parameter name */
	ulint		delta);		/* in: increment by this much */
/********************************************************************
Increment the per index value in the config table for column name. */

ulint
fts_config_increment_index_value(
/*=============================*/
					/* out: DB_SUCCESS or error
					code */
	trx_t*		trx,		/* transaction */
	dict_index_t*	index,		/* in: FTS index */
	const char*	name,		/* in: increment config value
					for this parameter name */
	ulint		delta);		/* in: increment by this much */
/********************************************************************
Get an ulint value from the config table. */

ulint
fts_config_get_index_ulint(
/*=======================*/
					/* out: DB_SUCCESS if all OK
					else error code */
	trx_t*		trx,		/* in: transaction */
	dict_index_t*	index,		/* in: FTS index */
	const char*	name,		/* in: param name */
	ulint*		int_value);	/* out: value */
/********************************************************************
Set an ulint value int the config table. */

ulint
fts_config_set_index_ulint(
/*=======================*/
					/* out: DB_SUCCESS if all OK
					else error code */
	trx_t*		trx,		/* in: transaction */
	dict_index_t*	index,		/* in: FTS index */
	const char*	name,		/* in: param name */
	ulint		int_value);	/* in: value */
/********************************************************************
Get an ulint value from the config table. */

ulint
fts_config_get_ulint(
/*=================*/
					/* out: DB_SUCCESS if all OK
					else error code */
	trx_t*		trx,		/* in: transaction */
	fts_table_t*	fts_table,	/* in: the indexed FTS table */
	const char*	name,		/* in: param name */
	ulint*		int_value);	/* out: value */
/********************************************************************
Search cache for word. */

const ib_vector_t*
fts_cache_find_word(
/*================*/
					/* out: the word node vector if found
					else NULL */
	const fts_index_cache_t*
			index_cache,	/* in: cache to search */

	const fts_string_t*
			text);		/* in: word to search for */
/********************************************************************
Check cache for deleted doc id. */

ibool
fts_cache_is_deleted_doc_id(
/*========================*/
					/* out: TRUE if deleted */
	const fts_cache_t*
			cache,		/* in: cache ito search */
	doc_id_t	doc_id);	/* in: doc id to search for */
/********************************************************************
Append deleted doc ids to vector and sort the vector. */

void
fts_cache_append_deleted_doc_ids(
/*=============================*/
	const fts_cache_t*
			cache,		/* in: cache to use */
	ib_vector_t*	vector);	/* in: append to this vector */
/********************************************************************
Wait for the background thread to start. We poll to detect change
of state, which is acceptable, since the wait should happen only
once during startup. */

ibool
fts_wait_for_background_thread_to_start(
/*====================================*/
					/* out: true if the thread
					started else FALSE (i.e timed out) */
	dict_table_t*	table,		/* in: table to which the thread
					is attached */
	ulint		max_wait);	/* in: time in microseconds, if set
					to 0 then it disables timeout
					checking */
/*********************************************************************//**
Get the total number of documents in the FTS.
@return estimated number of rows in the table */
UNIV_INTERN
ulint
fts_get_total_document_count(
/*=========================*/
	dict_table_t*	table);		/*!< in: table instance */
/********************************************************************
Get the total number of words in the FTS for a particular FTS index. */

ulint
fts_get_total_word_count(
/*=====================*/
					/* out: DB_SUCCESS if all OK
					else error code */
	trx_t*		trx,		/* in: transaction */
	dict_index_t*	index,		/* in: for this index */
	ulint*		total);		/* out: total words */
/********************************************************************
Search the index specific cache for a particular FTS index. */

const fts_index_cache_t*
fts_find_index_cache(
/*================*/
					/* out: the index specific
					cache else NULL */
	const fts_cache_t*
			cache,		/* in: cache to search */
	const dict_index_t*
			index);		/* in: index to search for */
/********************************************************************
Write the table id to the given buffer (including final NUL). Buffer must be
at least FTS_AUX_MIN_TABLE_ID_LENGTH bytes long.
@return	number of bytes written */
UNIV_INLINE
int
fts_write_object_id(
/*================*/
	ullint		id,		/* in: a table/index id */
	char*		str);		/* in: buffer to write the id to */
/********************************************************************
Read the table id from the string generated by fts_write_object_id(). */
UNIV_INLINE
ibool
fts_read_object_id(
/*===============*/
					/* out: TRUE if parse successful */
	ullint*		id,		/* out: a dulint id */
	const char*	str);		/* in: buffer to read from */
/********************************************************************
Select the FTS aux index using the character. */
UNIV_INLINE
ulint
fts_select_index(
/*=============*/
	byte		ch);		/* in: First character of a word */
/********************************************************************
Get the table id. */

int
fts_get_table_id(
/*=============*/
					/* out: number of bytes written */
	const fts_table_t*
			fts_table,	/* in: FTS Auxiliary table */
	char*		table_id);	/* out: table id, must be at least
					FTS_AUX_MIN_TABLE_ID_LENGTH bytes
					long */
/********************************************************************
Add the table to add to the OPTIMIZER's list. */

void
fts_optimize_add_table(
/*===================*/
	dict_table_t*	table);		/* in: table to add */
/********************************************************************
Remove the table from the OPTIMIZER's list. */

void
fts_optimize_remove_table(
/*======================*/
	dict_table_t*	table);		/* in: table to remove */
/********************************************************************
Optimize a table. */

void
fts_optimize_do_table(
/*==================*/
	dict_table_t*	table);		/* in: table to optimize */
/********************************************************************
Construct the prefix name of an FTS table. */

char*
fts_get_table_name_prefix(
/*======================*/
					/* out, own: table name,
					must be freed with mem_free() */
	const fts_table_t*
			fts_table);	/* in: Auxiliary table type */

UNIV_INTERN
void
fts_cache_node_add_positions(
/*=========================*/
	fts_cache_t*	cache,		/* in: cache */
	fts_node_t*	node,		/* in: word node */
	doc_id_t	doc_id,		/* in: doc id */
	ib_vector_t*	positions);	/* in: fts_token_t::positions */

#endif /* INNOBASE_FTS0PRIV_H */
