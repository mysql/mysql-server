/*****************************************************************************

Copyright (c) 2007, 2018, Oracle and/or its affiliates. All Rights Reserved.

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
@file fts/fts0que.cc
Full Text Search functionality.

Created 2007/03/27 Sunny Bains
Completed 2011/7/10 Sunny and Jimmy Yang
*******************************************************/

#include "ha_prototypes.h"

#include "dict0dict.h"
#include "ut0rbt.h"
#include "row0sel.h"
#include "fts0fts.h"
#include "fts0priv.h"
#include "fts0ast.h"
#include "fts0pars.h"
#include "fts0types.h"
#include "fts0plugin.h"
#include "ut0new.h"

#ifdef UNIV_NONINL
#include "fts0types.ic"
#include "fts0vlc.ic"
#endif

#include <iomanip>
#include <vector>

#define FTS_ELEM(t, n, i, j) (t[(i) * n + (j)])

#define RANK_DOWNGRADE		(-1.0F)
#define RANK_UPGRADE		(1.0F)

/* Maximum number of words supported in a phrase or proximity search. */
#define MAX_PROXIMITY_ITEM	128

/* Memory used by rbt itself for create and node add */
#define SIZEOF_RBT_CREATE	sizeof(ib_rbt_t) + sizeof(ib_rbt_node_t) * 2
#define SIZEOF_RBT_NODE_ADD	sizeof(ib_rbt_node_t)

/*Initial byte length for 'words' in fts_ranking_t */
#define RANKING_WORDS_INIT_LEN	4

// FIXME: Need to have a generic iterator that traverses the ilist.

typedef std::vector<fts_string_t, ut_allocator<fts_string_t> >	word_vector_t;

struct fts_word_freq_t;

/** State of an FTS query. */
struct fts_query_t {
	mem_heap_t*	heap;		/*!< Heap to use for allocations */

	trx_t*		trx;		/*!< The query transaction */

	dict_index_t*	index;		/*!< The FTS index to search */
					/*!< FTS auxiliary common table def */

	fts_table_t	fts_common_table;

	fts_table_t	fts_index_table;/*!< FTS auxiliary index table def */

	ulint		total_size;	/*!< total memory size used by query */

	fts_doc_ids_t*	deleted;	/*!< Deleted doc ids that need to be
					filtered from the output */

	fts_ast_node_t*	root;		/*!< Abstract syntax tree */

	fts_ast_node_t* cur_node;	/*!< Current tree node */

	ib_rbt_t*	word_map;	/*!< Matched word map for
					searching by word*/

	word_vector_t*	word_vector;	/*!< Matched word vector for
					searching by index */

	ib_rbt_t*       doc_ids;	/*!< The current set of matching
					doc ids, elements are of
					type fts_ranking_t */

	ib_rbt_t*	intersection;	/*!< The doc ids that were found in
					doc_ids, this tree will become
					the new doc_ids, elements are of type
					fts_ranking_t */

					/*!< Prepared statement to read the
					nodes from the FTS INDEX */
	que_t*		read_nodes_graph;

	fts_ast_oper_t	oper;		/*!< Current boolean mode operator */

					/*!< TRUE if we want to collect the
					word positions within the document */
	ibool		collect_positions;

	ulint		flags;		/*!< Specify the full text search type,
					such as  boolean search, phrase
					search, proximity search etc. */

	ulint		distance;	/*!< The proximity distance of a
					phrase search. */

					/*!< These doc ids are used as a
					boundary condition when searching the
					FTS index rows */

	doc_id_t	lower_doc_id;	/*!< Lowest doc id in doc_ids */

	doc_id_t	upper_doc_id;	/*!< Highest doc id in doc_ids */

	bool		boolean_mode;	/*!< TRUE if boolean mode query */

	ib_vector_t*	matched;	/*!< Array of matching documents
					(fts_match_t) to search for a phrase */

	ib_vector_t**	match_array;	/*!< Used for proximity search, contains
					position info for each matched word
					in the word list */

	ib_uint64_t	total_docs;	/*!< The total number of documents */

	ulint		total_words;	/*!< The total number of words */

	dberr_t		error;		/*!< Error code if any, that is
					encountered during query processing */

	ib_rbt_t*	word_freqs;	/*!< RB tree of word frequencies per
					document, its elements are of type
					fts_word_freq_t */

	ib_rbt_t*	wildcard_words;	/*!< words with wildcard */

	bool		multi_exist;	/*!< multiple FTS_EXIST oper */

	st_mysql_ftparser*	parser;	/*!< fts plugin parser */

	/** limit value for the fts query */
	ulonglong		limit;

	/** number of docs fetched by query. This is to restrict the
	result with limit value */
	ulonglong		n_docs;
};

/** For phrase matching, first we collect the documents and the positions
then we match. */
struct fts_match_t {
	doc_id_t	doc_id;		/*!< Document id */

	ulint		start;		/*!< Start the phrase match from
					this offset within the positions
					vector. */

	ib_vector_t*	positions;	/*!< Offsets of a word in a
					document */
};

/** For matching tokens in a phrase search. We use this data structure in
the callback that determines whether a document should be accepted or
rejected for a phrase search. */
struct fts_select_t {
	doc_id_t	doc_id;		/*!< The document id to match */

	ulint		min_pos;	/*!< For found to be TRUE at least
					one position must be greater than
					min_pos. */

	ibool		found;		/*!< TRUE if found */

	fts_word_freq_t*
			word_freq;	/*!< Word frequency instance of the
					current word being looked up in
					the FTS index */
};

typedef std::vector<ulint, ut_allocator<ulint> >       pos_vector_t;

/** structure defines a set of ranges for original documents, each of which
has a minimum position and maximum position. Text in such range should
contain all words in the proximity search. We will need to count the
words in such range to make sure it is less than the specified distance
of the proximity search */
struct fts_proximity_t {
	ulint		n_pos;		/*!< number of position set, defines
					a range (min to max) containing all
					matching words */
	pos_vector_t	min_pos;	/*!< the minimum position (in bytes)
					of the range */
	pos_vector_t	max_pos;	/*!< the maximum position (in bytes)
					of the range */
};

/** The match positions and tokesn to match */
struct fts_phrase_t {
	fts_phrase_t(const dict_table_t* table)
		:
		found(false),
		match(NULL),
		tokens(NULL),
		distance(0),
		charset(NULL),
		heap(NULL),
		page_size(dict_table_page_size(table)),
		proximity_pos(NULL),
		parser(NULL)
	{
	}

	/** Match result */
	ibool			found;

	/** Positions within text */
	const fts_match_t*	match;

	/** Tokens to match */
	const ib_vector_t*	tokens;

	/** For matching on proximity distance. Can be 0 for exact match */
	ulint			distance;

	/** Phrase match charset */
	CHARSET_INFO*		charset;

	/** Heap for word processing */
	mem_heap_t*		heap;

	/** Row page size */
	const page_size_t	page_size;

	/** Position info for proximity search verification. Records the
	min and max position of words matched */
	fts_proximity_t*	proximity_pos;

	/** FTS plugin parser */
	st_mysql_ftparser*	parser;
};

/** Paramter passed to fts phrase match by parser */
struct fts_phrase_param_t {
	fts_phrase_t*	phrase;		/*!< Match phrase instance */
	ulint		token_index;	/*!< Index of token to match next */
	mem_heap_t*	heap;		/*!< Heap for word processing */
};

/** For storing the frequncy of a word/term in a document */
struct fts_doc_freq_t {
	doc_id_t	doc_id;		/*!< Document id */
	ulint		freq;		/*!< Frequency of a word in a document */
};

/** To determine the word frequency per document. */
struct fts_word_freq_t {
	fts_string_t	word;		/*!< Word for which we need the freq,
					it's allocated on the query heap */

	ib_rbt_t*	doc_freqs;	/*!< RB Tree for storing per document
					word frequencies. The elements are
					of type fts_doc_freq_t */
	ib_uint64_t	doc_count;	/*!< Total number of documents that
					contain this word */
	double		idf;		/*!< Inverse document frequency */
};

/********************************************************************
Callback function to fetch the rows in an FTS INDEX record.
@return always TRUE */
static
ibool
fts_query_index_fetch_nodes(
/*========================*/
	void*		row,		/*!< in: sel_node_t* */
	void*		user_arg);	/*!< in: pointer to ib_vector_t */

/********************************************************************
Read and filter nodes.
@return fts_node_t instance */
static
dberr_t
fts_query_filter_doc_ids(
/*=====================*/
	fts_query_t*		query,		/*!< in: query instance */
	const fts_string_t*	word,		/*!< in: the current word */
	fts_word_freq_t*	word_freq,	/*!< in/out: word frequency */
	const fts_node_t*	node,		/*!< in: current FTS node */
	void*			data,		/*!< in: doc id ilist */
	ulint			len,		/*!< in: doc id ilist size */
	ibool			calc_doc_count);/*!< in: whether to remember doc
						count */

#if 0
/*****************************************************************//***
Find a doc_id in a word's ilist.
@return TRUE if found. */
static
ibool
fts_query_find_doc_id(
/*==================*/
	fts_select_t*	select,		/*!< in/out: search the doc id selected,
					update the frequency if found. */
	void*		data,		/*!< in: doc id ilist */
	ulint		len);		/*!< in: doc id ilist size */
#endif

/*************************************************************//**
This function implements a simple "blind" query expansion search:
words in documents found in the first search pass will be used as
search arguments to search the document again, thus "expand"
the search result set.
@return DB_SUCCESS if success, otherwise the error code */
static
dberr_t
fts_expand_query(
/*=============*/
	dict_index_t*	index,		/*!< in: FTS index to search */
	fts_query_t*	query)		/*!< in: query result, to be freed
					by the client */
	MY_ATTRIBUTE((nonnull, warn_unused_result));
/*************************************************************//**
This function finds documents that contain all words in a
phrase or proximity search. And if proximity search, verify
the words are close enough to each other, as in specified distance.
This function is called for phrase and proximity search.
@return TRUE if documents are found, FALSE if otherwise */
static
ibool
fts_phrase_or_proximity_search(
/*===========================*/
	fts_query_t*	query,		/*!< in/out:  query instance
					query->doc_ids might be instantiated
					with qualified doc IDs */
	ib_vector_t*	tokens);	/*!< in: Tokens contain words */
/*************************************************************//**
This function checks whether words in result documents are close to
each other (within proximity range as specified by "distance").
If "distance" is MAX_ULINT, then it will find all combinations of
positions of matching words and store min and max positions
in the "qualified_pos" for later verification.
@return true if words are close to each other, false if otherwise */
static
bool
fts_proximity_get_positions(
/*========================*/
	fts_match_t**		match,		/*!< in: query instance */
	ulint			num_match,	/*!< in: number of matching
						items */
	ulint			distance,	/*!< in: distance value
						for proximity search */
	fts_proximity_t*	qualified_pos);	/*!< out: the position info
						records ranges containing
						all matching words. */
#if 0
/********************************************************************
Get the total number of words in a documents. */
static
ulint
fts_query_terms_in_document(
/*========================*/
					/*!< out: DB_SUCCESS if all go well
					else error code */
	fts_query_t*	query,		/*!< in: FTS query state */
	doc_id_t	doc_id,		/*!< in: the word to check */
	ulint*		total);		/*!< out: total words in document */
#endif

/********************************************************************
Compare two fts_doc_freq_t doc_ids.
@return < 0 if n1 < n2, 0 if n1 == n2, > 0 if n1 > n2 */
UNIV_INLINE
int
fts_freq_doc_id_cmp(
/*================*/
	const void*	p1,			/*!< in: id1 */
	const void*	p2)			/*!< in: id2 */
{
	const fts_doc_freq_t*	fq1 = (const fts_doc_freq_t*) p1;
	const fts_doc_freq_t*	fq2 = (const fts_doc_freq_t*) p2;

	return((int) (fq1->doc_id - fq2->doc_id));
}

#if 0
/*******************************************************************//**
Print the table used for calculating LCS. */
static
void
fts_print_lcs_table(
/*================*/
	const ulint*	table,		/*!< in: array to print */
	ulint		n_rows,		/*!< in: total no. of rows */
	ulint		n_cols)		/*!< in: total no. of cols */
{
	ulint		i;

	for (i = 0; i < n_rows; ++i) {
		ulint	j;

		printf("\n");

		for (j = 0; j < n_cols; ++j) {

			printf("%2lu ", FTS_ELEM(table, n_cols, i, j));
		}
	}
}

/********************************************************************
Find the longest common subsequence between the query string and
the document. */
static
ulint
fts_query_lcs(
/*==========*/
					/*!< out: LCS (length) between
					two ilists */
	const	ulint*	p1,		/*!< in: word positions of query */
	ulint	len_p1,			/*!< in: no. of elements in p1 */
	const	ulint*	p2,		/*!< in: word positions within document */
	ulint	len_p2)			/*!< in: no. of elements in p2 */
{
	int	i;
	ulint	len = 0;
	ulint	r = len_p1;
	ulint	c = len_p2;
	ulint	size = (r + 1) * (c + 1) * sizeof(ulint);
	ulint*	table = (ulint*) ut_malloc_nokey(size);

	/* Traverse the table backwards, from the last row to the first and
	also from the last column to the first. We compute the smaller
	common subsequeces first, then use the caluclated values to determine
	the longest common subsequence. The result will be in TABLE[0][0]. */
	for (i = r; i >= 0; --i) {
		int	j;

		for (j = c; j >= 0; --j) {

			if (p1[i] == (ulint) -1 || p2[j] == (ulint) -1) {

				FTS_ELEM(table, c, i, j) = 0;

			} else if (p1[i] == p2[j]) {

				FTS_ELEM(table, c, i, j) = FTS_ELEM(
					table, c, i + 1, j + 1) + 1;

			} else {

				ulint	value;

				value = ut_max(
					FTS_ELEM(table, c, i + 1, j),
					FTS_ELEM(table, c, i, j + 1));

				FTS_ELEM(table, c, i, j) = value;
			}
		}
	}

	len = FTS_ELEM(table, c, 0, 0);

	fts_print_lcs_table(table, r, c);
	printf("\nLen=%lu\n", len);

	ut_free(table);

	return(len);
}
#endif

/*******************************************************************//**
Compare two fts_ranking_t instance on their rank value and doc ids in
descending order on the rank and ascending order on doc id.
@return 0 if p1 == p2, < 0 if p1 < p2, > 0 if p1 > p2 */
static
int
fts_query_compare_rank(
/*===================*/
	const void*	p1,		/*!< in: pointer to elem */
	const void*	p2)		/*!< in: pointer to elem */
{
	const fts_ranking_t*	r1 = (const fts_ranking_t*) p1;
	const fts_ranking_t*	r2 = (const fts_ranking_t*) p2;

	if (r2->rank < r1->rank) {
		return(-1);
	} else if (r2->rank == r1->rank) {

		if (r1->doc_id < r2->doc_id) {
			return(1);
		} else if (r1->doc_id > r2->doc_id) {
			return(1);
		}

		return(0);
	}

	return(1);
}

/*******************************************************************//**
Create words in ranking */
static
void
fts_ranking_words_create(
/*=====================*/
	fts_query_t*	query,		/*!< in: query instance */
	fts_ranking_t*	ranking)	/*!< in: ranking instance */
{
	ranking->words = static_cast<byte*>(
		mem_heap_zalloc(query->heap, RANKING_WORDS_INIT_LEN));
	ranking->words_len = RANKING_WORDS_INIT_LEN;
}

/*
The optimization here is using a char array(bitmap) to replace words rb tree
in fts_ranking_t.

It can save lots of memory except in some cases of QUERY EXPANSION.

'word_map' is used as a word dictionary, in which the key is a word, the value
is a number. In 'fts_ranking_words_add', we first check if the word is in 'word_map'.
if not, we add it into 'word_map', and give it a position(actually a number).
then we set the corresponding bit to '1' at the position in the char array 'words'.

'word_vector' is a useful backup of 'word_map', and we can get a word by its position,
more quickly than searching by value in 'word_map'. we use 'word_vector'
in 'fts_query_calculate_ranking' and 'fts_expand_query'. In the two functions, we need
to scan the bitmap 'words', and get a word when a bit is '1', then we get word_freq
by the word.
*/

/*******************************************************************//**
Add a word into ranking */
static
void
fts_ranking_words_add(
/*==================*/
	fts_query_t*		query,		/*!< in: query instance */
	fts_ranking_t*		ranking,	/*!< in: ranking instance */
	const fts_string_t*	word)		/*!< in: term/word to add */
{
	ulint	pos;
	ulint	byte_offset;
	ulint	bit_offset;
	ib_rbt_bound_t	parent;

	/* Note: we suppose the word map and vector are append-only. */
	ut_ad(query->word_vector->size() == rbt_size(query->word_map));

	/* We use ib_rbt to simulate a map, f_n_char means position. */
	if (rbt_search(query->word_map, &parent, word) == 0) {
		fts_string_t*	result_word;

		result_word = rbt_value(fts_string_t, parent.last);
		pos = result_word->f_n_char;
		ut_ad(pos < rbt_size(query->word_map));
	} else {
		/* Add the word to map. */
		fts_string_t	new_word;

		pos = rbt_size(query->word_map);

		fts_string_dup(&new_word, word, query->heap);
		new_word.f_n_char = pos;

		rbt_add_node(query->word_map, &parent, &new_word);
		ut_ad(rbt_validate(query->word_map));
		query->word_vector->push_back(new_word);
	}

	/* Check words len */
	byte_offset = pos / CHAR_BIT;
	if (byte_offset >= ranking->words_len) {
		byte*	words = ranking->words;
		ulint	words_len = ranking->words_len;

		while (byte_offset >= words_len) {
			words_len *= 2;
		}

		ranking->words = static_cast<byte*>(
			mem_heap_zalloc(query->heap, words_len));
		ut_memcpy(ranking->words, words, ranking->words_len);
		ranking->words_len = words_len;
	}

	/* Set ranking words */
	ut_ad(byte_offset < ranking->words_len);
	bit_offset = pos % CHAR_BIT;
	ranking->words[byte_offset] |= 1 << bit_offset;
}

/*******************************************************************//**
Get a word from a ranking
@return true if it's successful */
static
bool
fts_ranking_words_get_next(
/*=======================*/
	const	fts_query_t*	query,	/*!< in: query instance */
	fts_ranking_t*		ranking,/*!< in: ranking instance */
	ulint*			pos,	/*!< in/out: word start pos */
	fts_string_t*		word)	/*!< in/out: term/word to add */
{
	bool	ret = false;
	ulint	max_pos = ranking->words_len * CHAR_BIT;

	/* Search for next word */
	while (*pos < max_pos) {
		ulint	byte_offset = *pos / CHAR_BIT;
		ulint	bit_offset = *pos % CHAR_BIT;

		if (ranking->words[byte_offset] & (1 << bit_offset)) {
			ret = true;
			break;
		}

		*pos += 1;
	};

	/* Get next word from word vector */
	if (ret) {
		ut_ad(*pos < query->word_vector->size());
		*word = query->word_vector->at((size_t)*pos);
		*pos += 1;
	}

	return ret;
}

/*******************************************************************//**
Add a word if it doesn't exist, to the term freq RB tree. We store
a pointer to the word that is passed in as the argument.
@return pointer to word */
static
fts_word_freq_t*
fts_query_add_word_freq(
/*====================*/
	fts_query_t*		query,		/*!< in: query instance */
	const fts_string_t*	word)		/*!< in: term/word to add */
{
	ib_rbt_bound_t		parent;

	/* Lookup the word in our rb tree and add if it doesn't exist. */
	if (rbt_search(query->word_freqs, &parent, word) != 0) {
		fts_word_freq_t	word_freq;

		memset(&word_freq, 0, sizeof(word_freq));

		fts_string_dup(&word_freq.word, word, query->heap);

		word_freq.doc_count = 0;

		word_freq.doc_freqs = rbt_create(
			sizeof(fts_doc_freq_t), fts_freq_doc_id_cmp);

		parent.last = rbt_add_node(
			query->word_freqs, &parent, &word_freq);

		query->total_size += word->f_len
			+ SIZEOF_RBT_CREATE
			+ SIZEOF_RBT_NODE_ADD
			+ sizeof(fts_word_freq_t);
	}

	return(rbt_value(fts_word_freq_t, parent.last));
}

/*******************************************************************//**
Add a doc id if it doesn't exist, to the doc freq RB tree.
@return pointer to word */
static
fts_doc_freq_t*
fts_query_add_doc_freq(
/*===================*/
	fts_query_t*	query,		/*!< in: query instance	*/
	ib_rbt_t*	doc_freqs,	/*!< in: rb tree of fts_doc_freq_t */
	doc_id_t	doc_id)		/*!< in: doc id to add */
{
	ib_rbt_bound_t	parent;

	/* Lookup the doc id in our rb tree and add if it doesn't exist. */
	if (rbt_search(doc_freqs, &parent, &doc_id) != 0) {
		fts_doc_freq_t	doc_freq;

		memset(&doc_freq, 0, sizeof(doc_freq));

		doc_freq.freq = 0;
		doc_freq.doc_id = doc_id;

		parent.last = rbt_add_node(doc_freqs, &parent, &doc_freq);

		query->total_size += SIZEOF_RBT_NODE_ADD
			+ sizeof(fts_doc_freq_t);
	}

	return(rbt_value(fts_doc_freq_t, parent.last));
}

/*******************************************************************//**
Add the doc id to the query set only if it's not in the
deleted array. */
static
void
fts_query_union_doc_id(
/*===================*/
	fts_query_t*	query,		/*!< in: query instance */
	doc_id_t	doc_id,		/*!< in: the doc id to add */
	fts_rank_t	rank)		/*!< in: if non-zero, it is the
					rank associated with the doc_id */
{
	ib_rbt_bound_t	parent;
	ulint		size = ib_vector_size(query->deleted->doc_ids);
	fts_update_t*	array = (fts_update_t*) query->deleted->doc_ids->data;

	/* Check if the doc id is deleted and it's not already in our set. */
	if (fts_bsearch(array, 0, static_cast<int>(size), doc_id) < 0
	    && rbt_search(query->doc_ids, &parent, &doc_id) != 0) {

		fts_ranking_t	ranking;

		ranking.rank = rank;
		ranking.doc_id = doc_id;
		fts_ranking_words_create(query, &ranking);

		rbt_add_node(query->doc_ids, &parent, &ranking);

		query->total_size += SIZEOF_RBT_NODE_ADD
			+ sizeof(fts_ranking_t) + RANKING_WORDS_INIT_LEN;
	}
}

/*******************************************************************//**
Remove the doc id from the query set only if it's not in the
deleted set. */
static
void
fts_query_remove_doc_id(
/*====================*/
	fts_query_t*	query,		/*!< in: query instance */
	doc_id_t	doc_id)		/*!< in: the doc id to add */
{
	ib_rbt_bound_t	parent;
	ulint		size = ib_vector_size(query->deleted->doc_ids);
	fts_update_t*	array = (fts_update_t*) query->deleted->doc_ids->data;

	/* Check if the doc id is deleted and it's in our set. */
	if (fts_bsearch(array, 0, static_cast<int>(size), doc_id) < 0
	    && rbt_search(query->doc_ids, &parent, &doc_id) == 0) {
		ut_free(rbt_remove_node(query->doc_ids, parent.last));

		ut_ad(query->total_size >=
		      SIZEOF_RBT_NODE_ADD + sizeof(fts_ranking_t));
		query->total_size -= SIZEOF_RBT_NODE_ADD
			+ sizeof(fts_ranking_t);
	}
}

/*******************************************************************//**
Find the doc id in the query set but not in the deleted set, artificialy
downgrade or upgrade its ranking by a value and make/initialize its ranking
under or above its normal range 0 to 1. This is used for Boolean Search
operator such as Negation operator, which makes word's contribution to the
row's relevance to be negative */
static
void
fts_query_change_ranking(
/*====================*/
	fts_query_t*	query,		/*!< in: query instance */
	doc_id_t	doc_id,		/*!< in: the doc id to add */
	ibool		downgrade)	/*!< in: Whether to downgrade ranking */
{
	ib_rbt_bound_t	parent;
	ulint		size = ib_vector_size(query->deleted->doc_ids);
	fts_update_t*	array = (fts_update_t*) query->deleted->doc_ids->data;

	/* Check if the doc id is deleted and it's in our set. */
	if (fts_bsearch(array, 0, static_cast<int>(size), doc_id) < 0
	    && rbt_search(query->doc_ids, &parent, &doc_id) == 0) {

		fts_ranking_t*	ranking;

		ranking = rbt_value(fts_ranking_t, parent.last);

		ranking->rank += downgrade ? RANK_DOWNGRADE : RANK_UPGRADE;

		/* Allow at most 2 adjustment by RANK_DOWNGRADE (-0.5)
		and RANK_UPGRADE (0.5) */
		if (ranking->rank >= 1.0F) {
			ranking->rank = 1.0F;
		} else if (ranking->rank <= -1.0F) {
			ranking->rank = -1.0F;
		}
	}
}

/*******************************************************************//**
Check the doc id in the query set only if it's not in the
deleted array. The doc ids that were found are stored in
another rb tree (fts_query_t::intersect). */
static
void
fts_query_intersect_doc_id(
/*=======================*/
	fts_query_t*	query,		/*!< in: query instance */
	doc_id_t	doc_id,		/*!< in: the doc id to add */
	fts_rank_t	rank)		/*!< in: if non-zero, it is the
					rank associated with the doc_id */
{
	ib_rbt_bound_t	parent;
	ulint		size = ib_vector_size(query->deleted->doc_ids);
	fts_update_t*	array = (fts_update_t*) query->deleted->doc_ids->data;
	fts_ranking_t*	ranking= NULL;

	/* There are three types of intersect:
	   1. '+a': doc_ids is empty, add doc into intersect if it matches 'a'.
	   2. 'a +b': docs match 'a' is in doc_ids, add doc into intersect
	      if it matches 'b'. if the doc is also in  doc_ids, then change the
	      doc's rank, and add 'a' in doc's words.
	   3. '+a +b': docs matching '+a' is in doc_ids, add doc into intsersect
	      if it matches 'b' and it's in doc_ids.(multi_exist = true). */

	/* Check if the doc id is deleted and it's in our set */
	if (fts_bsearch(array, 0, static_cast<int>(size), doc_id) < 0) {
		fts_ranking_t	new_ranking;

		if (rbt_search(query->doc_ids, &parent, &doc_id) != 0) {
			if (query->multi_exist) {
				return;
			} else {
				new_ranking.words = NULL;
			}
		} else {
			ranking = rbt_value(fts_ranking_t, parent.last);

			/* We've just checked the doc id before */
			if (ranking->words == NULL) {
				ut_ad(rbt_search(query->intersection, &parent,
					ranking) == 0);
				return;
			}

			/* Merge rank */
			rank += ranking->rank;
			if (rank >= 1.0F) {
				rank = 1.0F;
			} else if (rank <= -1.0F) {
				rank = -1.0F;
			}

			/* Take words */
			new_ranking.words = ranking->words;
			new_ranking.words_len = ranking->words_len;
		}

		new_ranking.rank = rank;
		new_ranking.doc_id = doc_id;

		if (rbt_search(query->intersection, &parent,
			       &new_ranking) != 0) {
			if (new_ranking.words == NULL) {
				fts_ranking_words_create(query, &new_ranking);

				query->total_size += RANKING_WORDS_INIT_LEN;
			} else {
				/* Note that the intersection has taken
				ownership of the ranking data. */
				ranking->words = NULL;
			}

			rbt_add_node(query->intersection,
				     &parent, &new_ranking);

			query->total_size += SIZEOF_RBT_NODE_ADD
				+ sizeof(fts_ranking_t);
		}
	}
}

/*******************************************************************//**
Free the document ranking rb tree. */
static
void
fts_query_free_doc_ids(
/*===================*/
	fts_query_t*	query,		/*!< in: query instance */
	ib_rbt_t*	doc_ids)	/*!< in: rb tree to free */
{
	const ib_rbt_node_t*	node;

	for (node = rbt_first(doc_ids); node; node = rbt_first(doc_ids)) {

		fts_ranking_t*	ranking;

		ranking = rbt_value(fts_ranking_t, node);

		if (ranking->words) {
			ranking->words = NULL;
		}

		ut_free(rbt_remove_node(doc_ids, node));

		ut_ad(query->total_size >=
		      SIZEOF_RBT_NODE_ADD + sizeof(fts_ranking_t));
		query->total_size -= SIZEOF_RBT_NODE_ADD
			+ sizeof(fts_ranking_t);
	}

	rbt_free(doc_ids);

	ut_ad(query->total_size >= SIZEOF_RBT_CREATE);
	query->total_size -= SIZEOF_RBT_CREATE;
}

/*******************************************************************//**
Add the word to the documents "list" of matching words from
the query. We make a copy of the word from the query heap. */
static
void
fts_query_add_word_to_document(
/*===========================*/
	fts_query_t*		query,	/*!< in: query to update */
	doc_id_t		doc_id,	/*!< in: the document to update */
	const fts_string_t*	word)	/*!< in: the token to add */
{
	ib_rbt_bound_t		parent;
	fts_ranking_t*		ranking = NULL;

	if (query->flags == FTS_OPT_RANKING) {
		return;
	}

	/* First we search the intersection RB tree as it could have
	taken ownership of the words rb tree instance. */
	if (query->intersection
	    && rbt_search(query->intersection, &parent, &doc_id) == 0) {

		ranking = rbt_value(fts_ranking_t, parent.last);
	}

	if (ranking == NULL
	    && rbt_search(query->doc_ids, &parent, &doc_id) == 0) {

		ranking = rbt_value(fts_ranking_t, parent.last);
	}

	if (ranking != NULL) {
		fts_ranking_words_add(query, ranking, word);
	}
}

/*******************************************************************//**
Check the node ilist. */
static
void
fts_query_check_node(
/*=================*/
	fts_query_t*		query,	/*!< in: query to update */
	const fts_string_t*	token,	/*!< in: the token to search */
	const fts_node_t*	node)	/*!< in: node to check */
{
	/* Skip nodes whose doc ids are out range. */
	if (query->oper == FTS_EXIST
	    && ((query->upper_doc_id > 0
		&& node->first_doc_id > query->upper_doc_id)
		|| (query->lower_doc_id > 0
		    && node->last_doc_id < query->lower_doc_id))) {

		/* Ignore */

	} else {
		int		ret;
		ib_rbt_bound_t	parent;
		ulint		ilist_size = node->ilist_size;
		fts_word_freq_t*word_freqs;

		/* The word must exist. */
		ret = rbt_search(query->word_freqs, &parent, token);
		ut_a(ret == 0);

		word_freqs = rbt_value(fts_word_freq_t, parent.last);

		query->error = fts_query_filter_doc_ids(
					query, token, word_freqs, node,
					node->ilist, ilist_size, TRUE);
	}
}

/*****************************************************************//**
Search index cache for word with wildcard match.
@return number of words matched */
static
ulint
fts_cache_find_wildcard(
/*====================*/
	fts_query_t*		query,		/*!< in: query instance */
	const fts_index_cache_t*index_cache,	/*!< in: cache to search */
	const fts_string_t*	token)		/*!< in: token to search */
{
	ib_rbt_bound_t		parent;
	const ib_vector_t*	nodes = NULL;
	fts_string_t		srch_text;
	byte			term[FTS_MAX_WORD_LEN + 1];
	ulint			num_word = 0;

	srch_text.f_len = (token->f_str[token->f_len - 1] == '%')
			? token->f_len - 1
			: token->f_len;

	strncpy((char*) term, (char*) token->f_str, srch_text.f_len);
	term[srch_text.f_len] = '\0';
	srch_text.f_str = term;

	/* Lookup the word in the rb tree */
	if (rbt_search_cmp(index_cache->words, &parent, &srch_text, NULL,
			   innobase_fts_text_cmp_prefix) == 0) {
		const fts_tokenizer_word_t*     word;
		ulint				i;
		const ib_rbt_node_t*		cur_node;
		ibool				forward = FALSE;

		word = rbt_value(fts_tokenizer_word_t, parent.last);
		cur_node = parent.last;

		while (innobase_fts_text_cmp_prefix(
			index_cache->charset, &srch_text, &word->text) == 0) {

			nodes = word->nodes;

			for (i = 0; nodes && i < ib_vector_size(nodes); ++i) {
				int                     ret;
				const fts_node_t*       node;
				ib_rbt_bound_t          freq_parent;
				fts_word_freq_t*	word_freqs;

				node = static_cast<const fts_node_t*>(
					ib_vector_get_const(nodes, i));

				ret = rbt_search(query->word_freqs,
						 &freq_parent,
						 &srch_text);

				ut_a(ret == 0);

				word_freqs = rbt_value(
					fts_word_freq_t,
					freq_parent.last);

				query->error = fts_query_filter_doc_ids(
					query, &srch_text,
					word_freqs, node,
					node->ilist, node->ilist_size, TRUE);

				if (query->error != DB_SUCCESS) {
					return(0);
				}
			}

			num_word++;

			if (!forward) {
				cur_node = rbt_prev(
					index_cache->words, cur_node);
			} else {
cont_search:
				cur_node = rbt_next(
					index_cache->words, cur_node);
			}

			if (!cur_node) {
				break;
			}

			word = rbt_value(fts_tokenizer_word_t, cur_node);
		}

		if (!forward) {
			forward = TRUE;
			cur_node = parent.last;
			goto cont_search;
		}
	}

	return(num_word);
}

/*****************************************************************//**
Set difference.
@return DB_SUCCESS if all go well */
static MY_ATTRIBUTE((nonnull, warn_unused_result))
dberr_t
fts_query_difference(
/*=================*/
	fts_query_t*		query,	/*!< in: query instance */
	const fts_string_t*	token)	/*!< in: token to search */
{
	ulint			n_doc_ids= 0;
	trx_t*			trx = query->trx;
	dict_table_t*		table = query->index->table;

	ut_a(query->oper == FTS_IGNORE);

#ifdef FTS_INTERNAL_DIAG_PRINT
	{
		ib::info	out;
		out << "DIFFERENCE: Searching: '";
		out.write(token->f_str, token->f_len);
		out << "'";
	}
#endif

	if (query->doc_ids) {
		n_doc_ids = rbt_size(query->doc_ids);
	}

	/* There is nothing we can substract from an empty set. */
	if (query->doc_ids && !rbt_empty(query->doc_ids)) {
		ulint			i;
		fts_fetch_t		fetch;
		const ib_vector_t*	nodes;
		const fts_index_cache_t*index_cache;
		que_t*			graph = NULL;
		fts_cache_t*		cache = table->fts->cache;
		dberr_t			error;

		rw_lock_x_lock(&cache->lock);

		index_cache = fts_find_index_cache(cache, query->index);

		/* Must find the index cache */
		ut_a(index_cache != NULL);

		/* Search the cache for a matching word first. */
		if (query->cur_node->term.wildcard
		    && query->flags != FTS_PROXIMITY
		    && query->flags != FTS_PHRASE) {
			fts_cache_find_wildcard(query, index_cache, token);
		} else {
			nodes = fts_cache_find_word(index_cache, token);

			for (i = 0; nodes && i < ib_vector_size(nodes)
			     && query->error == DB_SUCCESS; ++i) {
				const fts_node_t*	node;

				node = static_cast<const fts_node_t*>(
					ib_vector_get_const(nodes, i));

				fts_query_check_node(query, token, node);
			}
		}

		rw_lock_x_unlock(&cache->lock);

		/* error is passed by 'query->error' */
		if (query->error != DB_SUCCESS) {
			ut_ad(query->error == DB_FTS_EXCEED_RESULT_CACHE_LIMIT);
			return(query->error);
		}

		/* Setup the callback args for filtering and
		consolidating the ilist. */
		fetch.read_arg = query;
		fetch.read_record = fts_query_index_fetch_nodes;

		error = fts_index_fetch_nodes(
			trx, &graph, &query->fts_index_table, token, &fetch);

		/* DB_FTS_EXCEED_RESULT_CACHE_LIMIT passed by 'query->error' */
		ut_ad(!(query->error != DB_SUCCESS && error != DB_SUCCESS));
		if (error != DB_SUCCESS) {
			query->error = error;
		}

		fts_que_graph_free(graph);
	}

	/* The size can't increase. */
	ut_a(rbt_size(query->doc_ids) <= n_doc_ids);

	return(query->error);
}

/*****************************************************************//**
Intersect the token doc ids with the current set.
@return DB_SUCCESS if all go well */
static MY_ATTRIBUTE((nonnull, warn_unused_result))
dberr_t
fts_query_intersect(
/*================*/
	fts_query_t*		query,	/*!< in: query instance */
	const fts_string_t*	token)	/*!< in: the token to search */
{
	trx_t*			trx = query->trx;
	dict_table_t*		table = query->index->table;

	ut_a(query->oper == FTS_EXIST);

#ifdef FTS_INTERNAL_DIAG_PRINT
	{
		ib::info	out;
		out << "INTERSECT: Searching: '";
		out.write(token->f_str, token->f_len);
		out << "'";
	}
#endif

	/* If the words set is not empty and multi exist is true,
	we know the intersection set is empty in advance. */
	if (!(rbt_empty(query->doc_ids) && query->multi_exist)) {
		ulint                   n_doc_ids = 0;
		ulint			i;
		fts_fetch_t		fetch;
		const ib_vector_t*	nodes;
		const fts_index_cache_t*index_cache;
		que_t*			graph = NULL;
		fts_cache_t*		cache = table->fts->cache;
		dberr_t			error;

		ut_a(!query->intersection);

		n_doc_ids = rbt_size(query->doc_ids);

		/* Create the rb tree that will hold the doc ids of
		the intersection. */
		query->intersection = rbt_create(
			sizeof(fts_ranking_t), fts_ranking_doc_id_cmp);

		query->total_size += SIZEOF_RBT_CREATE;

		/* This is to avoid decompressing the ilist if the
		node's ilist doc ids are out of range. */
		if (!rbt_empty(query->doc_ids) && query->multi_exist) {
			const ib_rbt_node_t*	node;
			doc_id_t*		doc_id;

			node = rbt_first(query->doc_ids);
			doc_id = rbt_value(doc_id_t, node);
			query->lower_doc_id = *doc_id;

			node = rbt_last(query->doc_ids);
			doc_id = rbt_value(doc_id_t, node);
			query->upper_doc_id = *doc_id;

		} else {
			query->lower_doc_id = 0;
			query->upper_doc_id = 0;
		}

		/* Search the cache for a matching word first. */

		rw_lock_x_lock(&cache->lock);

		/* Search for the index specific cache. */
		index_cache = fts_find_index_cache(cache, query->index);

		/* Must find the index cache. */
		ut_a(index_cache != NULL);

		if (query->cur_node->term.wildcard) {
			/* Wildcard search the index cache */
			fts_cache_find_wildcard(query, index_cache, token);
		} else {
			nodes = fts_cache_find_word(index_cache, token);

			for (i = 0; nodes && i < ib_vector_size(nodes)
			     && query->error == DB_SUCCESS; ++i) {
				const fts_node_t*	node;

				node = static_cast<const fts_node_t*>(
					ib_vector_get_const(nodes, i));

				fts_query_check_node(query, token, node);
			}
		}

		rw_lock_x_unlock(&cache->lock);

		/* error is passed by 'query->error' */
		if (query->error != DB_SUCCESS) {
			ut_ad(query->error == DB_FTS_EXCEED_RESULT_CACHE_LIMIT);
			return(query->error);
		}

		/* Setup the callback args for filtering and
		consolidating the ilist. */
		fetch.read_arg = query;
		fetch.read_record = fts_query_index_fetch_nodes;

		error = fts_index_fetch_nodes(
			trx, &graph, &query->fts_index_table, token, &fetch);

		/* DB_FTS_EXCEED_RESULT_CACHE_LIMIT passed by 'query->error' */
		ut_ad(!(query->error != DB_SUCCESS && error != DB_SUCCESS));
		if (error != DB_SUCCESS) {
			query->error = error;
		}

		fts_que_graph_free(graph);

		if (query->error == DB_SUCCESS) {
			/* Make the intesection (rb tree) the current doc id
			set and free the old set. */
			fts_query_free_doc_ids(query, query->doc_ids);
			query->doc_ids = query->intersection;
			query->intersection = NULL;

			ut_a(!query->multi_exist || (query->multi_exist
			     && rbt_size(query->doc_ids) <= n_doc_ids));
		}
	}

	return(query->error);
}

/*****************************************************************//**
Query index cache.
@return DB_SUCCESS if all go well */
static
dberr_t
fts_query_cache(
/*============*/
	fts_query_t*		query,	/*!< in/out: query instance */
	const fts_string_t*	token)	/*!< in: token to search */
{
	const fts_index_cache_t*index_cache;
	dict_table_t*		table = query->index->table;
	fts_cache_t*		cache = table->fts->cache;

	/* Search the cache for a matching word first. */
	rw_lock_x_lock(&cache->lock);

	/* Search for the index specific cache. */
	index_cache = fts_find_index_cache(cache, query->index);

	/* Must find the index cache. */
	ut_a(index_cache != NULL);

	if (query->cur_node->term.wildcard
	    && query->flags != FTS_PROXIMITY
	    && query->flags != FTS_PHRASE) {
		/* Wildcard search the index cache */
		fts_cache_find_wildcard(query, index_cache, token);
	} else {
		const ib_vector_t*      nodes;
		ulint			i;

		nodes = fts_cache_find_word(index_cache, token);

		for (i = 0; nodes && i < ib_vector_size(nodes)
		     && query->error == DB_SUCCESS; ++i) {
			const fts_node_t*	node;

			node = static_cast<const fts_node_t*>(
				ib_vector_get_const(nodes, i));

			fts_query_check_node(query, token, node);
		}
	}

	rw_lock_x_unlock(&cache->lock);

	return(query->error);
}

/*****************************************************************//**
Set union.
@return DB_SUCCESS if all go well */
static MY_ATTRIBUTE((nonnull, warn_unused_result))
dberr_t
fts_query_union(
/*============*/
	fts_query_t*		query,	/*!< in: query instance */
	fts_string_t*		token)	/*!< in: token to search */
{
	fts_fetch_t		fetch;
	ulint			n_doc_ids = 0;
	trx_t*			trx = query->trx;
	que_t*			graph = NULL;
	dberr_t			error;

	ut_a(query->oper == FTS_NONE || query->oper == FTS_DECR_RATING ||
	     query->oper == FTS_NEGATE || query->oper == FTS_INCR_RATING);

#ifdef FTS_INTERNAL_DIAG_PRINT
	{
		ib::info	out;
		out << "UNION: Searching: '";
		out.write(token->f_str, token->f_len);
		out << "'";
	}
#endif

	if (query->doc_ids) {
		n_doc_ids = rbt_size(query->doc_ids);
	}

	if (token->f_len == 0) {
		return(query->error);
	}

	fts_query_cache(query, token);

	/* Setup the callback args for filtering and
	consolidating the ilist. */
	fetch.read_arg = query;
	fetch.read_record = fts_query_index_fetch_nodes;

	/* Read the nodes from disk. */
	error = fts_index_fetch_nodes(
		trx, &graph, &query->fts_index_table, token, &fetch);

	/* DB_FTS_EXCEED_RESULT_CACHE_LIMIT passed by 'query->error' */
	ut_ad(!(query->error != DB_SUCCESS && error != DB_SUCCESS));
	if (error != DB_SUCCESS) {
		query->error = error;
	}

	fts_que_graph_free(graph);

	if (query->error == DB_SUCCESS) {

		/* The size can't decrease. */
		ut_a(rbt_size(query->doc_ids) >= n_doc_ids);

		/* Calulate the number of doc ids that were added to
		the current doc id set. */
		if (query->doc_ids) {
			n_doc_ids = rbt_size(query->doc_ids) - n_doc_ids;
		}
	}

	return(query->error);
}

/*****************************************************************//**
Depending upon the current query operator process the doc id.
return DB_SUCCESS if all go well
or return DB_FTS_EXCEED_RESULT_CACHE_LIMIT */
static
dberr_t
fts_query_process_doc_id(
/*=====================*/
	fts_query_t*	query,		/*!< in: query instance */
	doc_id_t	doc_id,		/*!< in: doc id to process */
	fts_rank_t	rank)		/*!< in: if non-zero, it is the
					rank associated with the doc_id */
{
	if (query->flags == FTS_OPT_RANKING) {
		return(DB_SUCCESS);
	}

	switch (query->oper) {
	case FTS_NONE:
		fts_query_union_doc_id(query, doc_id, rank);
		break;

	case FTS_EXIST:
		fts_query_intersect_doc_id(query, doc_id, rank);
		break;

	case FTS_IGNORE:
		fts_query_remove_doc_id(query, doc_id);
		break;

	case FTS_NEGATE:
		fts_query_change_ranking(query, doc_id, TRUE);
		break;

	case FTS_DECR_RATING:
		fts_query_union_doc_id(query, doc_id, rank);
		fts_query_change_ranking(query, doc_id, TRUE);
		break;

	case FTS_INCR_RATING:
		fts_query_union_doc_id(query, doc_id, rank);
		fts_query_change_ranking(query, doc_id, FALSE);
		break;

	default:
		ut_error;
	}

	if (query->total_size > fts_result_cache_limit) {
		return(DB_FTS_EXCEED_RESULT_CACHE_LIMIT);
	} else {
		return(DB_SUCCESS);
	}
}

/*****************************************************************//**
Merge two result sets. */
static
dberr_t
fts_merge_doc_ids(
/*==============*/
	fts_query_t*	query,		/*!< in,out: query instance */
	const ib_rbt_t*	doc_ids)	/*!< in: result set to merge */
{
	const ib_rbt_node_t*	node;

	DBUG_ENTER("fts_merge_doc_ids");

	ut_a(!query->intersection);

	/* To process FTS_EXIST operation (intersection), we need
	to create a new result set for fts_query_intersect(). */
	if (query->oper == FTS_EXIST) {

		query->intersection = rbt_create(
			sizeof(fts_ranking_t), fts_ranking_doc_id_cmp);

		query->total_size += SIZEOF_RBT_CREATE;
	}

	/* Merge the elements to the result set. */
	for (node = rbt_first(doc_ids); node; node = rbt_next(doc_ids, node)) {
		fts_ranking_t*		ranking;
		ulint			pos = 0;
		fts_string_t		word;

		ranking = rbt_value(fts_ranking_t, node);

		query->error = fts_query_process_doc_id(
				query, ranking->doc_id, ranking->rank);

		if (query->error != DB_SUCCESS) {
			DBUG_RETURN(query->error);
		}

		/* Merge words. Don't need to take operator into account. */
		ut_a(ranking->words);
		while (fts_ranking_words_get_next(query, ranking, &pos, &word)) {
			fts_query_add_word_to_document(query, ranking->doc_id,
						       &word);
		}
	}

	/* If it is an intersection operation, reset query->doc_ids
	to query->intersection and free the old result list. */
	if (query->oper == FTS_EXIST && query->intersection != NULL) {
		fts_query_free_doc_ids(query, query->doc_ids);
		query->doc_ids = query->intersection;
		query->intersection = NULL;
	}

	DBUG_RETURN(DB_SUCCESS);
}

/*****************************************************************//**
Skip non-whitespace in a string. Move ptr to the next word boundary.
@return pointer to first whitespace character or end */
UNIV_INLINE
byte*
fts_query_skip_word(
/*================*/
	byte*		ptr,		/*!< in: start of scan */
	const byte*	end)		/*!< in: pointer to end of string */
{
	/* TODO: Does this have to be UTF-8 too ? */
	while (ptr < end && !(ispunct(*ptr) || isspace(*ptr))) {
		++ptr;
	}

	return(ptr);
}

/*****************************************************************//**
Check whether the remaining terms in the phrase match the text.
@return TRUE if matched else FALSE */
static
ibool
fts_query_match_phrase_terms(
/*=========================*/
	fts_phrase_t*	phrase,		/*!< in: phrase to match */
	byte**		start,		/*!< in/out: text to search, we can't
					make this const becase we need to
					first convert the string to
					lowercase */
	const byte*	end,		/*!< in: pointer to the end of
					the string to search */
	mem_heap_t*	heap)		/*!< in: heap */
{
	ulint			i;
	byte*			ptr = *start;
	const ib_vector_t*	tokens = phrase->tokens;
	ulint			distance = phrase->distance;

	/* We check only from the second term onwards, since the first
	must have matched otherwise we wouldn't be here. */
	for (i = 1; ptr < end && i < ib_vector_size(tokens); /* No op */) {
		fts_string_t		match;
		fts_string_t		cmp_str;
		const fts_string_t*	token;
		int			result;
		ulint			ret;

		ret = innobase_mysql_fts_get_token(
			phrase->charset, ptr,
			const_cast<byte*>(end), &match);

		if (match.f_len > 0) {
			/* Get next token to match. */
			token = static_cast<const fts_string_t*>(
				ib_vector_get_const(tokens, i));

			fts_string_dup(&cmp_str, &match, heap);

			result = innobase_fts_text_case_cmp(
				phrase->charset, token, &cmp_str);

			/* Skip the rest of the tokens if this one doesn't
			match and the proximity distance is exceeded. */
			if (result
			    && (distance == ULINT_UNDEFINED
				|| distance == 0)) {

				break;
			}

			/* This token matched move to the next token. */
			if (result == 0) {
				/* Advance the text to search by the length
				of the last token. */
				ptr += ret;

				/* Advance to the next token. */
				++i;
			} else {

				ut_a(distance != ULINT_UNDEFINED);

				ptr = fts_query_skip_word(ptr, end);
			}

			/* Distance can be 0 for exact matches. */
			if (distance != ULINT_UNDEFINED && distance > 0) {
				--distance;
			}
		} else {
			ptr += ret;
		}
	}

	*start = ptr;

	/* Can't be greater than the number of elements. */
	ut_a(i <= ib_vector_size(tokens));

	/* This is the case for multiple words. */
	if (i == ib_vector_size(tokens)) {
		phrase->found = TRUE;
	}

	return(phrase->found);
}

/*****************************************************************//**
Callback function to count the number of words in position ranges,
and see whether the word count is in specified "phrase->distance"
@return true if the number of characters is less than the "distance" */
static
bool
fts_proximity_is_word_in_range(
/*===========================*/
	const fts_phrase_t*
			phrase,		/*!< in: phrase with the search info */
	byte*		start,		/*!< in: text to search */
	ulint		total_len)	/*!< in: length of text */
{
	fts_proximity_t*	proximity_pos = phrase->proximity_pos;

	ut_ad(proximity_pos->n_pos == proximity_pos->min_pos.size());
	ut_ad(proximity_pos->n_pos == proximity_pos->max_pos.size());

	/* Search each matched position pair (with min and max positions)
	and count the number of words in the range */
	for (ulint i = 0; i < proximity_pos->n_pos; i++) {
		ulint		cur_pos = proximity_pos->min_pos[i];
		ulint		n_word = 0;

		ut_ad(proximity_pos->max_pos[i] <= total_len);

		/* Walk through words in the range and count them */
		while (cur_pos <= proximity_pos->max_pos[i]) {
			ulint		len;
			fts_string_t	str;

			len = innobase_mysql_fts_get_token(
				phrase->charset,
				start + cur_pos,
				start + total_len, &str);

			if (len == 0) {
				break;
			}

			/* Advances position with "len" bytes */
			cur_pos += len;

			/* Record the number of words */
			if (str.f_n_char > 0) {
				n_word++;
			}

			if (n_word > phrase->distance) {
				break;
			}
		}

		/* Check if the number of words is less than specified
		"distance" */
		if (n_word && n_word <= phrase->distance) {
			return(true);
		}
	}

	return(false);
}

/*****************************************************************//**
FTS plugin parser 'myql_add_word' callback function for phrase match
Refer to 'st_mysql_ftparser_param' for more detail.
@return 0 if match, or return non-zero */
static
int
fts_query_match_phrase_add_word_for_parser(
/*=======================================*/
	MYSQL_FTPARSER_PARAM*	param,		/*!< in: parser param */
	char*			word,		/*!< in: token */
	int			word_len,	/*!< in: token length */
	MYSQL_FTPARSER_BOOLEAN_INFO* info)	/*!< in: token info */
{
	fts_phrase_param_t*	phrase_param;
	fts_phrase_t*		phrase;
	const ib_vector_t*	tokens;
	fts_string_t		match;
	fts_string_t		cmp_str;
	const fts_string_t*	token;
	int			result;
	mem_heap_t*		heap;

	phrase_param = static_cast<fts_phrase_param_t*>(param->mysql_ftparam);
	heap = phrase_param->heap;
	phrase = phrase_param->phrase;
	tokens = phrase->tokens;

	/* In case plugin parser doesn't check return value */
	if (phrase_param->token_index == ib_vector_size(tokens)) {
		return(1);
	}

	match.f_str = reinterpret_cast<byte*>(word);
	match.f_len = word_len;
	match.f_n_char = fts_get_token_size(phrase->charset, word, word_len);

	if (match.f_len > 0) {
		/* Get next token to match. */
		ut_a(phrase_param->token_index < ib_vector_size(tokens));
		token = static_cast<const fts_string_t*>(
			ib_vector_get_const(tokens, phrase_param->token_index));

		fts_string_dup(&cmp_str, &match, heap);

		result = innobase_fts_text_case_cmp(
			phrase->charset, token, &cmp_str);

		if (result == 0) {
			phrase_param->token_index++;
		} else {
			return(1);
		}
	}

	/* Can't be greater than the number of elements. */
	ut_a(phrase_param->token_index <= ib_vector_size(tokens));

	/* This is the case for multiple words. */
	if (phrase_param->token_index == ib_vector_size(tokens)) {
		phrase->found = TRUE;
	}

	return(static_cast<int>(phrase->found));
}

/*****************************************************************//**
Check whether the terms in the phrase match the text.
@return TRUE if matched else FALSE */
static
ibool
fts_query_match_phrase_terms_by_parser(
/*===================================*/
	fts_phrase_param_t*	phrase_param,	/* in/out: phrase param */
	st_mysql_ftparser*	parser,		/* in: plugin fts parser */
	byte*			text,		/* in: text to check */
	ulint			len)		/* in: text length */
{
	MYSQL_FTPARSER_PARAM	param;

	ut_a(parser);

	/* Set paramters for param */
	param.mysql_parse = fts_tokenize_document_internal;
	param.mysql_add_word = fts_query_match_phrase_add_word_for_parser;
	param.mysql_ftparam = phrase_param;
	param.cs = phrase_param->phrase->charset;
	param.doc = reinterpret_cast<char*>(text);
	param.length = static_cast<int>(len);
	param.mode= MYSQL_FTPARSER_WITH_STOPWORDS;

	PARSER_INIT(parser, &param);
	parser->parse(&param);
	PARSER_DEINIT(parser, &param);

	return(phrase_param->phrase->found);
}

/*****************************************************************//**
Callback function to fetch and search the document.
@return TRUE if matched else FALSE */
static
ibool
fts_query_match_phrase(
/*===================*/
	fts_phrase_t*	phrase,		/*!< in: phrase to match */
	byte*		start,		/*!< in: text to search, we can't make
					this const becase we need to first
					convert the string to lowercase */
	ulint		cur_len,	/*!< in: length of text */
	ulint		prev_len,	/*!< in: total length for searched
					doc fields*/
	mem_heap_t*	heap)		/* heap */
{
	ulint			i;
	const fts_string_t*	first;
	const byte*		end = start + cur_len;
	const ib_vector_t*	tokens = phrase->tokens;
	const ib_vector_t*	positions = phrase->match->positions;

	ut_a(!phrase->found);
	ut_a(phrase->match->doc_id > 0);
	ut_a(ib_vector_size(tokens) > 0);
	ut_a(ib_vector_size(positions) > 0);

	first = static_cast<const fts_string_t*>(
		ib_vector_get_const(tokens, 0));

	ut_a(phrase->match->start < ib_vector_size(positions));

	for (i = phrase->match->start; i < ib_vector_size(positions); ++i) {
		ulint		pos;
		byte*		ptr = start;

		pos = *(ulint*) ib_vector_get_const(positions, i);

		if (pos == ULINT_UNDEFINED) {
			break;
		}

		if (pos < prev_len) {
			continue;
		}

		/* Document positions are calculated from the beginning
		of the first field, need to save the length for each
		searched field to adjust the doc position when search
		phrases. */
		pos -= prev_len;
		ptr = start + pos;

		/* Within limits ? */
		if (ptr >= end) {
			break;
		}

		if (phrase->parser) {
			fts_phrase_param_t	phrase_param;

			phrase_param.phrase = phrase;
			phrase_param.token_index = 0;
			phrase_param.heap = heap;

			if (fts_query_match_phrase_terms_by_parser(
				&phrase_param,
				phrase->parser,
				ptr,
				(end - ptr))) {
				break;
			}
		} else {
			fts_string_t	match;
			fts_string_t	cmp_str;
			ulint		ret;

			match.f_str = ptr;
			ret = innobase_mysql_fts_get_token(
				phrase->charset, start + pos,
				const_cast<byte*>(end), &match);

			if (match.f_len == 0) {
				break;
			}

			fts_string_dup(&cmp_str, &match, heap);

			if (innobase_fts_text_case_cmp(
				phrase->charset, first, &cmp_str) == 0) {

				/* This is the case for the single word
				in the phrase. */
				if (ib_vector_size(phrase->tokens) == 1) {
					phrase->found = TRUE;
					break;
				}

				ptr += ret;

				/* Match the remaining terms in the phrase. */
				if (fts_query_match_phrase_terms(phrase, &ptr,
								 end, heap)) {
					break;
				}
			}
		}
	}

	return(phrase->found);
}

/*****************************************************************//**
Callback function to fetch and search the document.
@return whether the phrase is found */
static
ibool
fts_query_fetch_document(
/*=====================*/
	void*		row,		/*!< in:  sel_node_t* */
	void*		user_arg)	/*!< in:  fts_doc_t* */
{

	que_node_t*	exp;
	sel_node_t*	node = static_cast<sel_node_t*>(row);
	fts_phrase_t*	phrase = static_cast<fts_phrase_t*>(user_arg);
	ulint		prev_len = 0;
	ulint		total_len = 0;
	byte*		document_text = NULL;

	exp = node->select_list;

	phrase->found = FALSE;

	/* For proximity search, we will need to get the whole document
	from all fields, so first count the total length of the document
	from all the fields */
	if (phrase->proximity_pos) {
		 while (exp) {
			ulint		field_len;
			dfield_t*	dfield = que_node_get_val(exp);
			byte*		data = static_cast<byte*>(
						dfield_get_data(dfield));

			if (dfield_is_ext(dfield)) {
				ulint	local_len = dfield_get_len(dfield);

				local_len -= BTR_EXTERN_FIELD_REF_SIZE;

				field_len = mach_read_from_4(
					data + local_len + BTR_EXTERN_LEN + 4);
			} else {
				field_len = dfield_get_len(dfield);
			}

			if (field_len != UNIV_SQL_NULL) {
				total_len += field_len + 1;
			}

			exp = que_node_get_next(exp);
		}

		document_text = static_cast<byte*>(mem_heap_zalloc(
					phrase->heap, total_len));

		if (!document_text) {
			return(FALSE);
		}
	}

	exp = node->select_list;

	while (exp) {
		dfield_t*	dfield = que_node_get_val(exp);
		byte*		data = static_cast<byte*>(
					dfield_get_data(dfield));
		ulint		cur_len;

		if (dfield_is_ext(dfield)) {
			data = btr_copy_externally_stored_field(
				&cur_len, data, phrase->page_size,
				dfield_get_len(dfield), phrase->heap);
		} else {
			cur_len = dfield_get_len(dfield);
		}

		if (cur_len != UNIV_SQL_NULL && cur_len != 0) {
			if (phrase->proximity_pos) {
				ut_ad(prev_len + cur_len <= total_len);
				memcpy(document_text + prev_len, data, cur_len);
			} else {
				/* For phrase search */
				phrase->found =
					fts_query_match_phrase(
						phrase,
						static_cast<byte*>(data),
						cur_len, prev_len,
						phrase->heap);
			}

			/* Document positions are calculated from the beginning
			of the first field, need to save the length for each
			searched field to adjust the doc position when search
			phrases. */
			prev_len += cur_len + 1;
		}

		if (phrase->found) {
			break;
		}

		exp = que_node_get_next(exp);
	}

	if (phrase->proximity_pos) {
		ut_ad(prev_len <= total_len);

		phrase->found = fts_proximity_is_word_in_range(
			phrase, document_text, total_len);
	}

	return(phrase->found);
}

#if 0
/********************************************************************
Callback function to check whether a record was found or not. */
static
ibool
fts_query_select(
/*=============*/
	void*		row,		/*!< in:  sel_node_t* */
	void*		user_arg)	/*!< in:  fts_doc_t* */
{
	int		i;
	que_node_t*	exp;
	sel_node_t*	node = row;
	fts_select_t*	select = user_arg;

	ut_a(select->word_freq);
	ut_a(select->word_freq->doc_freqs);

	exp = node->select_list;

	for (i = 0; exp && !select->found; ++i) {
		dfield_t*	dfield = que_node_get_val(exp);
		void*		data = dfield_get_data(dfield);
		ulint		len = dfield_get_len(dfield);

		switch (i) {
		case 0: /* DOC_COUNT */
			if (len != UNIV_SQL_NULL && len != 0) {

				select->word_freq->doc_count +=
					mach_read_from_4(data);
			}
			break;

		case 1: /* ILIST */
			if (len != UNIV_SQL_NULL && len != 0) {

				fts_query_find_doc_id(select, data, len);
			}
			break;

		default:
			ut_error;
		}

		exp = que_node_get_next(exp);
	}

	return(FALSE);
}

/********************************************************************
Read the rows from the FTS index, that match word and where the
doc id is between first and last doc id.
@return DB_SUCCESS if all go well else error code */
static MY_ATTRIBUTE((nonnull, warn_unused_result))
dberr_t
fts_query_find_term(
/*================*/
	fts_query_t*		query,	/*!< in: FTS query state */
	que_t**			graph,	/*!< in: prepared statement */
	const fts_string_t*	word,	/*!< in: the word to fetch */
	doc_id_t		doc_id,	/*!< in: doc id to match */
	ulint*			min_pos,/*!< in/out: pos found must be
					 greater than this minimum value. */
	ibool*			found)	/*!< out: TRUE if found else FALSE */
{
	pars_info_t*		info;
	dberr_t			error;
	fts_select_t		select;
	doc_id_t		match_doc_id;
	trx_t*			trx = query->trx;
	char			table_name[MAX_FULL_NAME_LEN];

	trx->op_info = "fetching FTS index matching nodes";

	if (*graph) {
		info = (*graph)->info;
	} else {
		ulint	selected;

		info = pars_info_create();

		selected = fts_select_index(*word->f_str);
		query->fts_index_table.suffix = fts_get_suffix(selected);

		fts_get_table_name(&query->fts_index_table, table_name);
		pars_info_bind_id(info, true, "index_table_name", table_name);
	}

	select.found = FALSE;
	select.doc_id = doc_id;
	select.min_pos = *min_pos;
	select.word_freq = fts_query_add_word_freq(query, word->f_str);

	pars_info_bind_function(info, "my_func", fts_query_select, &select);
	pars_info_bind_varchar_literal(info, "word", word->f_str, word->f_len);

	/* Convert to "storage" byte order. */
	fts_write_doc_id((byte*) &match_doc_id, doc_id);

	fts_bind_doc_id(info, "min_doc_id", &match_doc_id);

	fts_bind_doc_id(info, "max_doc_id", &match_doc_id);

	if (!*graph) {

		*graph = fts_parse_sql(
			&query->fts_index_table,
			info,
			"DECLARE FUNCTION my_func;\n"
			"DECLARE CURSOR c IS"
			" SELECT doc_count, ilist\n"
			" FROM $index_table_name\n"
			" WHERE word LIKE :word AND"
			" first_doc_id <= :min_doc_id AND"
			" last_doc_id >= :max_doc_id\n"
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
				ib::warn() << "lock wait timeout reading FTS"
					" index. Retrying!";

				trx->error_state = DB_SUCCESS;
			} else {
				ib::error() << error
					<< " while reading FTS index.";

				break;			/* Exit the loop. */
			}
		}
	}

	/* Value to return */
	*found = select.found;

	if (*found) {
		*min_pos = select.min_pos;
	}

	return(error);
}

/********************************************************************
Callback aggregator for int columns. */
static
ibool
fts_query_sum(
/*==========*/
					/*!< out: always returns TRUE */
	void*		row,		/*!< in:  sel_node_t* */
	void*		user_arg)	/*!< in:  ulint* */
{

	que_node_t*	exp;
	sel_node_t*	node = row;
	ulint*		total = user_arg;

	exp = node->select_list;

	while (exp) {
		dfield_t*	dfield = que_node_get_val(exp);
		void*		data = dfield_get_data(dfield);
		ulint		len = dfield_get_len(dfield);

		if (len != UNIV_SQL_NULL && len != 0) {
			*total += mach_read_from_4(data);
		}

		exp = que_node_get_next(exp);
	}

	return(TRUE);
}

/********************************************************************
Calculate the total documents that contain a particular word (term).
@return DB_SUCCESS if all go well else error code */
static MY_ATTRIBUTE((nonnull, warn_unused_result))
dberr_t
fts_query_total_docs_containing_term(
/*=================================*/
	fts_query_t*		query,	/*!< in: FTS query state */
	const fts_string_t*	word,	/*!< in: the word to check */
	ulint*			total)	/*!< out: documents containing word */
{
	pars_info_t*		info;
	dberr_t			error;
	que_t*			graph;
	ulint			selected;
	trx_t*			trx = query->trx;
	char			table_name[MAX_FULL_NAME_LEN]

	trx->op_info = "fetching FTS index document count";

	*total = 0;

	info = pars_info_create();

	pars_info_bind_function(info, "my_func", fts_query_sum, total);
	pars_info_bind_varchar_literal(info, "word", word->f_str, word->f_len);

	selected = fts_select_index(*word->f_str);

	query->fts_index_table.suffix = fts_get_suffix(selected);

	fts_get_table_name(&query->fts_index_table, table_name);

	pars_info_bind_id(info, true, "index_table_name", table_name);

	graph = fts_parse_sql(
		&query->fts_index_table,
		info,
		"DECLARE FUNCTION my_func;\n"
		"DECLARE CURSOR c IS"
		" SELECT doc_count\n"
		" FROM $index_table_name\n"
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

	for (;;) {
		error = fts_eval_sql(trx, graph);

		if (error == DB_SUCCESS) {

			break;				/* Exit the loop. */
		} else {

			if (error == DB_LOCK_WAIT_TIMEOUT) {
				ib::warn() << "lock wait timeout reading FTS"
					" index. Retrying!";

				trx->error_state = DB_SUCCESS;
			} else {
				ib::error() << error
					<< " while reading FTS index.";

				break;			/* Exit the loop. */
			}
		}
	}

	fts_que_graph_free(graph);

	return(error);
}

/********************************************************************
Get the total number of words in a documents.
@return DB_SUCCESS if all go well else error code */
static MY_ATTRIBUTE((nonnull, warn_unused_result))
dberr_t
fts_query_terms_in_document(
/*========================*/
	fts_query_t*	query,		/*!< in: FTS query state */
	doc_id_t	doc_id,		/*!< in: the word to check */
	ulint*		total)		/*!< out: total words in document */
{
	pars_info_t*	info;
	dberr_t		error;
	que_t*		graph;
	doc_id_t	read_doc_id;
	trx_t*		trx = query->trx;
	char		table_name[MAX_FULL_NAME_LEN];

	trx->op_info = "fetching FTS document term count";

	*total = 0;

	info = pars_info_create();

	pars_info_bind_function(info, "my_func", fts_query_sum, total);

	/* Convert to "storage" byte order. */
	fts_write_doc_id((byte*) &read_doc_id, doc_id);
	fts_bind_doc_id(info, "doc_id", &read_doc_id);

	query->fts_index_table.suffix = "DOC_ID";

	fts_get_table_name(&query->fts_index_table, table_name);

	pars_info_bind_id(info, true, "index_table_name", table_name);

	graph = fts_parse_sql(
		&query->fts_index_table,
		info,
		"DECLARE FUNCTION my_func;\n"
		"DECLARE CURSOR c IS"
		" SELECT count\n"
		" FROM $index_table_name\n"
		" WHERE doc_id = :doc_id"
		" BEGIN\n"
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

			break;				/* Exit the loop. */
		} else {

			if (error == DB_LOCK_WAIT_TIMEOUT) {
				ib::warn() << "lock wait timeout reading FTS"
					" doc id table. Retrying!";

				trx->error_state = DB_SUCCESS;
			} else {
				ib::error() << error << " while reading FTS"
					" doc id table.";

				break;			/* Exit the loop. */
			}
		}
	}

	fts_que_graph_free(graph);

	return(error);
}
#endif

/*****************************************************************//**
Retrieve the document and match the phrase tokens.
@return DB_SUCCESS or error code */
static MY_ATTRIBUTE((nonnull, warn_unused_result))
dberr_t
fts_query_match_document(
/*=====================*/
	ib_vector_t*	tokens,		/*!< in: phrase tokens */
	fts_get_doc_t*	get_doc,	/*!< in: table and prepared statements */
	fts_match_t*	match,		/*!< in: doc id and positions */
	ulint		distance,	/*!< in: proximity distance */
	st_mysql_ftparser* parser,	/*!< in: fts plugin parser */
	ibool*		found)		/*!< out: TRUE if phrase found */
{
	dberr_t		error;
	fts_phrase_t	phrase(get_doc->index_cache->index->table);

	phrase.match = match;		/* Positions to match */
	phrase.tokens = tokens;		/* Tokens to match */
	phrase.distance = distance;
	phrase.charset = get_doc->index_cache->charset;
	phrase.heap = mem_heap_create(512);
	phrase.parser = parser;

	*found = phrase.found = FALSE;

	error = fts_doc_fetch_by_doc_id(
		get_doc, match->doc_id, NULL, FTS_FETCH_DOC_BY_ID_EQUAL,
		fts_query_fetch_document, &phrase);

	if (error != DB_SUCCESS) {
		ib::error() << "(" << ut_strerr(error)
			<< ") matching document.";
	} else {
		*found = phrase.found;
	}

	mem_heap_free(phrase.heap);

	return(error);
}

/*****************************************************************//**
This function fetches the original documents and count the
words in between matching words to see that is in specified distance
@return DB_SUCCESS if all OK */
static MY_ATTRIBUTE((nonnull, warn_unused_result))
bool
fts_query_is_in_proximity_range(
/*============================*/
	const fts_query_t*	query,		/*!< in:  query instance */
	fts_match_t**		match,		/*!< in: query instance */
	fts_proximity_t*	qualified_pos)	/*!< in: position info for
						qualified ranges */
{
	fts_get_doc_t	get_doc;
	fts_cache_t*	cache = query->index->table->fts->cache;
	dberr_t		err;

	memset(&get_doc, 0x0, sizeof(get_doc));

	rw_lock_x_lock(&cache->lock);
	get_doc.index_cache = fts_find_index_cache(cache, query->index);
	rw_lock_x_unlock(&cache->lock);
	ut_a(get_doc.index_cache != NULL);

	fts_phrase_t	phrase(get_doc.index_cache->index->table);

	phrase.distance = query->distance;
	phrase.charset = get_doc.index_cache->charset;
	phrase.heap = mem_heap_create(512);
	phrase.proximity_pos = qualified_pos;
	phrase.found = FALSE;

	err = fts_doc_fetch_by_doc_id(
		&get_doc, match[0]->doc_id, NULL, FTS_FETCH_DOC_BY_ID_EQUAL,
		fts_query_fetch_document, &phrase);

	if (err != DB_SUCCESS) {
		ib::error() << "(" << ut_strerr(err) << ") in verification"
			" phase of proximity search";
	}

	/* Free the prepared statement. */
	if (get_doc.get_document_graph) {
		fts_que_graph_free(get_doc.get_document_graph);
		get_doc.get_document_graph = NULL;
	}

	mem_heap_free(phrase.heap);

	return(err == DB_SUCCESS && phrase.found);
}

/*****************************************************************//**
Iterate over the matched document ids and search the for the
actual phrase in the text.
@return DB_SUCCESS if all OK */
static MY_ATTRIBUTE((nonnull, warn_unused_result))
dberr_t
fts_query_search_phrase(
/*====================*/
	fts_query_t*		query,		/*!< in: query instance */
	ib_vector_t*		orig_tokens,	/*!< in: tokens to search,
						with any stopwords in the
						original phrase */
	ib_vector_t*		tokens)		/*!< in: tokens that does
						not include stopwords and
						can be used to calculate
						ranking */
{
	ulint			i;
	fts_get_doc_t		get_doc;
	ulint			n_matched;
	fts_cache_t*		cache = query->index->table->fts->cache;

	n_matched = ib_vector_size(query->matched);

	/* Setup the doc retrieval infrastructure. */
	memset(&get_doc, 0x0, sizeof(get_doc));

	rw_lock_x_lock(&cache->lock);

	get_doc.index_cache = fts_find_index_cache(cache, query->index);

	/* Must find the index cache */
	ut_a(get_doc.index_cache != NULL);

	rw_lock_x_unlock(&cache->lock);

#ifdef FTS_INTERNAL_DIAG_PRINT
	ib::info() << "Start phrase search";
#endif

	/* Read the document from disk and do the actual
	match, matching documents will be added to the current
	doc id set. */
	for (i = 0; i < n_matched && query->error == DB_SUCCESS; ++i) {
		fts_match_t*	match;
		ibool		found = FALSE;

		match = static_cast<fts_match_t*>(
			ib_vector_get(query->matched, i));

		/* Skip the document ids that were filtered out by
		an earlier pass. */
		if (match->doc_id != 0) {

			query->error = fts_query_match_document(
				orig_tokens, &get_doc, match,
				query->distance, query->parser, &found);

			if (query->error == DB_SUCCESS && found) {
				ulint	z;

				query->error = fts_query_process_doc_id(query,
							 match->doc_id, 0);
				if (query->error != DB_SUCCESS) {
					goto func_exit;
				}

				for (z = 0; z < ib_vector_size(tokens); z++) {
					fts_string_t*   token;
					token = static_cast<fts_string_t*>(
						ib_vector_get(tokens, z));
					fts_query_add_word_to_document(
						query, match->doc_id, token);
				}
			}
		}
	}

func_exit:
	/* Free the prepared statement. */
	if (get_doc.get_document_graph) {
		fts_que_graph_free(get_doc.get_document_graph);
		get_doc.get_document_graph = NULL;
	}

	return(query->error);
}

/** Split the phrase into tokens
@param[in,out]	query		query instance
@param[in]	node		query node to search
@param[in,out]	tokens		token vector
@param[in,out]	orig_tokens	original node tokens include stopword
@param[in,out]	heap	mem heap */
static
void
fts_query_phrase_split(
	fts_query_t*		query,
	const fts_ast_node_t*	node,
	ib_vector_t*		tokens,
	ib_vector_t*		orig_tokens,
	mem_heap_t*		heap)
{
	fts_string_t		phrase;
	ulint			len = 0;
	ulint			cur_pos = 0;
	fts_ast_node_t*		term_node = NULL;

	if (node->type == FTS_AST_TEXT) {
		phrase.f_str = node->text.ptr->str;
		phrase.f_len = node->text.ptr->len;
		len = phrase.f_len;
	} else {
		ut_ad(node->type == FTS_AST_PARSER_PHRASE_LIST);
		phrase.f_str = NULL;
		phrase.f_len = 0;
		term_node = node->list.head;
	}

	while (true) {
		fts_cache_t*	cache = query->index->table->fts->cache;
		ulint		cur_len;
		fts_string_t	result_str;

		if (node->type == FTS_AST_TEXT) {
			if (cur_pos >= len) {
				break;
			}

			cur_len = innobase_mysql_fts_get_token(
				query->fts_index_table.charset,
				reinterpret_cast<const byte*>(phrase.f_str)
				+ cur_pos,
				reinterpret_cast<const byte*>(phrase.f_str)
				+ len,
				&result_str);

			if (cur_len == 0) {
				break;
			}

			cur_pos += cur_len;
		} else {
			ut_ad(node->type == FTS_AST_PARSER_PHRASE_LIST);
			/* Term node in parser phrase list */
			if (term_node == NULL) {
				break;
			}

			ut_a(term_node->type == FTS_AST_TERM);
			result_str.f_str = term_node->term.ptr->str;
			result_str.f_len = term_node->term.ptr->len;
			result_str.f_n_char = fts_get_token_size(
				query->fts_index_table.charset,
				reinterpret_cast<char*>(result_str.f_str),
				result_str.f_len);

			term_node = term_node->next;
		}

		if (result_str.f_n_char == 0) {
			continue;
		}

		fts_string_t*	token = static_cast<fts_string_t*>(
			ib_vector_push(tokens, NULL));
		fts_string_dup(token, &result_str, heap);

		if (fts_check_token(
			   &result_str,
			   cache->stopword_info.cached_stopword,
			   query->index->is_ngram,
			   query->fts_index_table.charset)) {
			/* Add the word to the RB tree so that we can
			calculate it's frequencey within a document. */
			fts_query_add_word_freq(query, token);
		} else {
			ib_vector_pop(tokens);
		}

		/* we will start to store all words including stopwords
		in the "orig_tokens" vector, but skip any leading words
		that are stopwords */
		if (!ib_vector_is_empty(tokens)) {
			fts_string_t*	orig_token = static_cast<fts_string_t*>(
				ib_vector_push(orig_tokens, NULL));

			orig_token->f_str = token->f_str;
			orig_token->f_len = token->f_len;
		}
	}
}

/*****************************************************************//**
Text/Phrase search.
@return DB_SUCCESS or error code */
static MY_ATTRIBUTE((warn_unused_result))
dberr_t
fts_query_phrase_search(
/*====================*/
	fts_query_t*		query,	/*!< in: query instance */
	const fts_ast_node_t*	node)	/*!< in: node to search */
{
	ib_vector_t*		tokens;
	ib_vector_t*		orig_tokens;
	mem_heap_t*		heap = mem_heap_create(sizeof(fts_string_t));
	ib_alloc_t*		heap_alloc;
	ulint			num_token;

	heap_alloc = ib_heap_allocator_create(heap);

	tokens = ib_vector_create(heap_alloc, sizeof(fts_string_t), 4);
	orig_tokens = ib_vector_create(heap_alloc, sizeof(fts_string_t), 4);

	if (query->distance != ULINT_UNDEFINED && query->distance > 0) {
		query->flags = FTS_PROXIMITY;
	} else {
		query->flags = FTS_PHRASE;
	}

	/* Split the phrase into tokens. */
	fts_query_phrase_split(query, node, tokens, orig_tokens, heap);

	num_token = ib_vector_size(tokens);
	if (num_token > MAX_PROXIMITY_ITEM) {
		query->error = DB_FTS_TOO_MANY_WORDS_IN_PHRASE;
		goto func_exit;
	}

	ut_ad(ib_vector_size(orig_tokens) >= num_token);

	/* Ignore empty strings. */
	if (num_token > 0) {
		fts_string_t*	token;
		fts_fetch_t	fetch;
		trx_t*		trx = query->trx;
		fts_ast_oper_t	oper = query->oper;
		que_t*		graph = NULL;
		ulint		i;
		dberr_t		error;

		/* Create the vector for storing matching document ids
		and the positions of the first token of the phrase. */
		if (!query->matched) {
			ib_alloc_t*	heap_alloc;

			heap_alloc = ib_heap_allocator_create(heap);

			if (!(query->flags & FTS_PROXIMITY)
			    && !(query->flags & FTS_PHRASE)) {
				query->matched = ib_vector_create(
					heap_alloc, sizeof(fts_match_t),
					64);
			} else {
				ut_a(num_token <= MAX_PROXIMITY_ITEM);
				query->match_array =
					(ib_vector_t**) mem_heap_alloc(
						heap,
						num_token *
						sizeof(query->matched));

				for (i = 0; i < num_token; i++) {
					query->match_array[i] =
					ib_vector_create(
						heap_alloc, sizeof(fts_match_t),
						64);
				}

				query->matched = query->match_array[0];
			}
		}

		/* Setup the callback args for filtering and consolidating
		the ilist. */
		fetch.read_arg = query;
		fetch.read_record = fts_query_index_fetch_nodes;

		for (i = 0; i < num_token; i++) {
			/* Search for the first word from the phrase. */
			token = static_cast<fts_string_t*>(
				ib_vector_get(tokens, i));

			if (query->flags & FTS_PROXIMITY
			    || query->flags & FTS_PHRASE) {
				query->matched = query->match_array[i];
			}

			error = fts_index_fetch_nodes(
				trx, &graph, &query->fts_index_table,
				token, &fetch);

			/* DB_FTS_EXCEED_RESULT_CACHE_LIMIT passed by 'query->error' */
			ut_ad(!(query->error != DB_SUCCESS && error != DB_SUCCESS));
			if (error != DB_SUCCESS) {
				query->error = error;
			}

			fts_que_graph_free(graph);
			graph = NULL;

			fts_query_cache(query, token);

			if (!(query->flags & FTS_PHRASE)
			    && !(query->flags & FTS_PROXIMITY)) {
				break;
			}

			/* If any of the token can't be found,
			no need to continue match */
			if (ib_vector_is_empty(query->match_array[i])
			    || query->error != DB_SUCCESS) {
				goto func_exit;
			}
		}

		/* Just a single word, no need to fetch the original
		documents to do phrase matching */
		if (ib_vector_size(orig_tokens) == 1
		    && !ib_vector_is_empty(query->match_array[0])) {
			fts_match_t*    match;
			ulint		n_matched;

			n_matched = ib_vector_size(query->match_array[0]);

			for (i = 0; i < n_matched; i++) {
				match = static_cast<fts_match_t*>(
					ib_vector_get(
						query->match_array[0], i));

				query->error = fts_query_process_doc_id(
						query, match->doc_id, 0);
				if (query->error != DB_SUCCESS) {
					goto func_exit;
				}

				fts_query_add_word_to_document(
					query, match->doc_id, token);
			}
			query->oper = oper;
			goto func_exit;
		}

		/* If we are doing proximity search, verify the distance
		between all words, and check they are in specified distance. */
		if (query->flags & FTS_PROXIMITY) {
			fts_phrase_or_proximity_search(query, tokens);
		} else {
			ibool	matched;

			/* Phrase Search case:
			We filter out the doc ids that don't contain
			all the tokens in the phrase. It's cheaper to
			search the ilist than bringing the documents in
			and then doing a search through the text. Isolated
			testing shows this also helps in mitigating disruption
			of the buffer cache. */
			matched = fts_phrase_or_proximity_search(query, tokens);
			query->matched = query->match_array[0];

			/* Read the actual text in and search for the phrase. */
			if (matched) {
				ut_ad(query->error == DB_SUCCESS);
				query->error = fts_query_search_phrase(
					query, orig_tokens, tokens);
			}
		}

		/* Restore original operation. */
		query->oper = oper;

		if (query->error != DB_SUCCESS) {
			goto func_exit;
		}
	}

func_exit:
	mem_heap_free(heap);

	/* Don't need it anymore. */
	query->matched = NULL;

	return(query->error);
}

/*****************************************************************//**
Find the word and evaluate.
@return DB_SUCCESS if all go well */
static MY_ATTRIBUTE((nonnull, warn_unused_result))
dberr_t
fts_query_execute(
/*==============*/
	fts_query_t*		query,	/*!< in: query instance */
	fts_string_t*		token)	/*!< in: token to search */
{
	switch (query->oper) {
	case FTS_NONE:
	case FTS_NEGATE:
	case FTS_INCR_RATING:
	case FTS_DECR_RATING:
		query->error = fts_query_union(query, token);
		break;

	case FTS_EXIST:
		query->error = fts_query_intersect(query, token);
		break;

	case FTS_IGNORE:
		query->error = fts_query_difference(query, token);
		break;

	default:
		ut_error;
	}

	return(query->error);
}

/*****************************************************************//**
Create a wildcard string. It's the responsibility of the caller to
free the byte* pointer. It's allocated using ut_malloc_nokey().
@return ptr to allocated memory */
static
byte*
fts_query_get_token(
/*================*/
	fts_ast_node_t*	node,		/*!< in: the current sub tree */
	fts_string_t*	token)		/*!< in: token to create */
{
	ulint		str_len;
	byte*		new_ptr = NULL;

	str_len = node->term.ptr->len;

	ut_a(node->type == FTS_AST_TERM);

	token->f_len = str_len;
	token->f_str = node->term.ptr->str;

	if (node->term.wildcard) {

		token->f_str = static_cast<byte*>(ut_malloc_nokey(str_len + 2));
		token->f_len = str_len + 1;

		memcpy(token->f_str, node->term.ptr->str, str_len);

		token->f_str[str_len] = '%';
		token->f_str[token->f_len] = 0;

		new_ptr = token->f_str;
	}

	return(new_ptr);
}

/*****************************************************************//**
Visit every node of the AST. */
static
dberr_t
fts_query_visitor(
/*==============*/
	fts_ast_oper_t	oper,		/*!< in: current operator */
	fts_ast_node_t*	node,		/*!< in: The root of the current subtree*/
	void*		arg)		/*!< in: callback arg*/
{
	byte*		ptr;
	fts_string_t	token;
	fts_query_t*	query = static_cast<fts_query_t*>(arg);

	ut_a(node);
	DBUG_ENTER("fts_query_visitor");
	DBUG_PRINT("fts", ("nodetype: %s", fts_ast_node_type_get(node->type)));

	token.f_n_char = 0;
	query->oper = oper;
	query->cur_node = node;

	switch (node->type) {
	case FTS_AST_TEXT:
	case FTS_AST_PARSER_PHRASE_LIST:

		if (query->oper == FTS_EXIST) {
			ut_ad(query->intersection == NULL);
			query->intersection = rbt_create(
				sizeof(fts_ranking_t), fts_ranking_doc_id_cmp);

			query->total_size += SIZEOF_RBT_CREATE;
		}

		/* Set the current proximity distance. */
		query->distance = node->text.distance;

		/* Force collection of doc ids and the positions. */
		query->collect_positions = TRUE;

		query->error = fts_query_phrase_search(query, node);

		query->collect_positions = FALSE;

		if (query->oper == FTS_EXIST) {
			fts_query_free_doc_ids(query, query->doc_ids);
			query->doc_ids = query->intersection;
			query->intersection = NULL;
		}

		break;

	case FTS_AST_TERM:
		token.f_str = node->term.ptr->str;
		token.f_len = node->term.ptr->len;

		/* Collect wildcard words for QUERY EXPANSION. */
		if (node->term.wildcard && query->wildcard_words != NULL) {
			ib_rbt_bound_t          parent;

			if (rbt_search(query->wildcard_words, &parent, &token)
			     != 0) {
				fts_string_t	word;

				fts_string_dup(&word, &token, query->heap);
				rbt_add_node(query->wildcard_words, &parent,
					     &word);
			}
		}

		/* Add the word to our RB tree that will be used to
		calculate this terms per document frequency. */
		fts_query_add_word_freq(query, &token);

		ptr = fts_query_get_token(node, &token);
		query->error = fts_query_execute(query, &token);

		if (ptr) {
			ut_free(ptr);
		}

		break;

	case FTS_AST_SUBEXP_LIST:
		query->error = fts_ast_visit_sub_exp(node, fts_query_visitor, arg);
		break;

	default:
		ut_error;
	}

	if (query->oper == FTS_EXIST) {
		query->multi_exist = true;
	}

	DBUG_RETURN(query->error);
}

/*****************************************************************//**
Process (nested) sub-expression, create a new result set to store the
sub-expression result by processing nodes under current sub-expression
list. Merge the sub-expression result with that of parent expression list.
@return DB_SUCCESS if all go well */
dberr_t
fts_ast_visit_sub_exp(
/*==================*/
	fts_ast_node_t*		node,		/*!< in,out: current root node */
	fts_ast_callback	visitor,	/*!< in: callback function */
	void*			arg)		/*!< in,out: arg for callback */
{
	fts_ast_oper_t		cur_oper;
	fts_query_t*		query = static_cast<fts_query_t*>(arg);
	ib_rbt_t*		parent_doc_ids;
	ib_rbt_t*		subexpr_doc_ids;
	dberr_t			error = DB_SUCCESS;
	bool			will_be_ignored = false;
	bool			multi_exist;

	DBUG_ENTER("fts_ast_visit_sub_exp");

	ut_a(node->type == FTS_AST_SUBEXP_LIST);

	cur_oper = query->oper;

	/* Save current result set */
	parent_doc_ids = query->doc_ids;

	/* Create new result set to store the sub-expression result. We
	will merge this result set with the parent after processing. */
	query->doc_ids = rbt_create(sizeof(fts_ranking_t),
				    fts_ranking_doc_id_cmp);

	query->total_size += SIZEOF_RBT_CREATE;

	multi_exist = query->multi_exist;
	query->multi_exist = false;
	/* Process nodes in current sub-expression and store its
	result set in query->doc_ids we created above. */
	error = fts_ast_visit(FTS_NONE, node, visitor,
			      arg, &will_be_ignored);

	/* Reinstate parent node state */
	query->multi_exist = multi_exist;
	query->oper = cur_oper;

	/* Merge the sub-expression result with the parent result set. */
	subexpr_doc_ids = query->doc_ids;
	query->doc_ids = parent_doc_ids;
	if (error == DB_SUCCESS) {
		error = fts_merge_doc_ids(query, subexpr_doc_ids);
	}

	/* Free current result set. Result already merged into parent. */
	fts_query_free_doc_ids(query, subexpr_doc_ids);

	DBUG_RETURN(error);
}

#if 0
/*****************************************************************//***
Check if the doc id exists in the ilist.
@return TRUE if doc id found */
static
ulint
fts_query_find_doc_id(
/*==================*/
	fts_select_t*	select,		/*!< in/out: contains the doc id to
					find, we update the word freq if
					document found */
	void*		data,		/*!< in: doc id ilist */
	ulint		len)		/*!< in: doc id ilist size */
{
	byte*		ptr = data;
	doc_id_t	doc_id = 0;
	ulint		decoded = 0;

	/* Decode the ilist and search for selected doc_id. We also
	calculate the frequency of the word in the document if found. */
	while (decoded < len && !select->found) {
		ulint		freq = 0;
		ulint		min_pos = 0;
		ulint		last_pos = 0;
		ulint		pos = fts_decode_vlc(&ptr);

		/* Add the delta. */
		doc_id += pos;

		while (*ptr) {
			++freq;
			last_pos += fts_decode_vlc(&ptr);

			/* Only if min_pos is not set and the current
			term exists in a position greater than the
			min_pos of the previous term. */
			if (min_pos == 0 && last_pos > select->min_pos) {
				min_pos = last_pos;
			}
		}

		/* Skip the end of word position marker. */
		++ptr;

		/* Bytes decoded so far. */
		decoded = ptr - (byte*) data;

		/* A word may exist in the document but we only consider a
		match if it exists in a position that is greater than the
		position of the previous term. */
		if (doc_id == select->doc_id && min_pos > 0) {
			fts_doc_freq_t*	doc_freq;

			/* Add the doc id to the doc freq rb tree, if
			the doc id doesn't exist it will be created. */
			doc_freq = fts_query_add_doc_freq(
				select->word_freq->doc_freqs, doc_id);

			/* Avoid duplicating the frequency tally */
			if (doc_freq->freq == 0) {
				doc_freq->freq = freq;
			}

			select->found = TRUE;
			select->min_pos = min_pos;
		}
	}

	return(select->found);
}
#endif

/*****************************************************************//**
Read and filter nodes.
@return DB_SUCCESS if all go well,
or return DB_FTS_EXCEED_RESULT_CACHE_LIMIT */
static
dberr_t
fts_query_filter_doc_ids(
/*=====================*/
	fts_query_t*		query,		/*!< in: query instance */
	const fts_string_t*	word,		/*!< in: the current word */
	fts_word_freq_t*	word_freq,	/*!< in/out: word frequency */
	const fts_node_t*	node,		/*!< in: current FTS node */
	void*			data,		/*!< in: doc id ilist */
	ulint			len,		/*!< in: doc id ilist size */
	ibool			calc_doc_count)	/*!< in: whether to remember doc count */
{
	byte*		ptr = static_cast<byte*>(data);
	doc_id_t	doc_id = 0;
	ulint		decoded = 0;
	ib_rbt_t*	doc_freqs = word_freq->doc_freqs;

	if (query->limit != ULONG_UNDEFINED
	    && query->n_docs >= query->limit) {
		return(DB_SUCCESS);
	}

	/* Decode the ilist and add the doc ids to the query doc_id set. */
	while (decoded < len) {
		ulint		freq = 0;
		fts_doc_freq_t*	doc_freq;
		fts_match_t*	match = NULL;
		ulint		last_pos = 0;
		ulint		pos = fts_decode_vlc(&ptr);

		/* Some sanity checks. */
		if (doc_id == 0) {
			ut_a(pos == node->first_doc_id);
		}

		/* Add the delta. */
		doc_id += pos;

		if (calc_doc_count) {
			word_freq->doc_count++;
		}

		/* We simply collect the matching instances here. */
		if (query->collect_positions) {
			ib_alloc_t*	heap_alloc;

			/* Create a new fts_match_t instance. */
			match = static_cast<fts_match_t*>(
				ib_vector_push(query->matched, NULL));

			match->start = 0;
			match->doc_id = doc_id;
			heap_alloc = ib_vector_allocator(query->matched);

			/* Allocate from the same heap as the
			parent container. */
			match->positions = ib_vector_create(
				heap_alloc, sizeof(ulint), 64);

			query->total_size += sizeof(fts_match_t)
				+ sizeof(ib_vector_t)
				+ sizeof(ulint) * 64;
		}

		/* Unpack the positions within the document. */
		while (*ptr) {
			last_pos += fts_decode_vlc(&ptr);

			/* Collect the matching word positions, for phrase
			matching later. */
			if (query->collect_positions) {
				ib_vector_push(match->positions, &last_pos);
			}

			++freq;
		}

		/* End of list marker. */
		last_pos = (ulint) -1;

		if (query->collect_positions) {
			ut_a(match != NULL);
			ib_vector_push(match->positions, &last_pos);
		}

		/* Add the doc id to the doc freq rb tree, if the doc id
		doesn't exist it will be created. */
		doc_freq = fts_query_add_doc_freq(query, doc_freqs, doc_id);

		/* Avoid duplicating frequency tally. */
		if (doc_freq->freq == 0) {
			doc_freq->freq = freq;
		}

		/* Skip the end of word position marker. */
		++ptr;

		/* Bytes decoded so far */
		decoded = ptr - (byte*) data;

		/* We simply collect the matching documents and the
		positions here and match later. */
		if (!query->collect_positions) {
			/* We ignore error here and will check it later */
			fts_query_process_doc_id(query, doc_id, 0);

			/* Add the word to the document's matched RB tree. */
			fts_query_add_word_to_document(query, doc_id, word);
		}

		if (query->limit != ULONG_UNDEFINED
		    && query->limit <= ++query->n_docs) {
			goto func_exit;
		}
	}

	/* Some sanity checks. */
	ut_a(doc_id == node->last_doc_id);

func_exit:
	if (query->total_size > fts_result_cache_limit) {
		return(DB_FTS_EXCEED_RESULT_CACHE_LIMIT);
	} else {
		query->n_docs = 0;
		return(DB_SUCCESS);
	}
}

/*****************************************************************//**
Read the FTS INDEX row.
@return DB_SUCCESS if all go well. */
static
dberr_t
fts_query_read_node(
/*================*/
	fts_query_t*		query,	/*!< in: query instance */
	const fts_string_t*	word,	/*!< in: current word */
	que_node_t*		exp)	/*!< in: query graph node */
{
	int			i;
	int			ret;
	fts_node_t		node;
	ib_rbt_bound_t		parent;
	fts_word_freq_t*	word_freq;
	ibool			skip = FALSE;
	fts_string_t		term;
	byte			buf[FTS_MAX_WORD_LEN + 1];
	dberr_t			error = DB_SUCCESS;

	ut_a(query->cur_node->type == FTS_AST_TERM
	     || query->cur_node->type == FTS_AST_TEXT
	     || query->cur_node->type == FTS_AST_PARSER_PHRASE_LIST);

	memset(&node, 0, sizeof(node));
	term.f_str = buf;

	/* Need to consider the wildcard search case, the word frequency
	is created on the search string not the actual word. So we need
	to assign the frequency on search string behalf. */
	if (query->cur_node->type == FTS_AST_TERM
	    && query->cur_node->term.wildcard) {

		term.f_len = query->cur_node->term.ptr->len;
		ut_ad(FTS_MAX_WORD_LEN >= term.f_len);
		memcpy(term.f_str, query->cur_node->term.ptr->str, term.f_len);
	} else {
		term.f_len = word->f_len;
		ut_ad(FTS_MAX_WORD_LEN >= word->f_len);
		memcpy(term.f_str, word->f_str, word->f_len);
	}

	/* Lookup the word in our rb tree, it must exist. */
	ret = rbt_search(query->word_freqs, &parent, &term);

	ut_a(ret == 0);

	word_freq = rbt_value(fts_word_freq_t, parent.last);

	/* Start from 1 since the first column has been read by the caller.
	Also, we rely on the order of the columns projected, to filter
	out ilists that are out of range and we always want to read
	the doc_count irrespective of the suitablility of the row. */

	for (i = 1; exp && !skip; exp = que_node_get_next(exp), ++i) {

		dfield_t*	dfield = que_node_get_val(exp);
		byte*		data = static_cast<byte*>(
			dfield_get_data(dfield));
		ulint		len = dfield_get_len(dfield);

		ut_a(len != UNIV_SQL_NULL);

		/* Note: The column numbers below must match the SELECT. */

		switch (i) {
		case 1: /* DOC_COUNT */
			word_freq->doc_count += mach_read_from_4(data);
			break;

		case 2: /* FIRST_DOC_ID */
			node.first_doc_id = fts_read_doc_id(data);

			/* Skip nodes whose doc ids are out range. */
			if (query->oper == FTS_EXIST
			    && query->upper_doc_id > 0
			    && node.first_doc_id > query->upper_doc_id) {
				skip = TRUE;
			}
			break;

		case 3: /* LAST_DOC_ID */
			node.last_doc_id = fts_read_doc_id(data);

			/* Skip nodes whose doc ids are out range. */
			if (query->oper == FTS_EXIST
			    && query->lower_doc_id > 0
			    && node.last_doc_id < query->lower_doc_id) {
				skip = TRUE;
			}
			break;

		case 4: /* ILIST */

			error = fts_query_filter_doc_ids(
					query, &word_freq->word, word_freq,
					&node, data, len, FALSE);

			break;

		default:
			ut_error;
		}
	}

	if (!skip) {
		/* Make sure all columns were read. */

		ut_a(i == 5);
	}

	return error;
}

/*****************************************************************//**
Callback function to fetch the rows in an FTS INDEX record.
@return always returns TRUE */
static
ibool
fts_query_index_fetch_nodes(
/*========================*/
	void*		row,		/*!< in: sel_node_t* */
	void*		user_arg)	/*!< in: pointer to fts_fetch_t */
{
	fts_string_t	key;
	sel_node_t*	sel_node = static_cast<sel_node_t*>(row);
	fts_fetch_t*	fetch = static_cast<fts_fetch_t*>(user_arg);
	fts_query_t*	query = static_cast<fts_query_t*>(fetch->read_arg);
	que_node_t*	exp = sel_node->select_list;
	dfield_t*	dfield = que_node_get_val(exp);
	void*		data = dfield_get_data(dfield);
	ulint		dfield_len = dfield_get_len(dfield);

	key.f_str = static_cast<byte*>(data);
	key.f_len = dfield_len;

	ut_a(dfield_len <= FTS_MAX_WORD_LEN);

	/* Note: we pass error out by 'query->error' */
	query->error = fts_query_read_node(query, &key, que_node_get_next(exp));

	if (query->error != DB_SUCCESS) {
		ut_ad(query->error == DB_FTS_EXCEED_RESULT_CACHE_LIMIT);
		return(FALSE);
	} else {
		return(TRUE);
	}
}

/*****************************************************************//**
Calculate the inverse document frequency (IDF) for all the terms. */
static
void
fts_query_calculate_idf(
/*====================*/
	fts_query_t*	query)	/*!< in: Query state */
{
	const ib_rbt_node_t*	node;
	ib_uint64_t		total_docs = query->total_docs;

	/* We need to free any instances of fts_doc_freq_t that we
	may have allocated. */
	for (node = rbt_first(query->word_freqs);
	     node;
	     node = rbt_next(query->word_freqs, node)) {

		fts_word_freq_t*	word_freq;

		word_freq = rbt_value(fts_word_freq_t, node);

		if (word_freq->doc_count > 0) {
			if (total_docs == word_freq->doc_count) {
				/* QP assume ranking > 0 if we find
				a match. Since Log10(1) = 0, we cannot
				make IDF a zero value if do find a
				word in all documents. So let's make
				it an arbitrary very small number */
				word_freq->idf = log10(1.0001);
			} else {
				word_freq->idf = log10(
					total_docs
					/ (double) word_freq->doc_count);
			}
		}

		if (fts_enable_diag_print) {
			ib::info() << "'" << word_freq->word.f_str << "' -> "
				<< query->total_docs << "/"
				<< word_freq->doc_count << " "
				<< std::setw(6) << std::setprecision(5)
				<< word_freq->idf;
		}
	}
}

/*****************************************************************//**
Calculate the ranking of the document. */
static
void
fts_query_calculate_ranking(
/*========================*/
	const fts_query_t*	query,		/*!< in: query state */
	fts_ranking_t*		ranking)	/*!< in: Document to rank */
{
	ulint	pos = 0;
	fts_string_t	word;

	/* At this stage, ranking->rank should not exceed the 1.0
	bound */
	ut_ad(ranking->rank <= 1.0 && ranking->rank >= -1.0);
	ut_ad(rbt_size(query->word_map) == query->word_vector->size());

	while (fts_ranking_words_get_next(query, ranking, &pos, &word)) {
		int			ret;
		ib_rbt_bound_t		parent;
		double			weight;
		fts_doc_freq_t*		doc_freq;
		fts_word_freq_t*	word_freq;

		ret = rbt_search(query->word_freqs, &parent, &word);

		/* It must exist. */
		ut_a(ret == 0);

		word_freq = rbt_value(fts_word_freq_t, parent.last);

		ret = rbt_search(
			word_freq->doc_freqs, &parent, &ranking->doc_id);

		/* It must exist. */
		ut_a(ret == 0);

		doc_freq = rbt_value(fts_doc_freq_t, parent.last);

		weight = (double) doc_freq->freq * word_freq->idf;

		ranking->rank += (fts_rank_t) (weight * word_freq->idf);
	}
}

/*****************************************************************//**
Add ranking to the result set. */
static
void
fts_query_add_ranking(
/*==================*/
	fts_query_t*		query,		/*!< in: query state */
	ib_rbt_t*		ranking_tree,	/*!< in: ranking tree */
	const fts_ranking_t*	new_ranking)	/*!< in: ranking of a document */
{
	ib_rbt_bound_t		parent;

	/* Lookup the ranking in our rb tree and add if it doesn't exist. */
	if (rbt_search(ranking_tree, &parent, new_ranking) == 0) {
		fts_ranking_t*	ranking;

		ranking = rbt_value(fts_ranking_t, parent.last);

		ranking->rank += new_ranking->rank;

		ut_a(ranking->words == NULL);
	} else {
		rbt_add_node(ranking_tree, &parent, new_ranking);

		query->total_size += SIZEOF_RBT_NODE_ADD
			+ sizeof(fts_ranking_t);
	}
}

/*****************************************************************//**
Retrieve the FTS Relevance Ranking result for doc with doc_id
@return the relevance ranking value, 0 if no ranking value
present. */
float
fts_retrieve_ranking(
/*=================*/
	fts_result_t*	result,	/*!< in: FTS result structure */
	doc_id_t	doc_id)	/*!< in: doc_id of the item to retrieve */
{
	ib_rbt_bound_t		parent;
	fts_ranking_t		new_ranking;

	DBUG_ENTER("fts_retrieve_ranking");

	if (!result || !result->rankings_by_id) {
		DBUG_RETURN(0);
	}

	new_ranking.doc_id = doc_id;

	/* Lookup the ranking in our rb tree */
	if (rbt_search(result->rankings_by_id, &parent, &new_ranking) == 0) {
		fts_ranking_t*  ranking;

		ranking = rbt_value(fts_ranking_t, parent.last);

		DBUG_RETURN(ranking->rank);
	}

	DBUG_RETURN(0);
}

/*****************************************************************//**
Create the result and copy the data to it. */
static
fts_result_t*
fts_query_prepare_result(
/*=====================*/
	fts_query_t*	query,	/*!< in: Query state */
	fts_result_t*	result)	/*!< in: result this can contain
				data from a previous search on
				another FTS index */
{
	const ib_rbt_node_t*	node;
	bool			result_is_null = false;

	DBUG_ENTER("fts_query_prepare_result");

	if (result == NULL) {
		result = static_cast<fts_result_t*>(
			ut_zalloc_nokey(sizeof(*result)));

		result->rankings_by_id = rbt_create(
			sizeof(fts_ranking_t), fts_ranking_doc_id_cmp);

		query->total_size += sizeof(fts_result_t) + SIZEOF_RBT_CREATE;
		result_is_null = true;
	}

	if (query->flags == FTS_OPT_RANKING) {
		fts_word_freq_t*	word_freq;
		ulint		size = ib_vector_size(query->deleted->doc_ids);
		fts_update_t*	array =
			(fts_update_t*) query->deleted->doc_ids->data;

		node = rbt_first(query->word_freqs);
		ut_ad(node);
		word_freq = rbt_value(fts_word_freq_t, node);

		for (node = rbt_first(word_freq->doc_freqs);
		     node;
		     node = rbt_next(word_freq->doc_freqs, node)) {
			fts_doc_freq_t* doc_freq;
			fts_ranking_t	ranking;

			doc_freq = rbt_value(fts_doc_freq_t, node);

			/* Don't put deleted docs into result */
			if (fts_bsearch(array, 0, static_cast<int>(size),
					doc_freq->doc_id) >= 0) {
				/* one less matching doc count */
				--word_freq->doc_count;
				continue;
			}

			ranking.doc_id = doc_freq->doc_id;
			ranking.rank = static_cast<fts_rank_t>(doc_freq->freq);
			ranking.words = NULL;

			fts_query_add_ranking(query, result->rankings_by_id,
					      &ranking);

			if (query->total_size > fts_result_cache_limit) {
				query->error = DB_FTS_EXCEED_RESULT_CACHE_LIMIT;
				fts_query_free_result(result);
				DBUG_RETURN(NULL);
			}
		}

		/* Calculate IDF only after we exclude the deleted items */
		fts_query_calculate_idf(query);

		node = rbt_first(query->word_freqs);
		word_freq = rbt_value(fts_word_freq_t, node);

		/* Calculate the ranking for each doc */
		for (node = rbt_first(result->rankings_by_id);
		     node != NULL;
		     node = rbt_next(result->rankings_by_id, node)) {

			fts_ranking_t*  ranking;

			ranking = rbt_value(fts_ranking_t, node);

			ranking->rank = static_cast<fts_rank_t>(
				ranking->rank * word_freq->idf * word_freq->idf);
		}

		DBUG_RETURN(result);
	}

	ut_a(rbt_size(query->doc_ids) > 0);

	for (node = rbt_first(query->doc_ids);
	     node;
	     node = rbt_next(query->doc_ids, node)) {

		fts_ranking_t*	ranking;

		ranking = rbt_value(fts_ranking_t, node);
		fts_query_calculate_ranking(query, ranking);

		// FIXME: I think we may requre this information to improve the
		// ranking of doc ids which have more word matches from
		// different FTS indexes.

		/* We don't need these anymore free the resources. */
		ranking->words = NULL;

		if (!result_is_null) {
			fts_query_add_ranking(query, result->rankings_by_id, ranking);

			 if (query->total_size > fts_result_cache_limit) {
				query->error = DB_FTS_EXCEED_RESULT_CACHE_LIMIT;
				fts_query_free_result(result);
				DBUG_RETURN(NULL);
                        }
		}
	}

	if (result_is_null) {
		/* Use doc_ids directly */
		rbt_free(result->rankings_by_id);
		result->rankings_by_id = query->doc_ids;
		query->doc_ids = NULL;
	}

	DBUG_RETURN(result);
}

/*****************************************************************//**
Get the result of the query. Calculate the similarity coefficient. */
static
fts_result_t*
fts_query_get_result(
/*=================*/
	fts_query_t*		query,	/*!< in: query instance */
	fts_result_t*		result)	/*!< in: result */
{
	DBUG_ENTER("fts_query_get_result");

	if (rbt_size(query->doc_ids) > 0 || query->flags == FTS_OPT_RANKING) {
		/* Copy the doc ids to the result. */
		result = fts_query_prepare_result(query, result);
	} else {
		/* Create an empty result instance. */
		result = static_cast<fts_result_t*>(
			ut_zalloc_nokey(sizeof(*result)));
	}

	DBUG_RETURN(result);
}

/*****************************************************************//**
FTS Query free resources and reset. */
static
void
fts_query_free(
/*===========*/
	fts_query_t*	query)		/*!< in: query instance to free*/
{

	if (query->read_nodes_graph) {
		fts_que_graph_free(query->read_nodes_graph);
	}

	if (query->root) {
		fts_ast_free_node(query->root);
	}

	if (query->deleted) {
		fts_doc_ids_free(query->deleted);
	}

	if (query->intersection) {
		fts_query_free_doc_ids(query, query->intersection);
		query->intersection = NULL;
	}

	if (query->doc_ids) {
		fts_query_free_doc_ids(query, query->doc_ids);
	}

	if (query->word_freqs) {
		const ib_rbt_node_t*	node;

		/* We need to free any instances of fts_doc_freq_t that we
		may have allocated. */
		for (node = rbt_first(query->word_freqs);
		     node;
		     node = rbt_next(query->word_freqs, node)) {

			fts_word_freq_t*	word_freq;

			word_freq = rbt_value(fts_word_freq_t, node);

			/* We need to cast away the const. */
			rbt_free(word_freq->doc_freqs);
		}

		rbt_free(query->word_freqs);
	}

	if (query->wildcard_words != NULL) {
		rbt_free(query->wildcard_words);
	}

	ut_a(!query->intersection);

	if (query->word_map) {
		rbt_free(query->word_map);
	}

	if (query->word_vector != NULL) {
		UT_DELETE(query->word_vector);
	}

	if (query->heap) {
		mem_heap_free(query->heap);
	}

	memset(query, 0, sizeof(*query));
}

/*****************************************************************//**
Parse the query using flex/bison or plugin parser.
@return parse tree node. */
static
fts_ast_node_t*
fts_query_parse(
/*============*/
	fts_query_t*	query,		/*!< in: query instance */
	byte*		query_str,	/*!< in: query string */
	ulint		query_len)	/*!< in: query string length */
{
	int		error;
	fts_ast_state_t state;
	bool		mode = query->boolean_mode;
	DBUG_ENTER("fts_query_parse");

	memset(&state, 0x0, sizeof(state));

	state.charset = query->fts_index_table.charset;

	DBUG_EXECUTE_IF("fts_instrument_query_disable_parser",
		query->parser = NULL;);

	if (query->parser) {
		state.root = state.cur_node =
			fts_ast_create_node_list(&state, NULL);
		error = fts_parse_by_parser(mode, query_str, query_len,
					    query->parser, &state);
	} else {
		/* Setup the scanner to use, this depends on the mode flag. */
		state.lexer = fts_lexer_create(mode, query_str, query_len);
		state.charset = query->fts_index_table.charset;
		error = fts_parse(&state);
		fts_lexer_free(state.lexer);
		state.lexer = NULL;
	}

	/* Error during parsing ? */
	if (error) {
		/* Free the nodes that were allocated during parsing. */
		fts_ast_state_free(&state);
	} else {
		query->root = state.root;

		if (fts_enable_diag_print && query->root != NULL) {
			fts_ast_node_print(query->root);
		}
	}

	DBUG_RETURN(state.root);
}

/*******************************************************************//**
FTS Query optimization
Set FTS_OPT_RANKING if it is a simple term query */
static
void
fts_query_can_optimize(
/*===================*/
	fts_query_t*	query,		/*!< in/out: query instance */
	uint		flags)		/*!< In: FTS search mode */
{
	fts_ast_node_t*	node = query->root;

	if (flags & FTS_EXPAND) {
		return;
	}

	/* Check if it has only a term without oper */
	ut_ad(node->type == FTS_AST_LIST);
	node = node->list.head;
	if (node != NULL && node->type == FTS_AST_TERM && node->next == NULL) {
		query->flags = FTS_OPT_RANKING;
	}
}



/** FTS Query entry point.
@param[in]	trx		transaction
@param[in]	index		fts index to search
@param[in]	flags		FTS search mode
@param[in]	query_str	FTS query
@param[in]	query_len	FTS query string len in bytes
@param[in,out]	result		result doc ids
@param[in]	limit		limit value
@return DB_SUCCESS if successful otherwise error code */
dberr_t
fts_query(
	trx_t*		trx,
	dict_index_t*	index,
	uint		flags,
	const byte*	query_str,
	ulint		query_len,
	fts_result_t**	result,
	ulonglong	limit)
{
	fts_query_t	query;
	dberr_t		error = DB_SUCCESS;
	byte*		lc_query_str;
	ulint		lc_query_str_len;
	ulint		result_len;
	bool		boolean_mode;
	trx_t*		query_trx;
	CHARSET_INFO*	charset;
	ulint		start_time_ms;
	bool		will_be_ignored = false;

	boolean_mode = flags & FTS_BOOL;

	*result = NULL;
	memset(&query, 0x0, sizeof(query));
	query_trx = trx_allocate_for_background();
	query_trx->op_info = "FTS query";

	start_time_ms = ut_time_ms();

	query.trx = query_trx;
	query.index = index;
	query.boolean_mode = boolean_mode;
	query.deleted = fts_doc_ids_create();
	query.cur_node = NULL;

	query.fts_common_table.type = FTS_COMMON_TABLE;
	query.fts_common_table.table_id = index->table->id;
	query.fts_common_table.parent = index->table->name.m_name;
	query.fts_common_table.table = index->table;

	charset = fts_index_get_charset(index);

	query.fts_index_table.type = FTS_INDEX_TABLE;
	query.fts_index_table.index_id = index->id;
	query.fts_index_table.table_id = index->table->id;
	query.fts_index_table.parent = index->table->name.m_name;
	query.fts_index_table.charset = charset;
	query.fts_index_table.table = index->table;

	query.word_map = rbt_create_arg_cmp(
		sizeof(fts_string_t), innobase_fts_text_cmp, charset);
	query.word_vector = UT_NEW_NOKEY(word_vector_t());
	query.error = DB_SUCCESS;

	/* Setup the RB tree that will be used to collect per term
	statistics. */
	query.word_freqs = rbt_create_arg_cmp(
		sizeof(fts_word_freq_t), innobase_fts_text_cmp, charset);

	if (flags & FTS_EXPAND) {
		query.wildcard_words = rbt_create_arg_cmp(
			sizeof(fts_string_t), innobase_fts_text_cmp, charset);
	}

	query.total_size += SIZEOF_RBT_CREATE;

	query.total_docs = dict_table_get_n_rows(index->table);

	query.limit = limit;

	query.n_docs = 0;
#ifdef FTS_DOC_STATS_DEBUG
	if (ft_enable_diag_print) {
		error = fts_get_total_word_count(
			trx, query.index, &query.total_words);

		if (error != DB_SUCCESS) {
			goto func_exit;
		}

		ib::info() << "Total docs: " << query.total_docs
			<< " Total words: " << query.total_words;
	}
#endif /* FTS_DOC_STATS_DEBUG */

	query.fts_common_table.suffix = "DELETED";

	/* Read the deleted doc_ids, we need these for filtering. */
	error = fts_table_fetch_doc_ids(
		NULL, &query.fts_common_table, query.deleted);

	if (error != DB_SUCCESS) {
		goto func_exit;
	}

	query.fts_common_table.suffix = "DELETED_CACHE";

	error = fts_table_fetch_doc_ids(
		NULL, &query.fts_common_table, query.deleted);

	if (error != DB_SUCCESS) {
		goto func_exit;
	}

	/* Get the deleted doc ids that are in the cache. */
	fts_cache_append_deleted_doc_ids(
		index->table->fts->cache, query.deleted->doc_ids);
	DEBUG_SYNC_C("fts_deleted_doc_ids_append");

	/* Sort the vector so that we can do a binary search over the ids. */
	ib_vector_sort(query.deleted->doc_ids, fts_update_doc_id_cmp);

	/* Convert the query string to lower case before parsing. We own
	the ut_malloc'ed result and so remember to free it before return. */

	lc_query_str_len = query_len * charset->casedn_multiply + 1;
	lc_query_str = static_cast<byte*>(ut_malloc_nokey(lc_query_str_len));

	/* For binary collations, a case sensitive search is
	performed. Hence don't convert to lower case. */
	if (my_binary_compare(charset)) {
	memcpy(lc_query_str, query_str, query_len);
		lc_query_str[query_len]= 0;
		result_len= query_len;
	} else {
	result_len = innobase_fts_casedn_str(
				charset, (char*)( query_str), query_len,
				(char*)(lc_query_str), lc_query_str_len);
	}

	ut_ad(result_len < lc_query_str_len);

	lc_query_str[result_len] = 0;

	query.heap = mem_heap_create(128);

	/* Create the rb tree for the doc id (current) set. */
	query.doc_ids = rbt_create(
		sizeof(fts_ranking_t), fts_ranking_doc_id_cmp);
	query.parser = index->parser;

	query.total_size += SIZEOF_RBT_CREATE;

	/* Parse the input query string. */
	if (fts_query_parse(&query, lc_query_str, result_len)) {
		fts_ast_node_t*	ast = query.root;
		ast->trx = trx;

		/* Optimize query to check if it's a single term */
		fts_query_can_optimize(&query, flags);

		DBUG_EXECUTE_IF("fts_instrument_result_cache_limit",
			        fts_result_cache_limit = 2048;
		);

		/* Optimisation is allowed for limit value
		when
		i)  No ranking involved
		ii) Only FTS Union operations involved. */
		if (query.limit != ULONG_UNDEFINED
		    && !fts_ast_node_check_union(ast)) {
			query.limit = ULONG_UNDEFINED;
		}

		DBUG_EXECUTE_IF("fts_union_limit_off",
			query.limit = ULONG_UNDEFINED;
		);

		/* Traverse the Abstract Syntax Tree (AST) and execute
		the query. */
		query.error = fts_ast_visit(
			FTS_NONE, ast, fts_query_visitor,
			&query, &will_be_ignored);
		if (query.error == DB_INTERRUPTED) {
			error = DB_INTERRUPTED;
			goto func_exit;
		}

		/* If query expansion is requested, extend the search
		with first search pass result */
		if (query.error == DB_SUCCESS && (flags & FTS_EXPAND)) {
			query.error = fts_expand_query(index, &query);
		}

		/* Calculate the inverse document frequency of the terms. */
		if (query.error == DB_SUCCESS
		    && query.flags != FTS_OPT_RANKING) {
			fts_query_calculate_idf(&query);
		}

		/* Copy the result from the query state, so that we can
		return it to the caller. */
		if (query.error == DB_SUCCESS) {
			*result = fts_query_get_result(&query, *result);
		}

		error = query.error;
	} else {
		/* still return an empty result set */
		*result = static_cast<fts_result_t*>(
			ut_zalloc_nokey(sizeof(**result)));
	}

	if (trx_is_interrupted(trx)) {
		error = DB_INTERRUPTED;
		goto func_exit;
	}

	ut_free(lc_query_str);

	if (fts_enable_diag_print && (*result)) {
		ulint	diff_time = ut_time_ms() - start_time_ms;

		ib::info() << "FTS Search Processing time: "
			<< diff_time / 1000 << " secs: " << diff_time % 1000
			<< " millisec: row(s) "
			<< ((*result)->rankings_by_id
			    ? rbt_size((*result)->rankings_by_id)
			    : -1);

		/* Log memory consumption & result size */
		ib::info() << "Full Search Memory: " << query.total_size
			<< " (bytes),  Row: "
			<< ((*result)->rankings_by_id
			    ? rbt_size((*result)->rankings_by_id)
			    : 0)
			<< ".";
	}

func_exit:
	fts_query_free(&query);

	trx_free_for_background(query_trx);

	return(error);
}

/*****************************************************************//**
FTS Query free result, returned by fts_query(). */
void
fts_query_free_result(
/*==================*/
	fts_result_t*	result)		/*!< in: result instance to free.*/
{
	if (result) {
		if (result->rankings_by_id != NULL) {
			rbt_free(result->rankings_by_id);
			result->rankings_by_id = NULL;
		}
		if (result->rankings_by_rank != NULL) {
			rbt_free(result->rankings_by_rank);
			result->rankings_by_rank = NULL;
		}

		ut_free(result);
		result = NULL;
	}
}

/*****************************************************************//**
FTS Query sort result, returned by fts_query() on fts_ranking_t::rank. */
void
fts_query_sort_result_on_rank(
/*==========================*/
	fts_result_t*	result)		/*!< out: result instance to sort.*/
{
	const ib_rbt_node_t*	node;
	ib_rbt_t*		ranked;

	ut_a(result->rankings_by_id != NULL);
	if (result->rankings_by_rank) {
		rbt_free(result->rankings_by_rank);
	}

	ranked = rbt_create(sizeof(fts_ranking_t), fts_query_compare_rank);

	/* We need to free any instances of fts_doc_freq_t that we
	may have allocated. */
	for (node = rbt_first(result->rankings_by_id);
	     node;
	     node = rbt_next(result->rankings_by_id, node)) {

		fts_ranking_t*	ranking;

		ranking = rbt_value(fts_ranking_t, node);

		ut_a(ranking->words == NULL);

		rbt_insert(ranked, ranking, ranking);
	}

	/* Reset the current node too. */
	result->current = NULL;
	result->rankings_by_rank = ranked;
}

/*******************************************************************//**
A debug function to print result doc_id set. */
static
void
fts_print_doc_id(
/*=============*/
	fts_query_t*	query)	/*!< in : tree that stores doc_ids.*/
{
	const ib_rbt_node_t*	node;

	/* Iterate each member of the doc_id set */
	for (node = rbt_first(query->doc_ids);
	     node;
	     node = rbt_next(query->doc_ids, node)) {
		fts_ranking_t*	ranking;
		ranking = rbt_value(fts_ranking_t, node);

		ib::info() << "doc_ids info, doc_id: " << ranking->doc_id;

		ulint		pos = 0;
		fts_string_t	word;

		while (fts_ranking_words_get_next(query, ranking, &pos, &word)) {
			ib::info() << "doc_ids info, value: " << word.f_str;
		}
	}
}

/*************************************************************//**
This function implements a simple "blind" query expansion search:
words in documents found in the first search pass will be used as
search arguments to search the document again, thus "expand"
the search result set.
@return DB_SUCCESS if success, otherwise the error code */
static MY_ATTRIBUTE((nonnull, warn_unused_result))
dberr_t
fts_expand_query(
/*=============*/
	dict_index_t*	index,		/*!< in: FTS index to search */
	fts_query_t*	query)		/*!< in: FTS query instance */
{
	const ib_rbt_node_t*	node;
	const ib_rbt_node_t*	token_node;
	fts_doc_t		result_doc;
	dberr_t			error = DB_SUCCESS;
	const fts_index_cache_t*index_cache;

	/* If no doc is found in first search pass, return */
	if (!rbt_size(query->doc_ids)) {
		return(error);
	}

	/* Init "result_doc", to hold words from the first search pass */
	fts_doc_init(&result_doc);

	rw_lock_x_lock(&index->table->fts->cache->lock);
	index_cache = fts_find_index_cache(index->table->fts->cache, index);
	rw_lock_x_unlock(&index->table->fts->cache->lock);

	ut_a(index_cache);

	result_doc.tokens = rbt_create_arg_cmp(
		sizeof(fts_token_t), innobase_fts_text_cmp,
		index_cache->charset);

	result_doc.charset = index_cache->charset;
	result_doc.parser = index_cache->index->parser;
	result_doc.is_ngram = index_cache->index->is_ngram;

	query->total_size += SIZEOF_RBT_CREATE;

	if (fts_enable_diag_print) {
		fts_print_doc_id(query);
	}

	for (node = rbt_first(query->doc_ids);
	     node;
	     node = rbt_next(query->doc_ids, node)) {

		fts_ranking_t*	ranking;
		ulint		prev_token_size;
		ulint		estimate_size;

		prev_token_size = rbt_size(result_doc.tokens);

		ranking = rbt_value(fts_ranking_t, node);

		/* Fetch the documents with the doc_id from the
		result of first seach pass. Since we do not
		store document-to-word mapping, we need to
		fetch the original document and parse them.
		Future optimization could be done here if we
		support some forms of document-to-word mapping */
		fts_doc_fetch_by_doc_id(NULL, ranking->doc_id, index,
					FTS_FETCH_DOC_BY_ID_EQUAL,
					fts_query_expansion_fetch_doc,
					&result_doc);

		/* Estimate memory used, see fts_process_token and fts_token_t.
		   We ignore token size here. */
		estimate_size = (rbt_size(result_doc.tokens) - prev_token_size)
			* (SIZEOF_RBT_NODE_ADD + sizeof(fts_token_t)
			+ sizeof(ib_vector_t) + sizeof(ulint) * 32);
		query->total_size += estimate_size;

		if (query->total_size > fts_result_cache_limit) {
			error = DB_FTS_EXCEED_RESULT_CACHE_LIMIT;
			goto	func_exit;
		}
	}

	/* Remove words that have already been searched in the first pass */
	for (ulint i = 0; i < query->word_vector->size(); i++) {
		fts_string_t	word = query->word_vector->at(i);
		ib_rbt_bound_t	parent;

		if (query->wildcard_words
		    && rbt_search(query->wildcard_words, &parent, &word) == 0) {
			/* If it's a wildcard word, remove words having
			it as prefix. */
			while (rbt_search_cmp(result_doc.tokens,
					      &parent, &word, NULL,
					      innobase_fts_text_cmp_prefix)
			       == 0) {
				ut_free(rbt_remove_node(result_doc.tokens,
							parent.last));
			}
		} else {
			/* We don't check return value, because the word may
			have been deleted by a previous wildcard word as its
			prefix, e.g. ('g * good'). */
			rbt_delete(result_doc.tokens, &word);
		}
	}

	/* Search the table the second time with expanded search list */
	for (token_node = rbt_first(result_doc.tokens);
	     token_node;
	     token_node = rbt_next(result_doc.tokens, token_node)) {
		fts_token_t*	mytoken;
		mytoken = rbt_value(fts_token_t, token_node);

		/* '%' in the end is treated as prefix search,
		it can cause assert failure, so we skip it. */
		if (mytoken->text.f_str[mytoken->text.f_len - 1] == '%') {
			continue;
		}

		ut_ad(mytoken->text.f_str[mytoken->text.f_len] == 0);
		fts_query_add_word_freq(query, &mytoken->text);
		error = fts_query_union(query, &mytoken->text);

		if (error != DB_SUCCESS) {
			break;
		}
	}

func_exit:
	fts_doc_free(&result_doc);

	return(error);
}
/*************************************************************//**
This function finds documents that contain all words in a
phrase or proximity search. And if proximity search, verify
the words are close enough to each other, as in specified distance.
This function is called for phrase and proximity search.
@return TRUE if documents are found, FALSE if otherwise */
static
ibool
fts_phrase_or_proximity_search(
/*===========================*/
	fts_query_t*	query,		/*!< in/out:  query instance.
					query->doc_ids might be instantiated
					with qualified doc IDs */
	ib_vector_t*	tokens)		/*!< in: Tokens contain words */
{
	ulint		n_matched;
	ulint		i;
	ibool		matched = FALSE;
	ulint		num_token = ib_vector_size(tokens);
	fts_match_t*	match[MAX_PROXIMITY_ITEM];
	ibool		end_list = FALSE;

	/* Number of matched documents for the first token */
	n_matched = ib_vector_size(query->match_array[0]);

	/* We have a set of match list for each word, we shall
	walk through the list and find common documents that
	contain all the matching words. */
	for (i = 0; i < n_matched; i++) {
		ulint		j;
		ulint		k = 0;
		fts_proximity_t	qualified_pos;

		match[0] = static_cast<fts_match_t*>(
			ib_vector_get(query->match_array[0], i));

		/* For remaining match list for the token(word), we
		try to see if there is a document with the same
		doc id */
		for (j = 1; j < num_token; j++) {
			match[j] = static_cast<fts_match_t*>(
				ib_vector_get(query->match_array[j], k));

			while (match[j]->doc_id < match[0]->doc_id
			       && k < ib_vector_size(query->match_array[j])) {
				 match[j] = static_cast<fts_match_t*>(
					ib_vector_get(
						query->match_array[j], k));
				k++;
			}

			if (match[j]->doc_id > match[0]->doc_id) {
				/* no match */
				if (query->flags & FTS_PHRASE) {
					match[0]->doc_id = 0;
				}
				break;
			}

			if (k == ib_vector_size(query->match_array[j])) {
				end_list = TRUE;

				if (match[j]->doc_id != match[0]->doc_id) {
					/* no match */
					if (query->flags & FTS_PHRASE) {
						ulint	s;

						match[0]->doc_id = 0;

						for (s = i + 1; s < n_matched;
						     s++) {
							match[0] = static_cast<
							fts_match_t*>(
							ib_vector_get(
							query->match_array[0],
							s));
							match[0]->doc_id = 0;
						}
					}

					goto func_exit;
				}
			}

			/* FIXME: A better solution will be a counter array
			remember each run's last position. So we don't
			reset it here very time */
			k = 0;
		}

		if (j != num_token) {
			continue;
		}

		/* For this matching doc, we need to further
		verify whether the words in the doc are close
		to each other, and within the distance specified
		in the proximity search */
		if (query->flags & FTS_PHRASE) {
			matched = TRUE;
		} else if (fts_proximity_get_positions(
			match, num_token, ULINT_MAX, &qualified_pos)) {

			/* Fetch the original documents and count the
			words in between matching words to see that is in
			specified distance */
			if (fts_query_is_in_proximity_range(
				query, match, &qualified_pos)) {
				/* If so, mark we find a matching doc */
				query->error = fts_query_process_doc_id(
					query, match[0]->doc_id, 0);
				if (query->error != DB_SUCCESS) {
					matched = FALSE;
					goto func_exit;
				}

				matched = TRUE;
				for (ulint z = 0; z < num_token; z++) {
					fts_string_t*	token;
					token = static_cast<fts_string_t*>(
						ib_vector_get(tokens, z));
					fts_query_add_word_to_document(
						query, match[0]->doc_id, token);
				}
			}
		}

		if (end_list) {
			break;
		}
	}

func_exit:
	return(matched);
}

/*************************************************************//**
This function checks whether words in result documents are close to
each other (within proximity range as specified by "distance").
If "distance" is MAX_ULINT, then it will find all combinations of
positions of matching words and store min and max positions
in the "qualified_pos" for later verification.
@return true if words are close to each other, false if otherwise */
static
bool
fts_proximity_get_positions(
/*========================*/
	fts_match_t**		match,		/*!< in: query instance */
	ulint			num_match,	/*!< in: number of matching
						items */
	ulint			distance,	/*!< in: distance value
						for proximity search */
	fts_proximity_t*	qualified_pos)	/*!< out: the position info
						records ranges containing
						all matching words. */
{
	ulint	i;
	ulint	idx[MAX_PROXIMITY_ITEM];
	ulint	num_pos[MAX_PROXIMITY_ITEM];
	ulint	min_idx;

	qualified_pos->n_pos = 0;

	ut_a(num_match <= MAX_PROXIMITY_ITEM);

	/* Each word could appear multiple times in a doc. So
	we need to walk through each word's position list, and find
	closest distance between different words to see if
	they are in the proximity distance. */

	/* Assume each word's position list is sorted, we
	will just do a walk through to all words' lists
	similar to a the merge phase of a merge sort */
	for (i = 0; i < num_match; i++) {
		/* idx is the current position we are checking
		for a particular word */
		idx[i] = 0;

		/* Number of positions for this word */
		num_pos[i] = ib_vector_size(match[i]->positions);
	}

	/* Start with the first word */
	min_idx = 0;

	while (idx[min_idx] < num_pos[min_idx]) {
		ulint	position[MAX_PROXIMITY_ITEM];
		ulint	min_pos = ULINT_MAX;
		ulint	max_pos = 0;

		/* Check positions in each word position list, and
		record the max/min position */
		for (i = 0; i < num_match; i++) {
			position[i] = *(ulint*) ib_vector_get_const(
				match[i]->positions, idx[i]);

			if (position[i] == ULINT_UNDEFINED) {
				break;
			}

			if (position[i] < min_pos) {
				min_pos = position[i];
				min_idx = i;
			}

			if (position[i] > max_pos) {
				max_pos = position[i];
			}
		}

		/* If max and min position are within range, we
		find a good match */
		if (max_pos - min_pos <= distance
		    && (i >= num_match || position[i] != ULINT_UNDEFINED)) {
			/* The charset has variable character
			length encoding, record the min_pos and
			max_pos, we will need to verify the actual
			number of characters */
			qualified_pos->min_pos.push_back(min_pos);
			qualified_pos->max_pos.push_back(max_pos);
			qualified_pos->n_pos++;
		}

		/* Otherwise, move to the next position is the
		list for the word with the smallest position */
		idx[min_idx]++;
	}

	return(qualified_pos->n_pos != 0);
}
