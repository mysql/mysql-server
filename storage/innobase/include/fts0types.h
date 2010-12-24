/******************************************************
Full Text Search functionality.

(c) 2007 Innobase Oy

Created 2007-03-27 Sunny Bains
*******************************************************/

#ifndef INNOBASE_FTS0TYPES_H
#define INNOBASE_FTS0TYPES_H

#include "que0types.h"
#include "ut0byte.h"
#include "fut0fut.h"
#include "ut0rbt.h"
#include "fts0fts.h"

/* Types (aliases) used within FTS. */
typedef struct fts_que_struct fts_que_t;
typedef struct fts_node_struct fts_node_t;
typedef struct fts_word_struct fts_word_t;
typedef struct fts_fetch_struct fts_fetch_t;
typedef struct fts_update_struct fts_update_t;
typedef struct fts_get_doc_struct fts_get_doc_t;
typedef struct fts_utf8_str_struct fts_utf8_str_t;
typedef struct fts_doc_stats_struct fts_doc_stats_t;
typedef struct fts_tokenizer_word_struct fts_tokenizer_word_t;
typedef struct fts_index_selector_struct fts_index_selector_t;

/* Callbacks used within FTS. */
typedef pars_user_func_cb_t fts_sql_callback;
typedef void (*fts_filter)(void*, fts_node_t*, void*, ulint len);

/* Statistics relevant to a particular document, used during retrieval. */
struct fts_doc_stats_struct {
	doc_id_t	doc_id;		/* Document id */
	ulint		word_count;	/* Total words in the document */
};

/* It's main purpose is to store the SQL prepared statements that
are required to retrieve a document from the database. */
struct fts_get_doc_struct {
	fts_index_cache_t*
			index_cache;	/* The index cache instance */

					/* Parsed sql statement */
	que_t*		get_document_graph;
};

/* Since we can have multiple FTS indexes on a table, we keep a
per index cache of words etc. */
struct fts_index_cache_struct {
	dict_index_t*	index;		/* The FTS index instance */

	ib_rbt_t*	words;		/* Nodes; indexed by fts_string_t*,
					cells are fts_tokenizer_word_t*.*/

	ib_vector_t*	doc_stats;	/* Array of the fts_doc_stats_t
					contained in the memory buffer.
					Must be in sorted order (ascending).
					The  ideal choice is an rb tree but
					the rb tree imposes a space overhead
					that we can do without */

	que_t**		ins_graph;	/* Insert query graphs */

	que_t**		sel_graph;	/* Select query graphs */
};

/* For supporting the tracking of updates on multiple FTS indexes we need
to track which FTS indexes need to be updated. For INSERT and DELETE we
update all fts indexes. */
struct fts_update_struct {
	doc_id_t	doc_id;		/* The doc id affected */

	ib_vector_t*	fts_indexes;	/* The FTS indexes that need to be
					updated. A NULL value means all
					indexes need to be updated.  This
					vector is not allocated on the heap
					and so must be freed explicitly,
					when we are done with it */
};

struct fts_stopword_struct {
	ulint		status;		/* Status of the stopword tree */
	ib_alloc_t*	heap;		/* The memory allocator to use */
	ib_rbt_t*	cached_stopword;/* This stores all active stopwords */
};

/* The SYNC state of the cache. There is one instance of this struct
associated with each ADD thread. */
struct fts_sync_struct {
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
        ib_time_t	start_time;	/*!< SYNC start time */
};

typedef struct fts_sync_struct	fts_sync_t;

/* The cache for the FTS system. It is a memory-based inverted index
that new entries are added to, until it grows over the configured maximum
size, at which time its contents are written to the INDEX table. */
struct fts_cache_struct {
	rw_lock_t	lock;		/* lock protecting all access to the
					memory buffer. FIXME: this needs to
					be our new upgrade-capable rw-lock */

	mutex_t		optimize_lock;	/* Lock for OPTIMIZE */

	mutex_t		deleted_lock;	/* Lock covering deleted_doc_ids */

	ib_vector_t*	deleted_doc_ids;/* Array of deleted doc ids, each
					element is of type fts_update_t */

	ib_vector_t*	indexes;	/* We store the stats and inverted
					index for the individual FTS indexes
					in this vector. Each element is
					an instance of fts_index_cache_t */

	ib_vector_t*	get_docs;	/* information required to read
					the document from the table. Each
					element is of type fts_doc_t */

	ulint		total_size;	/* total size consumed by the ilist
					field of all nodes. SYNC is run
					whenever this gets too big */
	fts_sync_t*	sync;		/* sync structure to sync data to
					disk */
	ib_alloc_t*	sync_heap;	/* The heap allocator, for indexes
					and deleted_doc_ids, ie. transient
					objects, they are recreated after
					a SYNC is completed */


	ib_alloc_t*	self_heap;	/* This heap is the heap out of
					which an instance of the cache itself
					was created. Objects created using
					this heap will last for the lifetime
					of the cache */

	doc_id_t	next_doc_id;	/* Next doc id */

	doc_id_t	last_doc_id;	/* Upper limit of allocation */

	ulint		deleted;	/* Number of doc ids deleted since last
					optimized. This variable is covered
					by deleted_lock */

	ulint		added;		/* Number of doc ids added since last
					optimized. This variable is covered by
					the deleted lock */

	fts_stopword_t	stopword_info;	/* Cached stopwords for the FTS */
};

/* Columns of the FTS auxiliary INDEX table */
struct fts_node_struct {
	doc_id_t	first_doc_id;	/* First document id in ilist. */

	doc_id_t	last_doc_id;	/* Last document id in ilist. */

	byte*		ilist;		/* Binary list of documents & word
					positions the token appears in.
					TODO: For now, these are simply
					ut_malloc'd, but if testing shows
					that they waste memory unacceptably, a
					special memory allocator will have
					to be written */

	ulint		doc_count;	/* Number of doc ids in ilist */

	ulint		ilist_size;	/* Used size of ilist in bytes. */

	ulint		ilist_size_alloc;
					/* Allocated size of ilist in
					bytes */
};

/* A tokenizer word. Contains information about one word. */
struct fts_tokenizer_word_struct {
	fts_string_t	text;		/* Token text. */

	ib_vector_t*	nodes;		/* Word node ilists, each element is
					of type fts_node_t */
};

/* Word text plus it's array of nodes as on disk in FTS index */
struct fts_word_struct {
	fts_string_t	text;		/* Word value in UTF-8 */
	ib_vector_t*	nodes;		/* Nodes read from disk */

	ib_alloc_t*	heap_alloc;	/* For handling all allocations */
};

/* Callback for reading and filtering nodes that are read from FTS index */
struct fts_fetch_struct {
	void*		read_arg;	/* Arg for the sql_callback */

	fts_sql_callback
			read_record;	/* Callback for reading index record */
};

/* For horizontally splitting an FTS auxiliary index */
struct fts_index_selector_struct {
	byte		ch;		/* Character at which to split */

	const char*	suffix;		/* FTS aux index suffix */
};

/* This type represents a single document. */
struct fts_doc_struct {
	fts_string_t	text;		/* document text */

	ibool		found;		/* TRUE if the document was found
					successfully in the database */

	ib_rbt_t*	tokens;		/* This is filled when the document
					is tokenized. Tokens; indexed by
					fts_string_t*, cells are of type
					fts_token_t* */

	ib_alloc_t*	self_heap;	/* An instance of this type is
					allocated from this heap along
					with any objects that have the
					same lifespan, most notably
					the vector of token positions */
};

/* A token and its positions within a document. */
struct fts_token_struct {
	fts_string_t	text;		/* token text */

	ib_vector_t*	positions;	/* an array of the positions the
					token is found in; each item is
					actually an ulint. */
};

/* It's defined in fut0fut.c */
extern const fts_index_selector_t fts_index_selector[];

/* Max length of an FTS word in characters */
#define FTS_MAX_WORD_LEN		32

/* Maximum token length in bytes(for UTF8)*/
#define FTS_MAX_UTF8_WORD_LEN		(FTS_MAX_WORD_LEN * 4)

/********************************************************************
Compare two UTF-8 strings. */
UNIV_INLINE
int
fts_utf8_string_cmp(
/*================*/
						/* out:
						< 0 if n1 < n2,
						0 if n1 == n2,
						> 0 if n1 > n2 */
	const void*	p1,			/* in: key */
	const void*	p2);			/* in: node */

/********************************************************************
Compare two fts_trx_row_t instances doc_ids. */
UNIV_INLINE
int
fts_trx_row_doc_id_cmp(
/*===================*/
						/* out:
						< 0 if n1 < n2,
						0 if n1 == n2,
						> 0 if n1 > n2 */
	const void*	p1,			/* in: id1 */
	const void*	p2);			/* in: id2 */
/********************************************************************
Compare two fts_ranking_t instances doc_ids. */
UNIV_INLINE
int
fts_ranking_doc_id_cmp(
/*===================*/
						/* out:
						< 0 if n1 < n2,
						0 if n1 == n2,
						> 0 if n1 > n2 */
	const void*	p1,			/* in: id1 */
	const void*	p2);			/* in: id2 */
/********************************************************************
Compare two fts_update_t instances doc_ids. */
UNIV_INLINE
int
fts_update_doc_id_cmp(
/*==================*/
						/* out:
						< 0 if n1 < n2,
						0 if n1 == n2,
						> 0 if n1 > n2 */
	const void*	p1,			/* in: id1 */
	const void*	p2);			/* in: id2 */
/********************************************************************
Decode and return the integer that was encoded using our VLC scheme.*/
UNIV_INLINE
ulint
fts_decode_vlc(
/*===========*/
			/* out: value decoded */
	byte**	ptr);	/* in: ptr to decode from, this ptr is
			incremented by the number of bytes decoded */
/********************************************************************
Duplicate an UTF-8 string. */
UNIV_INLINE
void
fts_utf8_string_dup(
/*================*/
						/* out:
						< 0 if n1 < n2,
						0 if n1 == n2,
						> 0 if n1 > n2 */
	fts_string_t*		dst,		/* in: dup to here */
	const fts_string_t*	src,		/* in: src string */
	mem_heap_t*		heap);		/* in: heap to use */
/********************************************************************
Return length of val if it were encoded using our VLC scheme. */
UNIV_INLINE
ulint
fts_get_encoded_len(
/*================*/
						/* out: length of value
						 encoded, in bytes */
	ulint		val);			/* in: value to encode */
/********************************************************************
Encode an integer using our VLC scheme and return the length in bytes. */
UNIV_INLINE
ulint
fts_encode_int(
/*===========*/
						/* out: length of value
						encoded, in bytes */
	ulint		val,			/* in: value to encode */
	byte*		buf);			/* in: buffer, must have
						enough space */
/********************************************************************
Decode a UTF-8 character.

http://www.unicode.org/versions/Unicode4.0.0/ch03.pdf:

 Scalar Value              1st Byte 2nd Byte 3rd Byte 4th Byte
00000000 0xxxxxxx          0xxxxxxx
00000yyy yyxxxxxx          110yyyyy 10xxxxxx
zzzzyyyy yyxxxxxx          1110zzzz 10yyyyyy 10xxxxxx
000uuuzz zzzzyyyy yyxxxxxx 11110uuu 10zzzzzz 10yyyyyy 10xxxxxx

This function decodes UTF-8 sequences up to 6 bytes (31 bits).

On error *ptr will point to the first byte that was not correctly
decoded. This will hopefully help in resyncing the input. */
UNIV_INLINE
ulint
fts_utf8_decode(
/*============*/
						/* out: UTF8_ERROR if *ptr
						did not point to a valid
						UTF-8 sequence, or the
						Unicode code point. */
	const byte**	ptr);			/* in/out: pointer to
						UTF-8 string. The
						pointer is advanced to
						the start of the next
						character. */
/********************************************************************
Check whether a character is a digit. */
UNIV_INLINE
ibool
fts_utf8_isdigit(
/*=============*/
						/* out: TRUE if the character
						is a digit*/
	ulint		ch);			/* in: UTF-8 character */
/********************************************************************
Check whether a character is an alphabetic one. */
UNIV_INLINE
ibool
fts_utf8_isalpha(
/*=============*/
						/* out: TRUE if the character
						is an alphabetic character */
	ulint		ch);			/* in: char to test */
/********************************************************************
Lowercase an UTF-8 string. */
UNIV_INLINE
void
fts_utf8_tolower(
/*=============*/
	fts_string_t*	str);			/* in: string */
/********************************************************************
Get the selected FTS aux INDEX suffix. */
UNIV_INLINE
const char*
fts_get_suffix(
/*===========*/
	ulint		selected);		/* in: selected index */

/********************************************************************
Get the number of index selectors. */
UNIV_INLINE
ulint
fts_get_n_selectors(void);
/*=====================*/

/********************************************************************
Select the FTS auxiliary index for the given character. */
UNIV_INLINE
ulint
fts_select_index(
/*=============*/
						/* out: the index
						to use for character */
	byte		ch);			/* in: First character
						of a word */

/********************************************************************
Select the next FTS auxiliary index for the given character. */
UNIV_INLINE
ulint
fts_select_next_index(
/*==================*/
						/* out: the next index
						to use for character */
	byte		ch);			/* in: First character of
						a word */
#ifndef UNIV_NONINL
#include "fts0types.ic"
#include "fts0vlc.ic"
#endif

#endif /* INNOBASE_FTS0TYPES_H */
