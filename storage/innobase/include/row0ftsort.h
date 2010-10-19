/*****************************************************************************

Copyright (c) 2010, Oracle and/or its affiliates. All Rights Reserved.

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
@file include/row0ftsort.h
reate Full Text Index with (parallel) merge sort

Created 10/13/2010 Jimmy Yang
*******************************************************/

#ifndef row0ftsort_h
#define row0ftsort_h

#include "univ.i"
#include "data0data.h"
#include "dict0types.h"
#include "row0mysql.h"
#include "fts0fts.h"
#include "fts0types.h"
#include "fts0priv.h"
#include "row0merge.h"

/** This structure defineds information the scan thread will fetch
and put to the linked list for parallel tokenization/sort threads
to process */
typedef struct fts_doc_item     fts_doc_item_t;

/** Information about temporary files used in merge sort */
struct fts_doc_item {
	const dfield_t*	field;		/*!< field contains document string */
	doc_id_t	doc_id;		/*!< document ID */
	UT_LIST_NODE_T(fts_doc_item_t)  doc_list;
					/*!< list of doc items */
};

/** This defines the list type that scan thread would feed the parallel
tokenization threads and sort threads. */
typedef UT_LIST_BASE_NODE_T(fts_doc_item_t)     fts_doc_list_t;

#define FTS_PARALLEL_DEGREE	2
#define FTS_PLL_ENABLED		1
#define FTS_NUM_AUX_INDEX	4
#define FTS_PLL_MERGE		1


/** Sort information passed to each individual parallel sort thread */
typedef struct fts_psort_info_struct	fts_psort_info_t;

/** Common info passed to each parallel sort thread */
struct fts_psort_common_struct {
	struct TABLE*		table;		/*!< MySQL table */
	dict_table_t*		new_table;	/*!< source table */
	trx_t*			trx;		/*!< transaction */
	dict_index_t*		sort_index;	/*!< FTS index */
	fts_psort_info_t*	all_info;	/*!< all parallel sort info */
	os_event_t		sort_event;	/*!< sort event */
};

typedef struct fts_psort_common_struct	fts_psort_common_t;

struct fts_psort_info_struct {
        ulint                   psort_id;	/*!< Parallel sort ID */
        row_merge_buf_t*        merge_buf[FTS_NUM_AUX_INDEX];
						/*!< sort buffer */
        merge_file_t*           merge_file[FTS_NUM_AUX_INDEX];
						/*!< sort file */
        row_merge_block_t*      merge_block[FTS_NUM_AUX_INDEX];
						/*!< buffer to write to file */
        ulint                   child_status;	/*!< child thread status */
        ulint                   state;		/*!< child thread state */
	fts_doc_list_t		fts_doc_list;	/*!< doc list to process */
	fts_psort_common_t*	psort_common;	/*!< ptr to all psort info */
};


/** status bit used for communication between parent and child thread */
#define FTS_PARENT_COMPLETE	1
#define FTS_CHILD_COMPLETE	1

/** Print some debug information */
#define	FTSORT_PRINT

#ifdef	FTSORT_PRINT
#define	DEBUG_FTS_SORT_PRINT(str)		\
	do {					\
		ut_print_timestamp(stderr);	\
		fprintf(stderr, str);		\
	} while (0)
#else
#define DEBUG_FTS_SORT_PRINT(str)
#endif	/* FTSORT_PRINT */

/*************************************************************//**
Create a temporary "fts sort index" used to merge sort the
tokenized doc string. The index has three "fields":

1) Tokenized word,
2) Doc ID
3) Word's position in original 'doc'.

@return dict_index_t structure for the fts sort index */
UNIV_INTERN
dict_index_t*
row_merge_create_fts_sort_index(
/*============================*/
	dict_index_t*		index,	/*!< in: Original FTS index
					based on which this sort index
					is created */
	const dict_table_t*	table);	/*!< in: table that FTS index
					is being created on */
/********************************************************************//**
Initialize FTS parallel sort structures.
@return TRUE if all successful */
UNIV_INTERN
ibool
row_fts_psort_info_init(
/*====================*/
	trx_t*			trx,	/*!< in: transaction */
	struct TABLE*		table,	/*!< in: MySQL table object */
	const dict_table_t*	new_table,/*!< in: table where indexes are
					created */
	dict_index_t*		index,	/*!< in: FTS index to be created */
	fts_psort_info_t**	psort,	/*!< out: parallel sort info to be
					instantiated */
	fts_psort_info_t**	merge);	/*!< out: parallel merge info
					to be instantiated */
/********************************************************************//**
Clean up and deallocate FTS parallel sort structures, and close
temparary merge sort files */
UNIV_INTERN
void
row_fts_psort_info_destroy(
/*=======================*/
        fts_psort_info_t*	psort_info,	/*!< parallel sort info */
	fts_psort_info_t*	merge_info);	/*!< parallel merge info */
/********************************************************************//**
Free up merge buffers when merge sort is done */
UNIV_INTERN
void
row_fts_free_pll_merge_buf(
/*=======================*/
        fts_psort_info_t*	psort_info);	/*!< in: parallel sort info */
/******************************************************//**
Tokenize incoming text data and add to the sort buffer.
FIXME: Consider run out of buffer in the middle of string parsing.
@return number of rows added, 0 if out of space */
UNIV_INTERN
ulint
row_merge_fts_doc_tokenize(
/*=======================*/
	row_merge_buf_t**	sort_buf,	/*!< in/out: sort buffer */
	const dfield_t*		dfield,		/*!< in: Field contain doc
						to be parsed */
	doc_id_t		doc_id,		/*!< in: document ID */
	ulint*			init_pos,       /*!< in/out: doc start
						position */
	ulint*			buf_used,       /*!< in/out: sort buffer used */
	ulint*			rows_added,	/*!< in/out: num rows added */
	merge_file_t**		merge_file);	/*!< in/out: merge file to
						fill */
/********************************************************************//**
Read sorted file containing index data tuples and insert these data
tuples to the index
@return DB_SUCCESS or error number */
UNIV_INTERN
ulint
row_merge_insert_fts_tuples(
/*========================*/
	trx_t*			trx,	/*!< in: transaction */
	dict_index_t*		index,	/*!< in: index */
	dict_table_t*		table,	/*!< in: new table */
	ulint			zip_size,/*!< in: compressed page size of
					the old table, or 0 if uncompressed */
	fts_psort_info_t*		psort_info,
	ulint			id);

/*********************************************************************//**
Function performs parallel tokenization of the incoming doc strings.
@return OS_THREAD_DUMMY_RETURN */
UNIV_INTERN
os_thread_ret_t
fts_parallel_tokenization(
/*======================*/
        void*		arg);		/*!< in: psort_info for the thread */
/*********************************************************************//**
Start the parallel tokenization and parallel merge sort */
UNIV_INTERN
void
row_fts_start_psort(
/*================*/
	fts_psort_info_t*	psort_info);	/*!< in: parallel sort info */
/*********************************************************************//**
Function performs the merge and insertion of the sorted records.
@return OS_THREAD_DUMMY_RETURN */
UNIV_INTERN
os_thread_ret_t
fts_parallel_merge(
/*===============*/
	void*		arg);		/*!< in: parallel merge info */
/*********************************************************************//**
Kick off the parallel merge and insert thread */
UNIV_INTERN
void
row_fts_start_parallel_merge(
/*=========================*/
        fts_psort_info_t* merge_info,	/*!< in: parallel sort info */
        fts_psort_info_t* psort_info);	/*!< in: parallel merge info */
/********************************************************************//**
Insert processed FTS data to the auxillary tables.
@return DB_SUCCESS if insertion runs fine */
UNIV_INTERN
ulint
row_merge_write_fts_word(
/*=====================*/
	trx_t*		trx,		/*!< in: transaction */
	que_t**		ins_graph,	/*!< in: Insert query graphs */
	fts_tokenizer_word_t*word,	/*!< in: sorted and tokenized
					word */
	fts_node_t*	fts_node,	/*!< in: fts node for FTS
					INDEX table */
	fts_table_t*	fts_table);	/*!< in: fts aux table instance */
/********************************************************************//**
Read sorted FTS data files and insert data tuples to auxillary tables.
@return DB_SUCCESS or error number */
UNIV_INTERN
void
row_fts_insert_tuple(
/*=================*/
	trx_t*		trx,		/*!< in: transaction */
	que_t**		ins_graph,	/*!< in: Insert query graphs */
	dict_index_t*	index,		/*!< in: fts sort index */
	fts_table_t*	fts_table,	/*!< in: fts aux table instance */
	fts_tokenizer_word_t* word,	/*!< in: last processed
					tokenized word */
	ib_vector_t*	positions,	/*!< in: word position */
	doc_id_t*	in_doc_id,	/*!< in: last item doc id */
	dtuple_t*	dtuple,		/*!< in: index entry */
	int*		count);		/*!< in/out: counter recording how many
					records have been inserted */
/********************************************************************//**
Propagate a newly added record up one level in the selection tree
@return parent where this value propagated to */
UNIV_INTERN
int
row_merge_fts_sel_propagate(
/*========================*/
	int		propogated,	/*<! in: tree node propagated */
	int*		sel_tree,	/*<! in: selection tree */
	ulint		level,		/*<! in: selection tree level */
	const mrec_t**	 mrec,		/*<! in: sort record */
	ulint**		offsets,	/*<! in: record offsets */
	dict_index_t*	index);		/*<! in: FTS index */
/********************************************************************//**
Read sorted file containing index data tuples and insert these data
tuples to the index
@return DB_SUCCESS or error number */
UNIV_INTERN
ulint
row_fts_merge_insert(
/*=================*/
	trx_t*		trx,		/*!< in: transaction */
	dict_index_t*	index,		/*!< in: index */
	dict_table_t*	table,		/*!< in: new table */
	ulint		zip_size,	/*!< in: compressed page size of
					the old table, or 0 if uncompressed */
	fts_psort_info_t* psort_info,	/*!< parallel sort info */
	ulint		id);		/* !< in: which auxiliary table's data
					to insert to */

#endif /* row0ftsort_h */
