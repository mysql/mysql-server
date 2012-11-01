/*****************************************************************************

Copyright (c) 2007, 2012, Oracle and/or its affiliates. All Rights Reserved.

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

#include "dict0dict.h" /* dict_table_get_n_rows() */
#include "ut0rbt.h"
#include "row0sel.h"
#include "fts0fts.h"
#include "fts0priv.h"
#include "fts0ast.h"
#include "fts0pars.h"
#include "fts0types.h"
#include "ha_prototypes.h"
#include <ctype.h>

#ifndef UNIV_NONINL
#include "fts0types.ic"
#include "fts0vlc.ic"
#endif

#define FTS_ELEM(t, n, i, j) (t[(i) * n + (j)])

#define RANK_DOWNGRADE		(-1.0F)
#define RANK_UPGRADE		(1.0F)

/* Maximum number of words supported in a proximity search.
FIXME, this limitation can be removed easily. Need to see
if we want to enforce such limitation */
#define MAX_PROXIMITY_ITEM	128

/* Coeffecient to use for normalize relevance ranking. */
static const double FTS_NORMALIZE_COEFF = 0.0115F;

// FIXME: Need to have a generic iterator that traverses the ilist.

/* For parsing the search phrase */
static const char* FTS_PHRASE_DELIMITER = "\t ";

struct fts_word_freq_t;

/** State of an FTS query. */
struct fts_query_t {
	mem_heap_t*	heap;		/*!< Heap to use for allocations */

	trx_t*		trx;		/*!< The query transaction */

	dict_index_t*	index;		/*!< The FTS index to search */
					/*!< FTS auxiliary common table def */
	fts_table_t	fts_common_table;

	fts_table_t	fts_index_table;/*!< FTS auxiliary index table def */

	fts_doc_ids_t*	deleted;	/*!< Deleted doc ids that need to be
					filtered from the output */

	fts_ast_node_t*	root;		/*!< Abstract syntax tree */

	fts_ast_node_t* cur_node;	/*!< Current tree node */

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

	ibool		boolean_mode;	/*!< TRUE if boolean mode query */

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

	ibool		inited;		/*!< Flag to test whether the query
					processing has started or not */
	ibool		multi_exist;	/*!< multiple FTS_EXIST oper */
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

/** The match positions and tokesn to match */
struct fts_phrase_t {
	ibool		found;		/*!< Match result */

	const fts_match_t*
			match;		/*!< Positions within text */

	const ib_vector_t*
			tokens;		/*!< Tokens to match */

	ulint		distance;	/*!< For matching on proximity
					distance. Can be 0 for exact match */
	CHARSET_INFO*	charset;	/*!< Phrase match charset */
	mem_heap_t*     heap;		/*!< Heap for word processing */
	ulint		zip_size;	/*!< row zip size */
};

/** For storing the frequncy of a word/term in a document */
struct fts_doc_freq_t {
	doc_id_t	doc_id;		/*!< Document id */
	ulint		freq;		/*!< Frequency of a word in a document */
};

/** To determine the word frequency per document. */
struct fts_word_freq_t {
	byte*		word;		/*!< Word for which we need the freq,
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
void
fts_query_filter_doc_ids(
/*=====================*/
	fts_query_t*	query,		/*!< in: query instance */
	const byte*	word,		/*!< in: the current word */
	fts_word_freq_t*word_freq,	/*!< in/out: word frequency */
	const fts_node_t*
			node,		/*!< in: current FTS node */
	void*		data,		/*!< in: doc id ilist */
	ulint		len,		/*!< in: doc id ilist size */
	ibool		calc_doc_count);/*!< in: whether to remember doc
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
	__attribute__((nonnull, warn_unused_result));
/*************************************************************//**
This function finds documents that contain all words in a
phrase or proximity search. And if proximity search, verify
the words are close to each other enough, as in specified distance.
This function is called for phrase and proximity search.
@return TRUE if documents are found, FALSE if otherwise */
static
ibool
fts_check_phrase_proximity(
/*=======================*/
	fts_query_t*	query,		/*!< in:  query instance */
	ib_vector_t*	tokens);	/*!< in: Tokens contain words */
/*************************************************************//**
This function check the words in result document are close to each
other enough (within proximity rnage). This is used for proximity search.
@return TRUE if words are close to each other, FALSE if otherwise */
static
ulint
fts_proximity_check_position(
/*=========================*/
	fts_match_t**	match,		/*!< in: query instance */
	ulint		num_match,	/*!< in: number of matching
					items */
	ulint		distance);	/*!< in: distance value
					for proximity search */
#if 0
/********************************************************************
Get the total number of words in a documents. */
static
ulint
fts_query_terms_in_document(
/*========================*/
					/*!< out: DB_SUCCESS if all went well
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
	ulint*	table = (ulint*) ut_malloc(size);

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
Compare two byte* arrays.
@return 0 if p1 == p2, < 0 if p1 <  p2, > 0 if p1 >  p2 */
static
int
fts_query_strcmp(
/*=============*/
	const void*	p1,		/*!< in: pointer to elem */
	const void*	p2)		/*!< in: pointer to elem */
{
	void* temp = const_cast<void*>(p2);

	return(strcmp(static_cast<const char*>(p1),
		      *(static_cast <char**>(temp))));
}

/*******************************************************************//**
Compare two fts_ranking_t instance on their rank value and doc ids in
descending order on the rank and ascending order on doc id.
@return 0 if p1 == p2, < 0 if p1 <  p2, > 0 if p1 >  p2 */
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

#ifdef FTS_UTF8_DEBUG
/*******************************************************************//**
Convert string to lowercase.
@return lower case string, callers responsibility to delete using
ut_free() */
static
byte*
fts_tolower(
/*========*/
	const byte*	src,		/*!< in: src string */
	ulint		len)		/*!< in: src string length */
{
	fts_string_t	str;
	byte*		lc_str = ut_malloc(len + 1);

	str.f_len = len;
	str.f_str = lc_str;

	memcpy(str.f_str, src, len);

	/* Make sure the last byte is NUL terminated */
	str.f_str[len] = '\0';

	fts_utf8_tolower(&str);

	return(lc_str);
}

/*******************************************************************//**
Do a case insensitive search. Doesn't check for NUL byte end marker
only relies on len. Convert str2 to lower case before comparing.
@return 0 if p1 == p2, < 0 if p1 <  p2, > 0 if p1 >  p2 */
static
int
fts_utf8_strcmp(
/*============*/
	const fts_string_t*
			str1,		/*!< in: should be lower case*/

	fts_string_t*	str2)		/*!< in: any case. We will use the length
					of this string during compare as it
					should be the min of the two strings */
{
	byte		b = str2->f_str[str2->f_len];

	ut_a(str2->f_len <= str1->f_len);

	/* We need to write a NUL byte at the end of the string because the
	string is converted to lowercase by a MySQL function which doesn't
	care about the length. */
	str2->f_str[str2->f_len] = 0;

	fts_utf8_tolower(str2);

	/* Restore the value we replaced above. */
	str2->f_str[str2->f_len] = b;

	return(memcmp(str1->f_str, str2->f_str, str2->f_len));
}
#endif

/*******************************************************************//**
Add a word if it doesn't exist, to the term freq RB tree. We store
a pointer to the word that is passed in as the argument.
@return pointer to word */
static
fts_word_freq_t*
fts_query_add_word_freq(
/*====================*/
	fts_query_t*	query,		/*!< in: query instance */
	const byte*	word)		/*!< in: term/word to add */
{
	ib_rbt_bound_t		parent;

	/* Lookup the word in our rb tree and add if it doesn't exist. */
	if (rbt_search(query->word_freqs, &parent, word) != 0) {
		fts_word_freq_t	word_freq;
		ulint		len = ut_strlen((char*) word) + 1;

		memset(&word_freq, 0, sizeof(word_freq));

		word_freq.word = static_cast<byte*>(
			mem_heap_alloc(query->heap, len));

		/* Need to copy the NUL character too. */
		memcpy(word_freq.word, word, len);

		word_freq.doc_count = 0;

		word_freq.doc_freqs = rbt_create(
			sizeof(fts_doc_freq_t), fts_freq_doc_id_cmp);

		parent.last = rbt_add_node(
			query->word_freqs, &parent, &word_freq);
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
	if (fts_bsearch(array, 0, size, doc_id) < 0
	    && rbt_search(query->doc_ids, &parent, &doc_id) != 0) {

		fts_ranking_t	ranking;

		ranking.rank = rank;
		ranking.doc_id = doc_id;
		ranking.words = rbt_create(sizeof(byte*), fts_query_strcmp);

		rbt_add_node(query->doc_ids, &parent, &ranking);
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
	if (fts_bsearch(array, 0, size, doc_id) < 0
	    && rbt_search(query->doc_ids, &parent, &doc_id) == 0) {

		fts_ranking_t*	ranking;

		ranking = rbt_value(fts_ranking_t, parent.last);
		rbt_free(ranking->words);

		ut_free(rbt_remove_node(query->doc_ids, parent.last));
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
	if (fts_bsearch(array, 0, size, doc_id) < 0
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
	fts_ranking_t*	ranking;

	/* Check if the doc id is deleted and it's in our set */
	if (fts_bsearch(array, 0, size, doc_id) < 0) {
		/* If this is the first FTS_EXIST we encountered, all of its
		value must be in intersect list */
		if (!query->multi_exist) {
			fts_ranking_t	new_ranking;

			if (rbt_search(query->doc_ids, &parent, &doc_id) == 0) {
				ranking = rbt_value(fts_ranking_t, parent.last);
				rank += (ranking->rank > 0)
					? ranking->rank : RANK_UPGRADE;
				if (rank >= 1.0F) {
					rank = 1.0F;
				}
			}

			new_ranking.rank = rank;
			new_ranking.doc_id = doc_id;
			new_ranking.words = rbt_create(
				sizeof(byte*), fts_query_strcmp);
			ranking = &new_ranking;

			if (rbt_search(query->intersection, &parent,
				       ranking) != 0) {
				rbt_add_node(query->intersection,
					     &parent, ranking);
			} else {
				rbt_free(new_ranking.words);
			}
		} else {

			if (rbt_search(query->doc_ids, &parent, &doc_id) != 0) {
				return;
			}

			ranking = rbt_value(fts_ranking_t, parent.last);

			ranking->rank = rank;

			if (ranking->words != NULL
			    && rbt_search(query->intersection, &parent,
					  ranking) != 0) {
				rbt_add_node(query->intersection, &parent,
					     ranking);

				/* Note that the intersection has taken
				ownership of the ranking data. */
				ranking->words = NULL;
			}
		}
	}
}

/*******************************************************************//**
Free the document ranking rb tree. */
static
void
fts_query_free_doc_ids(
/*===================*/
	ib_rbt_t*	doc_ids)	/*!< in: rb tree to free */
{
	const ib_rbt_node_t*	node;

	for (node = rbt_first(doc_ids); node; node = rbt_first(doc_ids)) {

		fts_ranking_t*	ranking;

		ranking = rbt_value(fts_ranking_t, node);

		if (ranking->words) {
			rbt_free(ranking->words);
			ranking->words = NULL;
		}

		ut_free(rbt_remove_node(doc_ids, node));
	}

	rbt_free(doc_ids);
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
	const byte*		word)	/*!< in: the token to add */
{
	ib_rbt_bound_t		parent;
	fts_ranking_t*		ranking = NULL;

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
		ulint	len;
		byte*	term;

		len = ut_strlen((char*) word) + 1;

		term = static_cast<byte*>(mem_heap_alloc(query->heap, len));

		/* Need to copy the NUL character too. */
		memcpy(term, (char*) word, len);

		/* The current set must have ownership of the RB tree. */
		ut_a(ranking->words != NULL);

		/* If the word doesn't exist in the words "list" we add it. */
		if (rbt_search(ranking->words, &parent, term) != 0) {
			rbt_add_node(ranking->words, &parent, &term);
		}
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
		ret = rbt_search(query->word_freqs, &parent, token->f_str);
		ut_a(ret == 0);

		word_freqs = rbt_value(fts_word_freq_t, parent.last);

		fts_query_filter_doc_ids(
			query, token->f_str, word_freqs, node,
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
						 srch_text.f_str);

				ut_a(ret == 0);

				word_freqs = rbt_value(
					fts_word_freq_t,
					freq_parent.last);

				fts_query_filter_doc_ids(
					query, srch_text.f_str,
					word_freqs, node,
					node->ilist, node->ilist_size, TRUE);
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
@return DB_SUCCESS if all went well */
static __attribute__((nonnull, warn_unused_result))
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
	fprintf(stderr, "DIFFERENCE: Searching: '%.*s'\n",
		(int) token->f_len, token->f_str);
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

			for (i = 0; nodes && i < ib_vector_size(nodes); ++i) {
				const fts_node_t*	node;

				node = static_cast<const fts_node_t*>(
					ib_vector_get_const(nodes, i));

				fts_query_check_node(query, token, node);
			}
		}

		rw_lock_x_unlock(&cache->lock);

		/* Setup the callback args for filtering and
		consolidating the ilist. */
		fetch.read_arg = query;
		fetch.read_record = fts_query_index_fetch_nodes;

		query->error = fts_index_fetch_nodes(
			trx, &graph, &query->fts_index_table, token, &fetch);

		fts_que_graph_free(graph);
	}

	/* The size can't increase. */
	ut_a(rbt_size(query->doc_ids) <= n_doc_ids);

	return(query->error);
}

/*****************************************************************//**
Intersect the token doc ids with the current set.
@return DB_SUCCESS if all went well */
static __attribute__((nonnull, warn_unused_result))
dberr_t
fts_query_intersect(
/*================*/
	fts_query_t*		query,	/*!< in: query instance */
	const fts_string_t*	token)	/*!< in: the token to search */
{
	ulint			n_doc_ids = 0;
	trx_t*			trx = query->trx;
	dict_table_t*		table = query->index->table;

	ut_a(query->oper == FTS_EXIST);

#ifdef FTS_INTERNAL_DIAG_PRINT
	fprintf(stderr, "INTERSECT: Searching: '%.*s'\n",
		(int) token->f_len, token->f_str);
#endif

	if (!query->inited) {

		ut_a(rbt_empty(query->doc_ids));

		/* Since this is the first time we need to convert this
		intersection query into a union query. Otherwise we
		will end up with an empty set. */
		query->oper = FTS_NONE;
		query->inited = TRUE;
	}

	if (query->doc_ids) {
		n_doc_ids = rbt_size(query->doc_ids);
	}

	/* If the words set is not empty or this is the first time. */

	if (!rbt_empty(query->doc_ids) || query->oper == FTS_NONE) {
		ulint			i;
		fts_fetch_t		fetch;
		const ib_vector_t*	nodes;
		const fts_index_cache_t*index_cache;
		que_t*			graph = NULL;
		fts_cache_t*		cache = table->fts->cache;

		ut_a(!query->intersection);

		/* Only if this is not the first time. */
		if (query->oper != FTS_NONE) {

			/* Create the rb tree that will hold the doc ids of
			the intersection. */
			query->intersection = rbt_create(
				sizeof(fts_ranking_t), fts_ranking_doc_id_cmp);
		}

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

			for (i = 0; nodes && i < ib_vector_size(nodes); ++i) {
				const fts_node_t*	node;

				node = static_cast<const fts_node_t*>(
					ib_vector_get_const(nodes, i));

				fts_query_check_node(query, token, node);
			}
		}

		rw_lock_x_unlock(&cache->lock);

		/* Setup the callback args for filtering and
		consolidating the ilist. */
		fetch.read_arg = query;
		fetch.read_record = fts_query_index_fetch_nodes;

		query->error = fts_index_fetch_nodes(
			trx, &graph, &query->fts_index_table, token, &fetch);

		fts_que_graph_free(graph);

		if (query->error == DB_SUCCESS) {
			if (query->oper == FTS_EXIST) {

				/* The size can't increase. */
				ut_a(rbt_size(query->doc_ids) <= n_doc_ids);
			}

			/* Make the intesection (rb tree) the current doc id
			set and free the old set. */
			if (query->intersection) {
				fts_query_free_doc_ids(query->doc_ids);
				query->doc_ids = query->intersection;
				query->intersection = NULL;
			}

			/* Reset the set operation to intersect. */
			query->oper = FTS_EXIST;
		}
	}

	if (!query->multi_exist) {
		query->multi_exist = TRUE;
	}

	return(query->error);
}

/*****************************************************************//**
Query index cache.
@return DB_SUCCESS if all went well */
static
ulint
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

		for (i = 0; nodes && i < ib_vector_size(nodes); ++i) {
			const fts_node_t*	node;

			node = static_cast<const fts_node_t*>(
				ib_vector_get_const(nodes, i));

			fts_query_check_node(query, token, node);
		}
	}

	rw_lock_x_unlock(&cache->lock);

	return(DB_SUCCESS);
}

/*****************************************************************//**
Set union.
@return DB_SUCCESS if all went well */
static __attribute__((nonnull, warn_unused_result))
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

	ut_a(query->oper == FTS_NONE || query->oper == FTS_DECR_RATING ||
	     query->oper == FTS_NEGATE || query->oper == FTS_INCR_RATING);

#ifdef FTS_INTERNAL_DIAG_PRINT
	fprintf(stderr, "UNION: Searching: '%.*s'\n",
		(int) token->f_len, token->f_str);
#endif

	query->error = DB_SUCCESS;

	if (query->doc_ids) {
		n_doc_ids = rbt_size(query->doc_ids);
	}

	if (token->f_len == 0) {
		return(query->error);
	}

	/* Single '%' would confuse parser in pars_like_rebind(). In addition,
	our wildcard search only supports prefix search */
	ut_ad(*token->f_str != '%');

	fts_query_cache(query, token);

	/* Setup the callback args for filtering and
	consolidating the ilist. */
	fetch.read_arg = query;
	fetch.read_record = fts_query_index_fetch_nodes;

	/* Read the nodes from disk. */
	query->error = fts_index_fetch_nodes(
		trx, &graph, &query->fts_index_table, token, &fetch);

	fts_que_graph_free(graph);

	if (query->error == DB_SUCCESS) {

		/* The size can't decrease. */
		ut_a(rbt_size(query->doc_ids) >= n_doc_ids);

		/* Calulate the number of doc ids that were added to
		the current doc id set. */
		if (query->doc_ids) {
			n_doc_ids = rbt_size(query->doc_ids) - n_doc_ids;
		}

		/* In case there were no matching docs then we reset the
		state, otherwise intersection will not be able to detect
		that it's being called for the first time. */
		if (!rbt_empty(query->doc_ids)) {
			query->inited = TRUE;
		}
	}

	return(query->error);
}

/*****************************************************************//**
Depending upon the current query operator process the doc id. */
static
void
fts_query_process_doc_id(
/*=====================*/
	fts_query_t*	query,		/*!< in: query instance */
	doc_id_t	doc_id,		/*!< in: doc id to process */
	fts_rank_t	rank)		/*!< in: if non-zero, it is the
					rank associated with the doc_id */
{
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
}

/*****************************************************************//**
Merge two result sets. */
static
void
fts_merge_doc_ids(
/*==============*/
	fts_query_t*	query,		/*!< in,out: query instance */
	const ib_rbt_t*	doc_ids)	/*!< in: result set to merge */
{
	const ib_rbt_node_t*	node;

	ut_a(!rbt_empty(doc_ids));
	ut_a(!query->intersection);

	/* To process FTS_EXIST operation (intersection), we need
	to create a new result set for fts_query_intersect(). */
	if (query->oper == FTS_EXIST) {

		query->intersection = rbt_create(
			sizeof(fts_ranking_t), fts_ranking_doc_id_cmp);
	}

	/* Merge the elements to the result set. */
	for (node = rbt_first(doc_ids); node; node = rbt_next(doc_ids, node)) {
		fts_ranking_t*		ranking;

		ranking = rbt_value(fts_ranking_t, node);

		fts_query_process_doc_id(
			query, ranking->doc_id, ranking->rank);
	}

	/* If it is an intersection operation, reset query->doc_ids
	to query->intersection and free the old result list. */
	if (query->oper == FTS_EXIST && query->intersection != NULL) {
		fts_query_free_doc_ids(query->doc_ids);
		query->doc_ids = query->intersection;
		query->intersection = NULL;
	}
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
		ulint			offset;

		ret = innobase_mysql_fts_get_token(
			phrase->charset, ptr, (byte*) end,
			&match, &offset);

		if (match.f_len > 0) {
			/* Get next token to match. */
			token = static_cast<const fts_string_t*>(
				ib_vector_get_const(tokens, i));

			fts_utf8_string_dup(&cmp_str, &match, heap);

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
		fts_string_t	match;
		fts_string_t	cmp_str;
		byte*		ptr = start;
		ulint		ret;
		ulint		offset;

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
		ptr = match.f_str = start + pos;

		/* Within limits ? */
		if (ptr >= end) {
			break;
		}

		ret = innobase_mysql_fts_get_token(
			phrase->charset, start + pos, (byte*) end,
			&match, &offset);

		if (match.f_len == 0) {
			break;
		}

		fts_utf8_string_dup(&cmp_str, &match, heap);

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

	exp = node->select_list;

	phrase->found = FALSE;

	while (exp) {
		dfield_t*	dfield = que_node_get_val(exp);
		void*		data = NULL;
		ulint		cur_len;

		if (dfield_is_ext(dfield)) {
			data = btr_copy_externally_stored_field(
				&cur_len, static_cast<const byte*>(data),
				phrase->zip_size,
				dfield_get_len(dfield), phrase->heap);
		} else {
			data = dfield_get_data(dfield);
			cur_len = dfield_get_len(dfield);
		}

		if (cur_len != UNIV_SQL_NULL && cur_len != 0) {
			phrase->found =
				fts_query_match_phrase(
					phrase, static_cast<byte*>(data),
					cur_len, prev_len, phrase->heap);
		}

		if (phrase->found) {
			break;
		}

		/* Document positions are calculated from the beginning
		of the first field, need to save the length for each
		searched field to adjust the doc position when search
		phrases. */
		prev_len += cur_len + 1;
		exp = que_node_get_next(exp);
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
@return DB_SUCCESS if all went well else error code */
static __attribute__((nonnull, warn_unused_result))
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

	trx->op_info = "fetching FTS index matching nodes";

	if (*graph) {
		info = (*graph)->info;
	} else {
		info = pars_info_create();
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
		ulint		selected;

		selected = fts_select_index(*word->f_str);

		query->fts_index_table.suffix = fts_get_suffix(selected);

		*graph = fts_parse_sql(
			&query->fts_index_table,
			info,
			"DECLARE FUNCTION my_func;\n"
			"DECLARE CURSOR c IS"
			" SELECT doc_count, ilist\n"
			" FROM %s\n"
			" WHERE word LIKE :word AND "
			"	first_doc_id <= :min_doc_id AND "
			"	last_doc_id >= :max_doc_id\n"
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

	for(;;) {
		error = fts_eval_sql(trx, *graph);

		if (error == DB_SUCCESS) {

			break;				/* Exit the loop. */
		} else {
			ut_print_timestamp(stderr);

			if (error == DB_LOCK_WAIT_TIMEOUT) {
				fprintf(stderr, " InnoDB: Warning: lock wait "
					"timeout reading FTS index. "
					"Retrying!\n");

				trx->error_state = DB_SUCCESS;
			} else {
				fprintf(stderr, " InnoDB: Error: %lu "
					"while reading FTS index.\n", error);

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
@return DB_SUCCESS if all went well else error code */
static __attribute__((nonnull, warn_unused_result))
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

	trx->op_info = "fetching FTS index document count";

	*total = 0;

	info = pars_info_create();

	pars_info_bind_function(info, "my_func", fts_query_sum, total);
	pars_info_bind_varchar_literal(info, "word", word->f_str, word->f_len);

	selected = fts_select_index(*word->f_str);

	query->fts_index_table.suffix = fts_get_suffix(selected);

	graph = fts_parse_sql(
		&query->fts_index_table,
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

	for(;;) {
		error = fts_eval_sql(trx, graph);

		if (error == DB_SUCCESS) {

			break;				/* Exit the loop. */
		} else {
			ut_print_timestamp(stderr);

			if (error == DB_LOCK_WAIT_TIMEOUT) {
				fprintf(stderr, " InnoDB: Warning: lock wait "
					"timeout reading FTS index. "
					"Retrying!\n");

				trx->error_state = DB_SUCCESS;
			} else {
				fprintf(stderr, " InnoDB: Error: %lu "
					"while reading FTS index.\n", error);

				break;			/* Exit the loop. */
			}
		}
	}

	fts_que_graph_free(graph);

	return(error);
}

/********************************************************************
Get the total number of words in a documents.
@return DB_SUCCESS if all went well else error code */
static __attribute__((nonnull, warn_unused_result))
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

	trx->op_info = "fetching FTS document term count";

	*total = 0;

	info = pars_info_create();

	pars_info_bind_function(info, "my_func", fts_query_sum, total);

	/* Convert to "storage" byte order. */
	fts_write_doc_id((byte*) &read_doc_id, doc_id);
	fts_bind_doc_id(info, "doc_id", &read_doc_id);

	query->fts_index_table.suffix = "DOC_ID";

	graph = fts_parse_sql(
		&query->fts_index_table,
		info,
		"DECLARE FUNCTION my_func;\n"
		"DECLARE CURSOR c IS"
		" SELECT count\n"
		" FROM %s\n"
		" WHERE doc_id = :doc_id "
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

	for(;;) {
		error = fts_eval_sql(trx, graph);

		if (error == DB_SUCCESS) {

			break;				/* Exit the loop. */
		} else {
			ut_print_timestamp(stderr);

			if (error == DB_LOCK_WAIT_TIMEOUT) {
				fprintf(stderr, " InnoDB: Warning: lock wait "
					"timeout reading FTS doc id table. "
					"Retrying!\n");

				trx->error_state = DB_SUCCESS;
			} else {
				fprintf(stderr, " InnoDB: Error: %lu "
					"while reading FTS doc id table.\n",
					error);

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
static __attribute__((nonnull, warn_unused_result))
dberr_t
fts_query_match_document(
/*=====================*/
	ib_vector_t*	tokens,		/*!< in: phrase tokens */
	fts_get_doc_t*	get_doc,	/*!< in: table and prepared statements */
	fts_match_t*	match,		/*!< in: doc id and positions */
	ulint		distance,	/*!< in: proximity distance */
	ibool*		found)		/*!< out: TRUE if phrase found */
{
	dberr_t		error;
	fts_phrase_t	phrase;

	memset(&phrase, 0x0, sizeof(phrase));

	phrase.match = match;		/* Positions to match */
	phrase.tokens = tokens;		/* Tokens to match */
	phrase.distance = distance;
	phrase.charset = get_doc->index_cache->charset;
	phrase.zip_size = dict_table_zip_size(
		get_doc->index_cache->index->table);
	phrase.heap = mem_heap_create(512);

	*found = phrase.found = FALSE;

	error = fts_doc_fetch_by_doc_id(
		get_doc, match->doc_id, NULL, FTS_FETCH_DOC_BY_ID_EQUAL,
		fts_query_fetch_document, &phrase);

	if (error != DB_SUCCESS) {
		ut_print_timestamp(stderr);
		fprintf(stderr, "InnoDB: Error: (%s) matching document.\n",
			ut_strerr(error));
	} else {
		*found = phrase.found;
	}

	mem_heap_free(phrase.heap);

	return(error);
}

/*****************************************************************//**
Iterate over the matched document ids and search the for the
actual phrase in the text.
@return DB_SUCCESS if all OK */
static __attribute__((nonnull, warn_unused_result))
dberr_t
fts_query_search_phrase(
/*====================*/
	fts_query_t*		query,	/*!< in: query instance */
	ib_vector_t*		tokens)	/*!< in: tokens to search */
{
	ulint			i;
	fts_get_doc_t		get_doc;
	ulint			n_matched;
	// FIXME: Debug code
	ulint			searched = 0;
	fts_cache_t*		cache = query->index->table->fts->cache;

	n_matched = ib_vector_size(query->matched);

	/* Setup the doc retrieval infrastructure. */
	memset(&get_doc, 0x0, sizeof(get_doc));

	rw_lock_x_lock(&cache->lock);

	// FIXME: We shouldn't have to cast here.
	get_doc.index_cache = (fts_index_cache_t*)
	fts_find_index_cache(cache, query->index);

	/* Must find the index cache */
	ut_a(get_doc.index_cache != NULL);

	rw_lock_x_unlock(&cache->lock);

#ifdef FTS_INTERNAL_DIAG_PRINT
	ut_print_timestamp(stderr);
	fprintf(stderr, " Start phrase search\n");
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

			// FIXME: Debug code
			++searched;

			query->error = fts_query_match_document(
				tokens, &get_doc,
				match, query->distance, &found);

			if (query->error == DB_SUCCESS && found) {
				ulint	z;

				fts_query_process_doc_id(query,
							 match->doc_id, 0);
				for (z = 0; z < ib_vector_size(tokens); z++) {
					fts_string_t*   token;
					token = static_cast<fts_string_t*>(
						ib_vector_get(tokens, z));
					fts_query_add_word_to_document(
						query, match->doc_id,
						token->f_str);
				}
			}
		}
	}

	/* Free the prepared statement. */
	if (get_doc.get_document_graph) {
		fts_que_graph_free(get_doc.get_document_graph);
		get_doc.get_document_graph = NULL;
	}

	// FIXME: Debug code
	ut_print_timestamp(stderr);
	printf(" End: %lu, %lu\n", searched, ib_vector_size(query->matched));

	return(query->error);
}

/*****************************************************************//**
Text/Phrase search.
@return DB_SUCCESS or error code */
static __attribute__((nonnull, warn_unused_result))
dberr_t
fts_query_phrase_search(
/*====================*/
	fts_query_t*		query,	/*!< in: query instance */
	const fts_string_t*	phrase)	/*!< in: token to search */
{
	char*			src;
	char*			state;	/* strtok_r internal state */
	ib_vector_t*		tokens;
	mem_heap_t*		heap = mem_heap_create(sizeof(fts_string_t));
	char*			utf8 = strdup((char*) phrase->f_str);
	ib_alloc_t*		heap_alloc;
	ulint			num_token;

	heap_alloc = ib_heap_allocator_create(heap);

	tokens = ib_vector_create(heap_alloc, sizeof(fts_string_t), 4);

	if (query->distance != ULINT_UNDEFINED && query->distance > 0) {
		query->flags = FTS_PROXIMITY;
	} else {
		query->flags = FTS_PHRASE;
	}

	/* Split the phrase into tokens. */
	for (src = utf8; /* No op */; src = NULL) {
		fts_string_t*	token = static_cast<fts_string_t*>(
			ib_vector_push(tokens, NULL));

		token->f_str = (byte*) strtok_r(
			src, FTS_PHRASE_DELIMITER, &state);

		if (token->f_str) {
			/* Add the word to the RB tree so that we can
			calculate it's frequencey within a document. */
			fts_query_add_word_freq(query, token->f_str);

			token->f_len = ut_strlen((char*) token->f_str);
		} else {
			ib_vector_pop(tokens);
			break;
		}
	}

	num_token = ib_vector_size(tokens);

	/* Ignore empty strings. */
	if (num_token > 0) {
		fts_string_t*	token;
		fts_fetch_t	fetch;
		trx_t*		trx = query->trx;
		fts_ast_oper_t	oper = query->oper;
		que_t*		graph = NULL;
		ulint		i;

		/* Create the rb tree for storing the words read form disk. */
		if (!query->inited) {

			/* Since this is the first time, we need to convert
			this intersection query into a union query. Otherwise
			we will end up with an empty set. */
			if (query->oper == FTS_EXIST) {
				query->oper = FTS_NONE;
			}

			query->inited = TRUE;
		}

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
				ut_a(num_token < MAX_PROXIMITY_ITEM);
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

			fts_index_fetch_nodes(
				trx, &graph, &query->fts_index_table,
				token, &fetch);

			fts_que_graph_free(graph);
			graph = NULL;

			fts_query_cache(query, token);

			if (!(query->flags & FTS_PHRASE)
			    && !(query->flags & FTS_PROXIMITY)) {
				break;
			}

			/* If any of the token can't be found,
			no need to continue match */
			if (ib_vector_is_empty(query->match_array[i])) {
				goto func_exit;
			}
		}

		if (num_token == 1
		    && !ib_vector_is_empty(query->match_array[0])) {
			fts_match_t*    match;
			ulint		n_matched;

			n_matched = ib_vector_size(query->match_array[0]);

			for (i = 0; i < n_matched; i++) {
				match = static_cast<fts_match_t*>(
					ib_vector_get(
						query->match_array[0], i));

				fts_query_process_doc_id(
					query, match->doc_id, 0);

				fts_query_add_word_to_document(
					query, match->doc_id, token->f_str);
			}
			query->oper = oper;
			goto func_exit;
		}

		/* If we are doing proximity search, verify the distance
		between all words, and check they are in specified distance. */
		if (query->flags & FTS_PROXIMITY) {
			fts_check_phrase_proximity(query, tokens);
		} else {
			ibool	matched;

			/* Phrase Search case:
			We filter out the doc ids that don't contain
			all the tokens in the phrase. It's cheaper to
			search the ilist than bringing the documents in
			and then doing a search through the text. Isolated
			testing shows this also helps in mitigating disruption
			of the buffer cache. */
			matched = fts_check_phrase_proximity(query, tokens);
			query->matched = query->match_array[0];

			/* Read the actual text in and search for the phrase. */
			if (matched) {
				query->error = DB_SUCCESS;
				query->error = fts_query_search_phrase(
					query, tokens);
			}
		}

		/* Restore original operation. */
		query->oper = oper;
	}

func_exit:
	free(utf8);
	mem_heap_free(heap);

	/* Don't need it anymore. */
	query->matched = NULL;

	return(query->error);
}

/*****************************************************************//**
Find the word and evaluate.
@return DB_SUCCESS if all went well */
static __attribute__((nonnull, warn_unused_result))
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
free the byte* pointer. It's allocated using ut_malloc().
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

	str_len = ut_strlen((char*) node->term.ptr);

	ut_a(node->type == FTS_AST_TERM);

	token->f_len = str_len;
	token->f_str = node->term.ptr;

	if (node->term.wildcard) {

		token->f_str = static_cast<byte*>(ut_malloc(str_len + 2));
		token->f_len = str_len + 1;

		/* Need to copy the NUL character too. */
		memcpy(token->f_str, node->term.ptr, str_len + 1);

		token->f_str[str_len] = '%';
		token->f_str[token->f_len] = 0;

		new_ptr = token->f_str;
	}

	return(new_ptr);
}

/*****************************************************************//**
Visit every node of the AST. */
static
ulint
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

	token.f_n_char = 0;

	query->oper = oper;

	query->cur_node = node;

	switch (node->type) {
	case FTS_AST_TEXT:
		token.f_str = node->text.ptr;
		token.f_len = ut_strlen((char*) token.f_str);

		/* "first second third" is treated as first & second
		& third. Create the rb tree that will hold the doc ids
		of the intersection. */
		if (!query->intersection && query->oper == FTS_EXIST) {

			query->intersection = rbt_create(
				sizeof(fts_ranking_t), fts_ranking_doc_id_cmp);
		}

		/* Set the current proximity distance. */
		query->distance = node->text.distance;

		/* Force collection of doc ids and the positions. */
		query->collect_positions = TRUE;

		query->error = fts_query_phrase_search(query, &token);

		query->collect_positions = FALSE;

		/* Make the intesection (rb tree) the current doc id
		set and free the old set. */
		if (query->intersection) {
			fts_query_free_doc_ids(query->doc_ids);
			query->doc_ids = query->intersection;
			query->intersection = NULL;
		}

		break;

	case FTS_AST_TERM:

		/* Add the word to our RB tree that will be used to
		calculate this terms per document frequency. */
		fts_query_add_word_freq(query, node->term.ptr);

		ptr = fts_query_get_token(node, &token);
		query->error = fts_query_execute(query, &token);

		if (ptr) {
			ut_free(ptr);
		}
		break;

	default:
		ut_error;
	}

	return(query->error);
}

/*****************************************************************//**
Process (nested) sub-expression, create a new result set to store the
sub-expression result by processing nodes under current sub-expression
list. Merge the sub-expression result with that of parent expression list.
@return DB_SUCCESS if all went well */
UNIV_INTERN
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
	ibool			inited = query->inited;
	bool			will_be_ignored = false;

	ut_a(node->type == FTS_AST_SUBEXP_LIST);

	node = node->list.head;

	if (!node || !node->next) {
		return(error);
	}

	cur_oper = node->oper;

	/* Save current result set */
	parent_doc_ids = query->doc_ids;

	/* Create new result set to store the sub-expression result. We
	will merge this result set with the parent after processing. */
	query->doc_ids = rbt_create(sizeof(fts_ranking_t),
				    fts_ranking_doc_id_cmp);

	/* Reset the query start flag because the sub-expression result
	set is independent of any previous results. The state flag
	reset is needed for not making an intersect operation on an empty
	set in the first call to fts_query_intersect() for the first term. */
	query->inited = FALSE;

	/* Process nodes in current sub-expression and store its
	result set in query->doc_ids we created above. */
	error = fts_ast_visit(FTS_NONE, node->next, visitor,
			      arg, &will_be_ignored);

	/* Reinstate parent node state and prepare for merge. */
	query->inited = inited;
	query->oper = cur_oper;
	subexpr_doc_ids = query->doc_ids;

	/* Restore current result set. */
	query->doc_ids = parent_doc_ids;

	if (query->oper == FTS_EXIST && !query->inited) {
		ut_a(rbt_empty(query->doc_ids));
		/* Since this is the first time we need to convert this
		intersection query into a union query. Otherwise we
		will end up with an empty set. */
		query->oper = FTS_NONE;
		query->inited = TRUE;
	}

	/* Merge the sub-expression result with the parent result set. */
	if (error == DB_SUCCESS && !rbt_empty(subexpr_doc_ids)) {
		fts_merge_doc_ids(query, subexpr_doc_ids);
	}

	if (query->oper == FTS_EXIST) {
		query->multi_exist = TRUE;
	}

	/* Free current result set. Result already merged into parent. */
	fts_query_free_doc_ids(subexpr_doc_ids);

	return(error);
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
@return fts_node_t instance */
static
void
fts_query_filter_doc_ids(
/*=====================*/
	fts_query_t*	query,		/*!< in: query instance */
	const byte*	word,		/*!< in: the current word */
	fts_word_freq_t*word_freq,	/*!< in/out: word frequency */
	const fts_node_t*
			node,		/*!< in: current FTS node */
	void*		data,		/*!< in: doc id ilist */
	ulint		len,		/*!< in: doc id ilist size */
	ibool		calc_doc_count)	/*!< in: whether to remember doc count */
{
	byte*		ptr = static_cast<byte*>(data);
	doc_id_t	doc_id = 0;
	ulint		decoded = 0;
	ib_rbt_t*	doc_freqs = word_freq->doc_freqs;

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
		doc_freq = fts_query_add_doc_freq(doc_freqs, doc_id);

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
			fts_query_process_doc_id(query, doc_id, 0);
		}

		/* Add the word to the document's matched RB tree. */
		fts_query_add_word_to_document(query, doc_id, word);
	}

	/* Some sanity checks. */
	ut_a(doc_id == node->last_doc_id);
}

/*****************************************************************//**
Read the FTS INDEX row. */
static
void
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
	byte			term[FTS_MAX_WORD_LEN + 1];

	ut_a(query->cur_node->type == FTS_AST_TERM ||
	     query->cur_node->type == FTS_AST_TEXT);

	memset(&node, 0, sizeof(node));

	/* Need to consider the wildcard search case, the word frequency
	is created on the search string not the actual word. So we need
	to assign the frequency on search string behalf. */
	if (query->cur_node->type == FTS_AST_TERM
	    && query->cur_node->term.wildcard) {

		/* These cast are safe since we only care about the
		terminating NUL character as an end of string marker. */
		ut_strcpy((char*) term, (char*) query->cur_node->term.ptr);
	} else {
		/* Need to copy the NUL character too. */
		memcpy(term, word->f_str, word->f_len);
		term[word->f_len] = 0;
	}

	/* Lookup the word in our rb tree, it must exist. */
	ret = rbt_search(query->word_freqs, &parent, term);

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

			fts_query_filter_doc_ids(
				query, word_freq->word, word_freq,
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

	fts_query_read_node(query, &key, que_node_get_next(exp));

	return(TRUE);
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
			fprintf(stderr,"'%s' -> " UINT64PF "/" UINT64PF
				" %6.5lf\n",
			        word_freq->word,
			        query->total_docs, word_freq->doc_count,
			        word_freq->idf);
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
	const ib_rbt_node_t*	node;

	/* At this stage, ranking->rank should not exceed the 1.0
	bound */
	ut_ad(ranking->rank <= 1.0 && ranking->rank >= -1.0);

	for (node = rbt_first(ranking->words);
	     node;
	     node = rbt_first(ranking->words)) {

		int			ret;
		const byte*		word;
		const byte**		wordp;
		ib_rbt_bound_t		parent;
		double			weight;
		fts_doc_freq_t*		doc_freq;
		fts_word_freq_t*	word_freq;

		wordp = rbt_value(const byte*, node);
		word = *wordp;

		ret = rbt_search(query->word_freqs, &parent, word);

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

		ut_free(rbt_remove_node(ranking->words, node));
	}
}

/*****************************************************************//**
Add ranking to the result set. */
static
void
fts_query_add_ranking(
/*==================*/
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

	if (!result || !result->rankings_by_id) {
		return(0);
	}

	new_ranking.doc_id = doc_id;

	/* Lookup the ranking in our rb tree */
	if (rbt_search(result->rankings_by_id, &parent, &new_ranking) == 0) {
		fts_ranking_t*  ranking;

		ranking = rbt_value(fts_ranking_t, parent.last);

		return(ranking->rank);
	}

	return(0);
}

/*****************************************************************//**
Create the result and copy the data to it. */
static
fts_result_t*
fts_query_prepare_result(
/*=====================*/
	const fts_query_t*	query,	/*!< in: Query state */
	fts_result_t*		result)	/*!< in: result this can contain
					data from a previous search on
					another FTS index */
{
	const ib_rbt_node_t*	node;

	ut_a(rbt_size(query->doc_ids) > 0);

	if (result == NULL) {
		result = static_cast<fts_result_t*>(ut_malloc(sizeof(*result)));

		memset(result, 0x0, sizeof(*result));

		result->rankings_by_id = rbt_create(
			sizeof(fts_ranking_t), fts_ranking_doc_id_cmp);
	}

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
		ut_a(rbt_empty(ranking->words));
		rbt_free(ranking->words);
		ranking->words = NULL;

		fts_query_add_ranking(result->rankings_by_id, ranking);
	}

	return(result);
}

/*****************************************************************//**
Get the result of the query. Calculate the similarity coefficient. */
static
fts_result_t*
fts_query_get_result(
/*=================*/
	const fts_query_t*	query,	/*!< in: query instance */
	fts_result_t*		result)	/*!< in: result */
{
	if (rbt_size(query->doc_ids) > 0) {
		/* Copy the doc ids to the result. */
		result = fts_query_prepare_result(query, result);
	} else {
		/* Create an empty result instance. */
		result = static_cast<fts_result_t*>(ut_malloc(sizeof(*result)));
		memset(result, 0, sizeof(*result));
	}

	return(result);
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

	if (query->doc_ids) {
		fts_query_free_doc_ids(query->doc_ids);
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

	ut_a(!query->intersection);

	if (query->heap) {
		mem_heap_free(query->heap);
	}

	memset(query, 0, sizeof(*query));
}

/*****************************************************************//**
Parse the query using flex/bison. */
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
	ibool		mode = query->boolean_mode;

	memset(&state, 0x0, sizeof(state));

	/* Setup the scanner to use, this depends on the mode flag. */
	state.lexer = fts_lexer_create(mode, query_str, query_len);
	error = fts_parse(&state);
	fts_lexer_free(state.lexer);
	state.lexer = NULL;

	/* Error during parsing ? */
	if (error) {
		/* Free the nodes that were allocated during parsing. */
		fts_ast_state_free(&state);
	} else {
		query->root = state.root;
	}

	return(state.root);
}


/*******************************************************************//**
FTS Query entry point.
@return DB_SUCCESS if successful otherwise error code */
UNIV_INTERN
dberr_t
fts_query(
/*======*/
	trx_t*		trx,		/*!< in: transaction */
	dict_index_t*	index,		/*!< in: The FTS index to search */
	uint		flags,		/*!< in: FTS search mode */
	const byte*	query_str,	/*!< in: FTS query */
	ulint		query_len,	/*!< in: FTS query string len
					in bytes */
	fts_result_t**	result)		/*!< in/out: result doc ids */
{
	fts_query_t	query;
	dberr_t		error = DB_SUCCESS;
	byte*		lc_query_str;
	ulint		lc_query_str_len;
	ulint		result_len;
	ibool		boolean_mode;
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
	query.inited = FALSE;
	query.boolean_mode = boolean_mode;
	query.deleted = fts_doc_ids_create();
	query.cur_node = NULL;

	query.fts_common_table.type = FTS_COMMON_TABLE;
	query.fts_common_table.table_id = index->table->id;
	query.fts_common_table.parent = index->table->name;

	charset = fts_index_get_charset(index);

	query.fts_index_table.type = FTS_INDEX_TABLE;
	query.fts_index_table.index_id = index->id;
	query.fts_index_table.table_id = index->table->id;
	query.fts_index_table.parent = index->table->name;
	query.fts_index_table.charset = charset;


	/* Setup the RB tree that will be used to collect per term
	statistics. */
	query.word_freqs = rbt_create_arg_cmp(
		sizeof(fts_word_freq_t), innobase_fts_string_cmp, charset);

	query.total_docs = dict_table_get_n_rows(index->table);

#ifdef FTS_DOC_STATS_DEBUG
	if (ft_enable_diag_print) {
		error = fts_get_total_word_count(
			trx, query.index, &query.total_words);

		if (error != DB_SUCCESS) {
			goto func_exit;
		}

		fprintf(stderr, "Total docs: " UINT64PF " Total words: %lu\n",
			query.total_docs, query.total_words);
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

	/* Sort the vector so that we can do a binary search over the ids. */
	ib_vector_sort(query.deleted->doc_ids, fts_update_doc_id_cmp);

	/* Convert the query string to lower case before parsing. We own
	the ut_malloc'ed result and so remember to free it before return. */

	lc_query_str_len = query_len * charset->casedn_multiply + 1;
	lc_query_str = static_cast<byte*>(ut_malloc(lc_query_str_len));

	result_len = innobase_fts_casedn_str(
		charset, (char*) query_str, query_len,
		(char*) lc_query_str, lc_query_str_len);

	ut_ad(result_len < lc_query_str_len);

	lc_query_str[result_len] = 0;

	query.heap = mem_heap_create(128);

	/* Create the rb tree for the doc id (current) set. */
	query.doc_ids = rbt_create(
		sizeof(fts_ranking_t), fts_ranking_doc_id_cmp);

	/* Parse the input query string. */
	if (fts_query_parse(&query, lc_query_str, result_len)) {
		fts_ast_node_t*	ast = query.root;

		/* Traverse the Abstract Syntax Tree (AST) and execute
		the query. */
		query.error = fts_ast_visit(
			FTS_NONE, ast, fts_query_visitor,
			&query, &will_be_ignored);

		/* If query expansion is requested, extend the search
		with first search pass result */
		if (query.error == DB_SUCCESS && (flags & FTS_EXPAND)) {
			 query.error = fts_expand_query(index, &query);
		}

		/* Calculate the inverse document frequency of the terms. */
		fts_query_calculate_idf(&query);

		/* Copy the result from the query state, so that we can
		return it to the caller. */
		if (query.error == DB_SUCCESS) {
			*result = fts_query_get_result(&query, *result);
		}

		error = query.error;
	} else {
		/* still return an empty result set */
		*result = static_cast<fts_result_t*>(
			ut_malloc(sizeof(**result)));
		memset(*result, 0, sizeof(**result));
	}

	ut_free(lc_query_str);

	if (fts_enable_diag_print && (*result)) {
		ulint	diff_time = ut_time_ms() - start_time_ms;
		fprintf(stderr, "FTS Search Processing time: %ld secs:"
				" %ld millisec: row(s) %d \n",
			diff_time / 1000, diff_time % 1000,
			(*result)->rankings_by_id
				? (int) rbt_size((*result)->rankings_by_id)
				: -1);
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

#ifdef UNIV_DEBUG
/*******************************************************************//**
A debug function to print result doc_id set. */
static
void
fts_print_doc_id(
/*=============*/
	ib_rbt_t*	doc_ids)	/*!< in : tree that stores doc_ids.*/
{
	const ib_rbt_node_t*	node;
	const ib_rbt_node_t*	node_word;

	/* Iterate each member of the doc_id set */
	for (node = rbt_first(doc_ids);
	     node;
	     node = rbt_next(doc_ids, node)) {
		fts_ranking_t*	ranking;
		ranking = rbt_value(fts_ranking_t, node);

		fprintf(stderr, "doc_ids info, doc_id: %ld \n",
			(ulint) ranking->doc_id);

		for (node_word = rbt_first(ranking->words);
		     node_word;
		     node_word = rbt_next(ranking->words, node_word)) {

			const byte** value;

			value = rbt_value(const byte*, node_word);

			fprintf(stderr, "doc_ids info, value: %s \n", *value);
		}
	}
}
#endif

/*************************************************************//**
This function implements a simple "blind" query expansion search:
words in documents found in the first search pass will be used as
search arguments to search the document again, thus "expand"
the search result set.
@return DB_SUCCESS if success, otherwise the error code */
static __attribute__((nonnull, warn_unused_result))
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

#ifdef UNIV_DEBUG
	fts_print_doc_id(query->doc_ids);
#endif

	for (node = rbt_first(query->doc_ids);
	     node;
	     node = rbt_next(query->doc_ids, node)) {

		fts_ranking_t*	ranking;
		const ib_rbt_node_t*	node_word;

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

		/* Remove words that have already been searched in the
		first pass */
		for (node_word = rbt_first(ranking->words);
		     node_word;
		     node_word = rbt_next(ranking->words, node_word)) {
			fts_string_t	str;
			ibool		ret;
			const byte**	strp;

			strp = rbt_value(const byte*, node_word);
			/* FIXME: We are discarding a const qualifier here. */
			str.f_str = (byte*) *strp;
			str.f_len = ut_strlen((const char*) str.f_str);
			ret = rbt_delete(result_doc.tokens, &str);

			/* The word must exist in the doc we found */
			if (!ret) {
				fprintf(stderr, " InnoDB: Error: Did not "
					"find word %s in doc %ld for query "
					"expansion search.\n", str.f_str,
					(ulint) ranking->doc_id);
			}
		}
	}

	/* Search the table the second time with expanded search list */
	for (token_node = rbt_first(result_doc.tokens);
	     token_node;
	     token_node = rbt_next(result_doc.tokens, token_node)) {
		fts_token_t*	mytoken;
		mytoken = rbt_value(fts_token_t, token_node);

		fts_query_add_word_freq(query, mytoken->text.f_str);
		error = fts_query_union(query, &mytoken->text);

		if (error != DB_SUCCESS) {
			break;
		}
	}

	fts_doc_free(&result_doc);

	return(error);
}
/*************************************************************//**
This function finds documents that contain all words in a
phrase or proximity search. And if proximity search, verify
the words are close to each other enough, as in specified distance.
This function is called for phrase and proximity search.
@return TRUE if documents are found, FALSE if otherwise */
static
ibool
fts_check_phrase_proximity(
/*=======================*/
	fts_query_t*	query,		/*!< in:  query instance */
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
		ulint	j;
		ulint	k = 0;

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
		to each other, and with in distance specified
		in the proximity search */
		if (query->flags & FTS_PHRASE) {
			matched = TRUE;
		} else if (fts_proximity_check_position(
			match, num_token, query->distance)) {
			ulint	z;
			/* If so, mark we find a matching doc */
			fts_query_process_doc_id(query, match[0]->doc_id, 0);

			matched = TRUE;
			for (z = 0; z < num_token; z++) {
				fts_string_t*	token;
				token = static_cast<fts_string_t*>(
					ib_vector_get(tokens, z));
				fts_query_add_word_to_document(
					query, match[0]->doc_id,
					token->f_str);
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
This function check the words in result document are close to each
other (within proximity range). This is used for proximity search.
@return TRUE if words are close to each other, FALSE if otherwise */
static
ulint
fts_proximity_check_position(
/*=========================*/
	fts_match_t**	match,		/*!< in: query instance */
	ulint		num_match,	/*!< in: number of matching
					items */
	ulint		distance)	/*!< in: distance value
					for proximity search */
{
	ulint	i;
	ulint	idx[MAX_PROXIMITY_ITEM];
	ulint	num_pos[MAX_PROXIMITY_ITEM];
	ulint	min_idx;

	ut_a(num_match < MAX_PROXIMITY_ITEM);

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
			return(TRUE);
		} else {
			/* Otherwise, move to the next position is the
			list for the word with the smallest position */
			idx[min_idx]++;
		}
	}

	/* Failed to find all words within the range for the doc */
	return(FALSE);
}
