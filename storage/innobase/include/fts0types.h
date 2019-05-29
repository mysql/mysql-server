/*****************************************************************************

Copyright (c) 2007, 2019, Oracle and/or its affiliates. All Rights Reserved.

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
@file include/fts0types.h
Full text search types file

Created 2007-03-27 Sunny Bains
*******************************************************/

#ifndef INNOBASE_FTS0TYPES_H
#define INNOBASE_FTS0TYPES_H

#include "univ.i"
#include "fts0fts.h"
#include "fut0fut.h"
#include "pars0pars.h"
#include "que0types.h"
#include "ut0byte.h"
#include "ut0rbt.h"

/** Types used within FTS. */
struct fts_que_t;
struct fts_node_t;

/** Callbacks used within FTS. */
typedef pars_user_func_cb_t fts_sql_callback;
typedef void (*fts_filter)(void*, fts_node_t*, void*, ulint len);

/** Statistics relevant to a particular document, used during retrieval. */
struct fts_doc_stats_t {
	doc_id_t	doc_id;		/*!< Document id */
	ulint		word_count;	/*!< Total words in the document */
};

/** It's main purpose is to store the SQL prepared statements that
are required to retrieve a document from the database. */
struct fts_get_doc_t {
	fts_index_cache_t*
			index_cache;	/*!< The index cache instance */

					/*!< Parsed sql statement */
	que_t*		get_document_graph;
	fts_cache_t*	cache;		/*!< The parent cache */
};

/** Since we can have multiple FTS indexes on a table, we keep a
per index cache of words etc. */
struct fts_index_cache_t {
	dict_index_t*	index;		/*!< The FTS index instance */

	ib_rbt_t*	words;		/*!< Nodes; indexed by fts_string_t*,
					cells are fts_tokenizer_word_t*.*/

	ib_vector_t*	doc_stats;	/*!< Array of the fts_doc_stats_t
					contained in the memory buffer.
					Must be in sorted order (ascending).
					The  ideal choice is an rb tree but
					the rb tree imposes a space overhead
					that we can do without */

	que_t**		ins_graph;	/*!< Insert query graphs */

	que_t**		sel_graph;	/*!< Select query graphs */
	CHARSET_INFO*	charset;	/*!< charset */
};

/** For supporting the tracking of updates on multiple FTS indexes we need
to track which FTS indexes need to be updated. For INSERT and DELETE we
update all fts indexes. */
struct fts_update_t {
	doc_id_t	doc_id;		/*!< The doc id affected */

	ib_vector_t*	fts_indexes;	/*!< The FTS indexes that need to be
					updated. A NULL value means all
					indexes need to be updated.  This
					vector is not allocated on the heap
					and so must be freed explicitly,
					when we are done with it */
};

/** Stop word control infotmation. */
struct fts_stopword_t {
	ulint		status;		/*!< Status of the stopword tree */
	ib_alloc_t*	heap;		/*!< The memory allocator to use */
	ib_rbt_t*	cached_stopword;/*!< This stores all active stopwords */
	CHARSET_INFO*	charset;	/*!< charset for stopword */
};

/** The SYNC state of the cache. There is one instance of this struct
associated with each ADD thread. */
struct fts_sync_t {
	trx_t*		trx;		/*!< The transaction used for SYNCing
					the cache to disk */
	dict_table_t*	table;		/*!< Table with FTS index(es) */
	ulint		max_cache_size;	/*!< Max size in bytes of the cache */
	ibool		cache_full;	/*!< flag, when true it indicates that
					we need to sync the cache to disk */
	ulint		lower_index;	/*!< the start index of the doc id
					vector from where to start adding
					documents to the FTS cache */
	ulint		upper_index;	/*!< max index of the doc id vector to
					add to the FTS cache */
	ibool		interrupted;	/*!< TRUE if SYNC was interrupted */
	doc_id_t	min_doc_id;	/*!< The smallest doc id added to the
					cache. It should equal to
					doc_ids[lower_index] */
	doc_id_t	max_doc_id;	/*!< The doc id at which the cache was
					noted as being full, we use this to
					set the upper_limit field */
	ib_time_monotonic_t	start_time;
	/*!< SYNC start time */
	bool		in_progress;	/*!< flag whether sync is in progress.*/
	bool		unlock_cache;	/*!< flag whether unlock cache when
					write fts node */
	os_event_t	event;		/*!< sync finish event */
};

/** The cache for the FTS system. It is a memory-based inverted index
that new entries are added to, until it grows over the configured maximum
size, at which time its contents are written to the INDEX table. */
struct fts_cache_t {
	rw_lock_t	lock;		/*!< lock protecting all access to the
					memory buffer. FIXME: this needs to
					be our new upgrade-capable rw-lock */

	rw_lock_t	init_lock;	/*!< lock used for the cache
					intialization, it has different
					SYNC level as above cache lock */

	ib_mutex_t	optimize_lock;	/*!< Lock for OPTIMIZE */

	ib_mutex_t	deleted_lock;	/*!< Lock covering deleted_doc_ids */

	ib_mutex_t	doc_id_lock;	/*!< Lock covering Doc ID */

	ib_vector_t*	deleted_doc_ids;/*!< Array of deleted doc ids, each
					element is of type fts_update_t */

	ib_vector_t*	indexes;	/*!< We store the stats and inverted
					index for the individual FTS indexes
					in this vector. Each element is
					an instance of fts_index_cache_t */

	ib_vector_t*	get_docs;	/*!< information required to read
					the document from the table. Each
					element is of type fts_doc_t */

	ulint		total_size;	/*!< total size consumed by the ilist
					field of all nodes. SYNC is run
					whenever this gets too big */
	fts_sync_t*	sync;		/*!< sync structure to sync data to
					disk */
	ib_alloc_t*	sync_heap;	/*!< The heap allocator, for indexes
					and deleted_doc_ids, ie. transient
					objects, they are recreated after
					a SYNC is completed */

	ib_alloc_t*	self_heap;	/*!< This heap is the heap out of
					which an instance of the cache itself
					was created. Objects created using
					this heap will last for the lifetime
					of the cache */

	doc_id_t	next_doc_id;	/*!< Next doc id */

	doc_id_t	synced_doc_id;	/*!< Doc ID sync-ed to CONFIG table */

	doc_id_t	first_doc_id;	/*!< first doc id since this table
					was opened */

	ulint		deleted;	/*!< Number of doc ids deleted since
					last optimized. This variable is
					covered by deleted_lock */

	ulint		added;		/*!< Number of doc ids added since last
					optimized. This variable is covered by
					the deleted lock */

	fts_stopword_t	stopword_info;	/*!< Cached stopwords for the FTS */
	mem_heap_t*	cache_heap;	/*!< Cache Heap */
};

/** Columns of the FTS auxiliary INDEX table */
struct fts_node_t {
	doc_id_t	first_doc_id;	/*!< First document id in ilist. */

	doc_id_t	last_doc_id;	/*!< Last document id in ilist. */

	byte*		ilist;		/*!< Binary list of documents & word
					positions the token appears in.
					TODO: For now, these are simply
					ut_malloc'd, but if testing shows
					that they waste memory unacceptably, a
					special memory allocator will have
					to be written */

	ulint		doc_count;	/*!< Number of doc ids in ilist */

	ulint		ilist_size;	/*!< Used size of ilist in bytes. */

	ulint		ilist_size_alloc;
					/*!< Allocated size of ilist in
					bytes */
	bool		synced;		/*!< flag whether the node is synced */
};

/** A tokenizer word. Contains information about one word. */
struct fts_tokenizer_word_t {
	fts_string_t	text;		/*!< Token text. */

	ib_vector_t*	nodes;		/*!< Word node ilists, each element is
					of type fts_node_t */
};

/** Word text plus it's array of nodes as on disk in FTS index */
struct fts_word_t {
	fts_string_t	text;		/*!< Word value in UTF-8 */
	ib_vector_t*	nodes;		/*!< Nodes read from disk */

	ib_alloc_t*	heap_alloc;	/*!< For handling all allocations */
};

/** Callback for reading and filtering nodes that are read from FTS index */
struct fts_fetch_t {
	void*		read_arg;	/*!< Arg for the sql_callback */

	fts_sql_callback
			read_record;	/*!< Callback for reading index
					record */
	ulint		total_memory;	/*!< Total memory used */
};

/** For horizontally splitting an FTS auxiliary index */
struct fts_index_selector_t {
	ulint		value;		/*!< Character value at which
					to split */

	const char*	suffix;		/*!< FTS aux index suffix */
};

/** This type represents a single document. */
struct fts_doc_t {
	fts_string_t	text;		/*!< document text */

	ibool		found;		/*!< TRUE if the document was found
					successfully in the database */

	ib_rbt_t*	tokens;		/*!< This is filled when the document
					is tokenized. Tokens; indexed by
					fts_string_t*, cells are of type
					fts_token_t* */

	ib_alloc_t*	self_heap;	/*!< An instance of this type is
					allocated from this heap along
					with any objects that have the
					same lifespan, most notably
					the vector of token positions */
	CHARSET_INFO*	charset;	/*!< Document's charset info */

	st_mysql_ftparser* parser;	/*!< fts plugin parser */

	bool		is_ngram;	/*!< Whether it is a ngram parser */

	ib_rbt_t*	stopwords;	/*!< Stopwords */
};

/** A token and its positions within a document. */
struct fts_token_t {
	fts_string_t	text;		/*!< token text */

	ib_vector_t*	positions;	/*!< an array of the positions the
					token is found in; each item is
					actually an ulint. */
};

/** It's defined in fts/fts0fts.c */
extern const fts_index_selector_t fts_index_selector[];

/******************************************************************//**
Compare two fts_trx_row_t instances doc_ids. */
UNIV_INLINE
int
fts_trx_row_doc_id_cmp(
/*===================*/
						/*!< out:
						< 0 if n1 < n2,
						0 if n1 == n2,
						> 0 if n1 > n2 */
	const void*	p1,			/*!< in: id1 */
	const void*	p2);			/*!< in: id2 */

/******************************************************************//**
Compare two fts_ranking_t instances doc_ids. */
UNIV_INLINE
int
fts_ranking_doc_id_cmp(
/*===================*/
						/*!< out:
						< 0 if n1 < n2,
						0 if n1 == n2,
						> 0 if n1 > n2 */
	const void*	p1,			/*!< in: id1 */
	const void*	p2);			/*!< in: id2 */

/******************************************************************//**
Compare two fts_update_t instances doc_ids. */
UNIV_INLINE
int
fts_update_doc_id_cmp(
/*==================*/
						/*!< out:
						< 0 if n1 < n2,
						0 if n1 == n2,
						> 0 if n1 > n2 */
	const void*	p1,			/*!< in: id1 */
	const void*	p2);			/*!< in: id2 */

/******************************************************************//**
Decode and return the integer that was encoded using our VLC scheme.*/
UNIV_INLINE
ulint
fts_decode_vlc(
/*===========*/
			/*!< out: value decoded */
	byte**	ptr);	/*!< in: ptr to decode from, this ptr is
			incremented by the number of bytes decoded */

/******************************************************************//**
Duplicate a string. */
UNIV_INLINE
void
fts_string_dup(
/*===========*/
						/*!< out:
						< 0 if n1 < n2,
						0 if n1 == n2,
						> 0 if n1 > n2 */
	fts_string_t*		dst,		/*!< in: dup to here */
	const fts_string_t*	src,		/*!< in: src string */
	mem_heap_t*		heap);		/*!< in: heap to use */

/******************************************************************//**
Return length of val if it were encoded using our VLC scheme. */
UNIV_INLINE
ulint
fts_get_encoded_len(
/*================*/
						/*!< out: length of value
						 encoded, in bytes */
	ulint		val);			/*!< in: value to encode */

/******************************************************************//**
Encode an integer using our VLC scheme and return the length in bytes. */
UNIV_INLINE
ulint
fts_encode_int(
/*===========*/
						/*!< out: length of value
						encoded, in bytes */
	ulint		val,			/*!< in: value to encode */
	byte*		buf);			/*!< in: buffer, must have
						enough space */

/******************************************************************//**
Get the selected FTS aux INDEX suffix. */
UNIV_INLINE
const char*
fts_get_suffix(
/*===========*/
	ulint		selected);		/*!< in: selected index */

/** Select the FTS auxiliary index for the given character.
@param[in]	cs	charset
@param[in]	str	string
@param[in]	len	string length in bytes
@return the index to use for the string */
UNIV_INLINE
ulint
fts_select_index(
	const CHARSET_INFO*	cs,
	const byte*		str,
	ulint			len);

#ifndef UNIV_NONINL
#include "fts0types.ic"
#include "fts0vlc.ic"
#endif

#endif /* INNOBASE_FTS0TYPES_H */
