/*****************************************************************************

Copyright (c) 2011, 2018, Oracle and/or its affiliates. All Rights Reserved.

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

/******************************************************************//**
@file include/fts0priv.h
Full text search internal header file

Created 2011/09/02 Sunny Bains
***********************************************************************/

#ifndef INNOBASE_FTS0PRIV_H
#define INNOBASE_FTS0PRIV_H

#include "univ.i"
#include "dict0dict.h"
#include "pars0pars.h"
#include "que0que.h"
#include "que0types.h"
#include "fts0types.h"

/* The various states of the FTS sub system pertaining to a table with
FTS indexes defined on it. */
enum fts_table_state_enum {
					/* !<This must be 0 since we insert
					a hard coded '0' at create time
					to the config table */

	FTS_TABLE_STATE_RUNNING = 0,	/*!< Auxiliary tables created OK */

	FTS_TABLE_STATE_OPTIMIZING,	/*!< This is a substate of RUNNING */

	FTS_TABLE_STATE_DELETED		/*!< All aux tables to be dropped when
					it's safe to do so */
};

typedef enum fts_table_state_enum fts_table_state_t;

/** The default time to wait for the background thread (in microsecnds). */
#define FTS_MAX_BACKGROUND_THREAD_WAIT		10000

/** Maximum number of iterations to wait before we complain */
#define FTS_BACKGROUND_THREAD_WAIT_COUNT	1000

/** The maximum length of the config table's value column in bytes */
#define FTS_MAX_CONFIG_NAME_LEN			64

/** The maximum length of the config table's value column in bytes */
#define FTS_MAX_CONFIG_VALUE_LEN		1024

/** Approx. upper limit of ilist length in bytes. */
#define FTS_ILIST_MAX_SIZE			(64 * 1024)

/** FTS config table name parameters */

/** The number of seconds after which an OPTIMIZE run will stop */
#define FTS_OPTIMIZE_LIMIT_IN_SECS	"optimize_checkpoint_limit"

/** The next doc id */
#define FTS_SYNCED_DOC_ID		"synced_doc_id"

/** The last word that was OPTIMIZED */
#define FTS_LAST_OPTIMIZED_WORD		"last_optimized_word"

/** Total number of documents that have been deleted. The next_doc_id
minus this count gives us the total number of documents. */
#define FTS_TOTAL_DELETED_COUNT		"deleted_doc_count"

/** Total number of words parsed from all documents */
#define FTS_TOTAL_WORD_COUNT		"total_word_count"

/** Start of optimize of an FTS index */
#define FTS_OPTIMIZE_START_TIME		"optimize_start_time"

/** End of optimize for an FTS index */
#define FTS_OPTIMIZE_END_TIME		"optimize_end_time"

/** User specified stopword table name */
#define	FTS_STOPWORD_TABLE_NAME		"stopword_table_name"

/** Whether to use (turn on/off) stopword */
#define	FTS_USE_STOPWORD		"use_stopword"

/** State of the FTS system for this table. It can be one of
 RUNNING, OPTIMIZING, DELETED. */
#define FTS_TABLE_STATE			"table_state"

/** The minimum length of an FTS auxiliary table names's id component
e.g., For an auxiliary table name

	FTS_<TABLE_ID>_SUFFIX

This constant is for the minimum length required to store the <TABLE_ID>
component.
*/
#define FTS_AUX_MIN_TABLE_ID_LENGTH	48

/** Maximum length of an integer stored in the config table value column. */
#define FTS_MAX_INT_LEN			32

/******************************************************************//**
Parse an SQL string. %s is replaced with the table's id.
@return query graph */
que_t*
fts_parse_sql(
/*==========*/
	fts_table_t*	fts_table,	/*!< in: FTS aux table */
	pars_info_t*	info,		/*!< in: info struct, or NULL */
	const char*	sql)		/*!< in: SQL string to evaluate */
	MY_ATTRIBUTE((warn_unused_result));

/******************************************************************//**
Evaluate a parsed SQL statement
@return DB_SUCCESS or error code */
dberr_t
fts_eval_sql(
/*=========*/
	trx_t*		trx,		/*!< in: transaction */
	que_t*		graph)		/*!< in: Parsed statement */
	MY_ATTRIBUTE((warn_unused_result));

/******************************************************************//**
Construct the name of an ancillary FTS table for the given table.
Caller must allocate enough memory(usually size of MAX_FULL_NAME_LEN)
for param 'table_name'. */
void
fts_get_table_name(
/*===============*/
	const fts_table_t*
			fts_table,	/*!< in: FTS aux table info */
	char*		table_name);	/*!< in/out: aux table name */

/******************************************************************//**
Construct the column specification part of the SQL string for selecting the
indexed FTS columns for the given table. Adds the necessary bound
ids to the given 'info' and returns the SQL string. Examples:

One indexed column named "text":

 "$sel0",
 info/ids: sel0 -> "text"

Two indexed columns named "subject" and "content":

 "$sel0, $sel1",
 info/ids: sel0 -> "subject", sel1 -> "content",
@return heap-allocated WHERE string */
const char*
fts_get_select_columns_str(
/*=======================*/
	dict_index_t*	index,		/*!< in: FTS index */
	pars_info_t*	info,		/*!< in/out: parser info */
	mem_heap_t*	heap)		/*!< in: memory heap */
	MY_ATTRIBUTE((warn_unused_result));

/** define for fts_doc_fetch_by_doc_id() "option" value, defines whether
we want to get Doc whose ID is equal to or greater or smaller than supplied
ID */
#define	FTS_FETCH_DOC_BY_ID_EQUAL	1
#define	FTS_FETCH_DOC_BY_ID_LARGE	2
#define	FTS_FETCH_DOC_BY_ID_SMALL	3

/*************************************************************//**
Fetch document (= a single row's indexed text) with the given
document id.
@return: DB_SUCCESS if fetch is successful, else error */
dberr_t
fts_doc_fetch_by_doc_id(
/*====================*/
	fts_get_doc_t*	get_doc,	/*!< in: state */
	doc_id_t	doc_id,		/*!< in: id of document to fetch */
	dict_index_t*	index_to_use,	/*!< in: caller supplied FTS index,
					or NULL */
	ulint		option,         /*!< in: search option, if it is
                                        greater than doc_id or equal */
	fts_sql_callback
			callback,	/*!< in: callback to read
					records */
	void*		arg);		/*!< in: callback arg */

/*******************************************************************//**
Callback function for fetch that stores the text of an FTS document,
converting each column to UTF-16.
@return always FALSE */
ibool
fts_query_expansion_fetch_doc(
/*==========================*/
	void*		row,		/*!< in: sel_node_t* */
	void*		user_arg);	/*!< in: fts_doc_t* */

/********************************************************************
Write out a single word's data as new entry/entries in the INDEX table.
@return DB_SUCCESS if all OK. */
dberr_t
fts_write_node(
/*===========*/
	trx_t*		trx,		/*!< in: transaction */
	que_t**		graph,		/*!< in: query graph */
	fts_table_t*	fts_table,	/*!< in: the FTS aux index */
	fts_string_t*	word,		/*!< in: word in UTF-8 */
	fts_node_t*	node)		/*!< in: node columns */
	MY_ATTRIBUTE((warn_unused_result));

/** Check fts token
1. for ngram token, check whether the token contains any words in stopwords
2. for non-ngram token, check if it's stopword or less than fts_min_token_size
or greater than fts_max_token_size.
@param[in]	token		token string
@param[in]	stopwords	stopwords rb tree
@param[in]	is_ngram	is ngram parser
@param[in]	cs		token charset
@retval true	if it is not stopword and length in range
@retval false	if it is stopword or length not in range */
bool
fts_check_token(
	const fts_string_t*	token,
	const ib_rbt_t*		stopwords,
	bool			is_ngram,
	const CHARSET_INFO*	cs);

/*******************************************************************//**
Tokenize a document. */
void
fts_tokenize_document(
/*==================*/
	fts_doc_t*	doc,		/*!< in/out: document to
					tokenize */
	fts_doc_t*	result,		/*!< out: if provided, save
					result tokens here */
	st_mysql_ftparser*	parser);/* in: plugin fts parser */

/*******************************************************************//**
Continue to tokenize a document. */
void
fts_tokenize_document_next(
/*=======================*/
	fts_doc_t*	doc,		/*!< in/out: document to
					tokenize */
	ulint		add_pos,	/*!< in: add this position to all
					tokens from this tokenization */
	fts_doc_t*	result,		/*!< out: if provided, save
					result tokens here */
	st_mysql_ftparser*	parser);/* in: plugin fts parser */

/******************************************************************//**
Initialize a document. */
void
fts_doc_init(
/*=========*/
	fts_doc_t*	doc);		/*!< in: doc to initialize */

/******************************************************************//**
Do a binary search for a doc id in the array
@return +ve index if found -ve index where it should be
        inserted if not found */
int
fts_bsearch(
/*========*/
	fts_update_t*	array,		/*!< in: array to sort */
	int		lower,		/*!< in: lower bound of array*/
	int		upper,		/*!< in: upper bound of array*/
	doc_id_t	doc_id)		/*!< in: doc id to lookup */
	MY_ATTRIBUTE((warn_unused_result));
/******************************************************************//**
Free document. */
void
fts_doc_free(
/*=========*/
	fts_doc_t*	doc);		/*!< in: document */

/******************************************************************//**
Free fts_optimizer_word_t instanace.*/
void
fts_word_free(
/*==========*/
	fts_word_t*	word);		/*!< in: instance to free.*/

/******************************************************************//**
Read the rows from the FTS inde
@return DB_SUCCESS or error code */
dberr_t
fts_index_fetch_nodes(
/*==================*/
	trx_t*		trx,		/*!< in: transaction */
	que_t**		graph,		/*!< in: prepared statement */
	fts_table_t*	fts_table,	/*!< in: FTS aux table */
	const fts_string_t*
			word,		/*!< in: the word to fetch */
	fts_fetch_t*	fetch);		/*!< in: fetch callback.*/

/******************************************************************//**
Create a fts_optimizer_word_t instance.
@return new instance */
fts_word_t*
fts_word_init(
/*==========*/
	fts_word_t*	word,		/*!< in: word to initialize */
	byte*		utf8,		/*!< in: UTF-8 string */
	ulint		len);		/*!< in: length of string in bytes */

/******************************************************************//**
Compare two fts_trx_table_t instances, we actually compare the
table id's here.
@return < 0 if n1 < n2, 0 if n1 == n2, > 0 if n1 > n2 */
UNIV_INLINE
int
fts_trx_table_cmp(
/*==============*/
	const void*	v1,		/*!< in: id1 */
	const void*	v2);		/*!< in: id2 */

/******************************************************************//**
Compare a table id with a trx_table_t table id.
@return < 0 if n1 < n2, 0 if n1 == n2, > 0 if n1 > n2 */
UNIV_INLINE
int
fts_trx_table_id_cmp(
/*=================*/
	const void*	p1,		/*!< in: id1 */
	const void*	p2);		/*!< in: id2 */

/******************************************************************//**
Commit a transaction.
@return DB_SUCCESS if all OK */
dberr_t
fts_sql_commit(
/*===========*/
	trx_t*		trx);		/*!< in: transaction */

/******************************************************************//**
Rollback a transaction.
@return DB_SUCCESS if all OK */
dberr_t
fts_sql_rollback(
/*=============*/
	trx_t*		trx);		/*!< in: transaction */

/******************************************************************//**
Parse an SQL string. %s is replaced with the table's id. Don't acquire
the dict mutex
@return query graph */
que_t*
fts_parse_sql_no_dict_lock(
/*=======================*/
	fts_table_t*	fts_table,	/*!< in: table with FTS index */
	pars_info_t*	info,		/*!< in: parser info */
	const char*	sql)		/*!< in: SQL string to evaluate */
	MY_ATTRIBUTE((warn_unused_result));

/******************************************************************//**
Get value from config table. The caller must ensure that enough
space is allocated for value to hold the column contents
@return DB_SUCCESS or error code */
dberr_t
fts_config_get_value(
/*=================*/
	trx_t*		trx,		/* transaction */
	fts_table_t*	fts_table,	/*!< in: the indexed FTS table */
	const char*	name,		/*!< in: get config value for
					this parameter name */
	fts_string_t*	value);		/*!< out: value read from
					config table */
/******************************************************************//**
Get value specific to an FTS index from the config table. The caller
must ensure that enough space is allocated for value to hold the
column contents.
@return DB_SUCCESS or error code */
dberr_t
fts_config_get_index_value(
/*=======================*/
	trx_t*		trx,		/*!< transaction */
	dict_index_t*	index,		/*!< in: index */
	const char*	param,		/*!< in: get config value for
					this parameter name */
	fts_string_t*	value)		/*!< out: value read from
					config table */
	MY_ATTRIBUTE((warn_unused_result));

/******************************************************************//**
Set the value in the config table for name.
@return DB_SUCCESS or error code */
dberr_t
fts_config_set_value(
/*=================*/
	trx_t*		trx,		/*!< transaction */
	fts_table_t*	fts_table,	/*!< in: the indexed FTS table */
	const char*	name,		/*!< in: get config value for
					this parameter name */
	const fts_string_t*
			value);		/*!< in: value to update */

/****************************************************************//**
Set an ulint value in the config table.
@return DB_SUCCESS if all OK else error code */
dberr_t
fts_config_set_ulint(
/*=================*/
	trx_t*		trx,		/*!< in: transaction */
	fts_table_t*	fts_table,	/*!< in: the indexed FTS table */
	const char*	name,		/*!< in: param name */
	ulint		int_value)	/*!< in: value */
	MY_ATTRIBUTE((warn_unused_result));

/******************************************************************//**
Set the value specific to an FTS index in the config table.
@return DB_SUCCESS or error code */
dberr_t
fts_config_set_index_value(
/*=======================*/
	trx_t*		trx,		/*!< transaction */
	dict_index_t*	index,		/*!< in: index */
	const char*	param,		/*!< in: get config value for
					this parameter name */
	fts_string_t*	value)		/*!< out: value read from
					config table */
	MY_ATTRIBUTE((warn_unused_result));

/******************************************************************//**
Increment the value in the config table for column name.
@return DB_SUCCESS or error code */
dberr_t
fts_config_increment_value(
/*=======================*/
	trx_t*		trx,		/*!< transaction */
	fts_table_t*	fts_table,	/*!< in: the indexed FTS table */
	const char*	name,		/*!< in: increment config value
					for this parameter name */
	ulint		delta)		/*!< in: increment by this much */
	MY_ATTRIBUTE((warn_unused_result));

/******************************************************************//**
Increment the per index value in the config table for column name.
@return DB_SUCCESS or error code */
dberr_t
fts_config_increment_index_value(
/*=============================*/
	trx_t*		trx,		/*!< transaction */
	dict_index_t*	index,		/*!< in: FTS index */
	const char*	name,		/*!< in: increment config value
					for this parameter name */
	ulint		delta);		/*!< in: increment by this much */

/******************************************************************//**
Get an ulint value from the config table.
@return DB_SUCCESS or error code */
dberr_t
fts_config_get_index_ulint(
/*=======================*/
	trx_t*		trx,		/*!< in: transaction */
	dict_index_t*	index,		/*!< in: FTS index */
	const char*	name,		/*!< in: param name */
	ulint*		int_value)	/*!< out: value */
	MY_ATTRIBUTE((warn_unused_result));

/******************************************************************//**
Set an ulint value int the config table.
@return DB_SUCCESS or error code */
dberr_t
fts_config_set_index_ulint(
/*=======================*/
	trx_t*		trx,		/*!< in: transaction */
	dict_index_t*	index,		/*!< in: FTS index */
	const char*	name,		/*!< in: param name */
	ulint		int_value)	/*!< in: value */
	MY_ATTRIBUTE((warn_unused_result));

/******************************************************************//**
Get an ulint value from the config table.
@return DB_SUCCESS or error code */
dberr_t
fts_config_get_ulint(
/*=================*/
	trx_t*		trx,		/*!< in: transaction */
	fts_table_t*	fts_table,	/*!< in: the indexed FTS table */
	const char*	name,		/*!< in: param name */
	ulint*		int_value);	/*!< out: value */

/******************************************************************//**
Search cache for word.
@return the word node vector if found else NULL */
const ib_vector_t*
fts_cache_find_word(
/*================*/
	const fts_index_cache_t*
			index_cache,	/*!< in: cache to search */
	const fts_string_t*
			text)		/*!< in: word to search for */
	MY_ATTRIBUTE((warn_unused_result));

/******************************************************************//**
Check cache for deleted doc id.
@return TRUE if deleted */
ibool
fts_cache_is_deleted_doc_id(
/*========================*/
	const fts_cache_t*
			cache,		/*!< in: cache ito search */
	doc_id_t	doc_id)		/*!< in: doc id to search for */
	MY_ATTRIBUTE((warn_unused_result));

/******************************************************************//**
Append deleted doc ids to vector and sort the vector. */
void
fts_cache_append_deleted_doc_ids(
/*=============================*/
	const fts_cache_t*
			cache,		/*!< in: cache to use */
	ib_vector_t*	vector);	/*!< in: append to this vector */
/******************************************************************//**
Wait for the background thread to start. We poll to detect change
of state, which is acceptable, since the wait should happen only
once during startup.
@return true if the thread started else FALSE (i.e timed out) */
ibool
fts_wait_for_background_thread_to_start(
/*====================================*/
	dict_table_t*	table,		/*!< in: table to which the thread
					is attached */
	ulint		max_wait);	/*!< in: time in microseconds, if set
					to 0 then it disables timeout
					checking */
#ifdef FTS_DOC_STATS_DEBUG
/******************************************************************//**
Get the total number of words in the FTS for a particular FTS index.
@return DB_SUCCESS or error code */
dberr_t
fts_get_total_word_count(
/*=====================*/
	trx_t*		trx,		/*!< in: transaction */
	dict_index_t*	index,		/*!< in: for this index */
	ulint*		total)		/*!< out: total words */
	MY_ATTRIBUTE((warn_unused_result));
#endif
/******************************************************************//**
Search the index specific cache for a particular FTS index.
@return the index specific cache else NULL */
fts_index_cache_t*
fts_find_index_cache(
/*================*/
	const fts_cache_t*
			cache,		/*!< in: cache to search */
	const dict_index_t*
			index)		/*!< in: index to search for */
	MY_ATTRIBUTE((warn_unused_result));

/******************************************************************//**
Write the table id to the given buffer (including final NUL). Buffer must be
at least FTS_AUX_MIN_TABLE_ID_LENGTH bytes long.
@return number of bytes written */
UNIV_INLINE
int
fts_write_object_id(
/*================*/
	ib_id_t		id,		/*!< in: a table/index id */
	char*		str,		/*!< in: buffer to write the id to */
	bool		hex_format MY_ATTRIBUTE((unused)));
					/*!< in: true for fixed hex format,
					false for old ambiguous format */

/******************************************************************//**
Read the table id from the string generated by fts_write_object_id().
@return TRUE if parse successful */
UNIV_INLINE
ibool
fts_read_object_id(
/*===============*/
	ib_id_t*	id,		/*!< out: a table id */
	const char*	str)		/*!< in: buffer to read from */
	MY_ATTRIBUTE((warn_unused_result));

/******************************************************************//**
Get the table id.
@return number of bytes written */
int
fts_get_table_id(
/*=============*/
	const fts_table_t*
			fts_table,	/*!< in: FTS Auxiliary table */
	char*		table_id)	/*!< out: table id, must be at least
					FTS_AUX_MIN_TABLE_ID_LENGTH bytes
					long */
	MY_ATTRIBUTE((warn_unused_result));

/******************************************************************//**
Construct the prefix name of an FTS table.
@return own: table name, must be freed with ut_free() */
char*
fts_get_table_name_prefix(
/*======================*/
	const fts_table_t*
			fts_table)	/*!< in: Auxiliary table type */
	MY_ATTRIBUTE((warn_unused_result));

/******************************************************************//**
Add node positions. */
void
fts_cache_node_add_positions(
/*=========================*/
	fts_cache_t*	cache,		/*!< in: cache */
	fts_node_t*	node,		/*!< in: word node */
	doc_id_t	doc_id,		/*!< in: doc id */
	ib_vector_t*	positions);	/*!< in: fts_token_t::positions */

/******************************************************************//**
Create the config table name for retrieving index specific value.
@return index config parameter name */
char*
fts_config_create_index_param_name(
/*===============================*/
	const char*		param,	/*!< in: base name of param */
	const dict_index_t*	index)	/*!< in: index for config */
	MY_ATTRIBUTE((warn_unused_result));

#ifndef UNIV_NONINL
#include "fts0priv.ic"
#endif

#endif /* INNOBASE_FTS0PRIV_H */
