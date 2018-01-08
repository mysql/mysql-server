/*****************************************************************************

Copyright (c) 2009, 2017, Oracle and/or its affiliates. All Rights Reserved.

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

#include "my_dbug.h"
#include "my_inttypes.h"

/**************************************************//**
@file dict/dict0stats.cc
Code used for calculating and manipulating table statistics.

Created Jan 06, 2010 Vasil Dimov
*******************************************************/

#include <mysql_com.h>
#include <algorithm>
#include <map>
#include <vector>

#include "dict0stats.h"
#include "dyn0buf.h"
#include "ha_prototypes.h"
#include "lob0lob.h"
#include "pars0pars.h"
#include "row0sel.h"
#include "trx0trx.h"
#include "univ.i"
#include "ut0new.h"
#include "ut0rnd.h"
#include "ut0ut.h"

/* Sampling algorithm description @{

The algorithm is controlled by one number - N_SAMPLE_PAGES(index),
let it be A, which is the number of leaf pages to analyze for a given index
for each n-prefix (if the index is on 3 columns, then 3*A leaf pages will be
analyzed).

Let the total number of leaf pages in the table be T.
Level 0 - leaf pages, level H - root.

Definition: N-prefix-boring record is a record on a non-leaf page that equals
the next (to the right, cross page boundaries, skipping the supremum and
infimum) record on the same level when looking at the fist n-prefix columns.
The last (user) record on a level is not boring (it does not match the
non-existent user record to the right). We call the records boring because all
the records on the page below a boring record are equal to that boring record.

We avoid diving below boring records when searching for a leaf page to
estimate the number of distinct records because we know that such a leaf
page will have number of distinct records == 1.

For each n-prefix: start from the root level and full scan subsequent lower
levels until a level that contains at least A*10 distinct records is found.
Lets call this level LA.
As an optimization the search is canceled if it has reached level 1 (never
descend to the level 0 (leaf)) and also if the next level to be scanned
would contain more than A pages. The latter is because the user has asked
to analyze A leaf pages and it does not make sense to scan much more than
A non-leaf pages with the sole purpose of finding a good sample of A leaf
pages.

After finding the appropriate level LA with >A*10 distinct records (or less in
the exceptions described above), divide it into groups of equal records and
pick A such groups. Then pick the last record from each group. For example,
let the level be:

index:  0,1,2,3,4,5,6,7,8,9,10
record: 1,1,1,2,2,7,7,7,7,7,9

There are 4 groups of distinct records and if A=2 random ones are selected,
e.g. 1,1,1 and 7,7,7,7,7, then records with indexes 2 and 9 will be selected.

After selecting A records as described above, dive below them to find A leaf
pages and analyze them, finding the total number of distinct records. The
dive to the leaf level is performed by selecting a non-boring record from
each page and diving below it.

This way, a total of A leaf pages are analyzed for the given n-prefix.

Let the number of different key values found in each leaf page i be Pi (i=1..A).
Let N_DIFF_AVG_LEAF be (P1 + P2 + ... + PA) / A.
Let the number of different key values on level LA be N_DIFF_LA.
Let the total number of records on level LA be TOTAL_LA.
Let R be N_DIFF_LA / TOTAL_LA, we assume this ratio is the same on the
leaf level.
Let the number of leaf pages be N.
Then the total number of different key values on the leaf level is:
N * R * N_DIFF_AVG_LEAF.
See REF01 for the implementation.

The above describes how to calculate the cardinality of an index.
This algorithm is executed for each n-prefix of a multi-column index
where n=1..n_uniq.
@} */

/* names of the tables from the persistent statistics storage */
#define TABLE_STATS_NAME	"mysql/innodb_table_stats"
#define TABLE_STATS_NAME_PRINT	"mysql.innodb_table_stats"
#define INDEX_STATS_NAME	"mysql/innodb_index_stats"
#define INDEX_STATS_NAME_PRINT	"mysql.innodb_index_stats"

#ifdef UNIV_STATS_DEBUG
#define DEBUG_PRINTF(fmt, ...)	printf(fmt, ## __VA_ARGS__)
#else /* UNIV_STATS_DEBUG */
#define DEBUG_PRINTF(fmt, ...)	/* noop */
#endif /* UNIV_STATS_DEBUG */

/* Gets the number of leaf pages to sample in persistent stats estimation */
#define N_SAMPLE_PAGES(index)					\
	static_cast<ib_uint64_t>(				\
		(index)->table->stats_sample_pages != 0		\
		? (index)->table->stats_sample_pages		\
		: srv_stats_persistent_sample_pages)

/* number of distinct records on a given level that are required to stop
descending to lower levels and fetch N_SAMPLE_PAGES(index) records
from that level */
#define N_DIFF_REQUIRED(index)	(N_SAMPLE_PAGES(index) * 10)

/* A dynamic array where we store the boundaries of each distinct group
of keys. For example if a btree level is:
index: 0,1,2,3,4,5,6,7,8,9,10,11,12
data:  b,b,b,b,b,b,g,g,j,j,j, x, y
then we would store 5,7,10,11,12 in the array. */
typedef std::vector<ib_uint64_t, ut_allocator<ib_uint64_t> >	boundaries_t;

/** Allocator type used for index_map_t. */
typedef ut_allocator<std::pair<const char* const, dict_index_t*> >
	index_map_t_allocator;

/** Auxiliary map used for sorting indexes by name in dict_stats_save(). */
typedef std::map<const char*, dict_index_t*, ut_strcmp_functor,
		index_map_t_allocator>	index_map_t;

/*********************************************************************//**
Checks whether an index should be ignored in stats manipulations:
* stats fetch
* stats recalc
* stats save
@return true if exists and all tables are ok */
UNIV_INLINE
bool
dict_stats_should_ignore_index(
/*===========================*/
	const dict_index_t*	index)	/*!< in: index */
{
	return((index->type & DICT_FTS)
	       || index->is_corrupted()
	       || dict_index_is_spatial(index)
	       || index->to_be_dropped
	       || !index->is_committed());
}

/** Executes a given SQL statement using the InnoDB internal SQL parser.
This function will free the pinfo object.
@param[in,out]	pinfo	pinfo to pass to que_eval_sql() must already
have any literals bound to it
@param[in]	sql	SQL string to execute
@param[in,out]	trx	in case of NULL the function will allocate and
free the trx object. If it is not NULL then it will be rolled back
only in the case of error, but not freed.
@return DB_SUCCESS or error code */
static
dberr_t
dict_stats_exec_sql(
	pars_info_t*	pinfo,
	const char*	sql,
	trx_t*		trx)
{
	dberr_t	err;
	bool	trx_started = false;

	ut_ad(rw_lock_own(dict_operation_lock, RW_LOCK_X));
	ut_ad(!mutex_own(&dict_sys->mutex));

	if (trx == NULL) {
		trx = trx_allocate_for_background();
		trx_started = true;

		if (srv_read_only_mode) {
			trx_start_internal_read_only(trx);
		} else {
			trx_start_internal(trx);
		}
	}

	err = que_eval_sql(pinfo, sql, FALSE, trx); /* pinfo is freed here */

	DBUG_EXECUTE_IF("stats_index_error",
		if (!trx_started) {
			err = DB_STATS_DO_NOT_EXIST;
			trx->error_state = DB_STATS_DO_NOT_EXIST;
		});

	if (!trx_started && err == DB_SUCCESS) {
		return(DB_SUCCESS);
	}

	if (err == DB_SUCCESS) {
		trx_commit_for_mysql(trx);
	} else {
		trx->op_info = "rollback of internal trx on stats tables";
		trx->dict_operation_lock_mode = RW_X_LATCH;
		trx_rollback_to_savepoint(trx, NULL);
		trx->dict_operation_lock_mode = 0;
		trx->op_info = "";
		ut_a(trx->error_state == DB_SUCCESS);
	}

	if (trx_started) {
		trx_free_for_background(trx);
	}

	return(err);
}

/*********************************************************************//**
Duplicate a table object and its indexes.
This function creates a dummy dict_table_t object and initializes the
following table and index members:
dict_table_t::id (copied)
dict_table_t::heap (newly created)
dict_table_t::name (copied)
dict_table_t::corrupted (copied)
dict_table_t::indexes<> (newly created)
dict_table_t::magic_n
for each entry in dict_table_t::indexes, the following are initialized:
(indexes that have DICT_FTS set in index->type are skipped)
dict_index_t::id (copied)
dict_index_t::name (copied)
dict_index_t::table_name (points to the copied table name)
dict_index_t::table (points to the above semi-initialized object)
dict_index_t::type (copied)
dict_index_t::to_be_dropped (copied)
dict_index_t::online_status (copied)
dict_index_t::n_uniq (copied)
dict_index_t::fields[] (newly created, only first n_uniq, only fields[i].name)
dict_index_t::indexes<> (newly created)
dict_index_t::stat_n_diff_key_vals[] (only allocated, left uninitialized)
dict_index_t::stat_n_sample_sizes[] (only allocated, left uninitialized)
dict_index_t::stat_n_non_null_key_vals[] (only allocated, left uninitialized)
dict_index_t::magic_n
The returned object should be freed with dict_stats_table_clone_free()
when no longer needed.
@return incomplete table object */
static
dict_table_t*
dict_stats_table_clone_create(
/*==========================*/
	const dict_table_t*	table)	/*!< in: table whose stats to copy */
{
	size_t			heap_size;
	const dict_index_t*	index;

	/* Estimate the size needed for the table and all of its indexes */

	heap_size = 0;
	heap_size += sizeof(dict_table_t);
	heap_size += strlen(table->name.m_name) + 1;

	for (index = table->first_index();
	     index != NULL;
	     index = index->next()) {

		if (dict_stats_should_ignore_index(index)) {
			continue;
		}

		ut_ad(!dict_index_is_ibuf(index));

		ulint	n_uniq = dict_index_get_n_unique(index);

		heap_size += sizeof(dict_index_t);
		heap_size += strlen(index->name) + 1;
		heap_size += n_uniq * sizeof(index->fields[0]);
		for (ulint i = 0; i < n_uniq; i++) {
			heap_size += strlen(index->fields[i].name) + 1;
		}
		heap_size += n_uniq * sizeof(index->stat_n_diff_key_vals[0]);
		heap_size += n_uniq * sizeof(index->stat_n_sample_sizes[0]);
		heap_size += n_uniq * sizeof(index->stat_n_non_null_key_vals[0]);
	}

	/* Allocate the memory and copy the members */

	mem_heap_t*	heap;

	heap = mem_heap_create(heap_size);

	dict_table_t*	t;

	t = (dict_table_t*) mem_heap_alloc(heap, sizeof(*t));

	UNIV_MEM_ASSERT_RW_ABORT(&table->id, sizeof(table->id));
	t->id = table->id;

	t->heap = heap;

	t->name.m_name = mem_heap_strdup(heap, table->name.m_name);

	/* This private object "t" is not shared with other threads, so
	we do not need the stats_latch (thus we pass false below). The
	dict_table_stats_lock()/unlock() routines will do nothing. */
	dict_table_stats_latch_create(t, false);

	UT_LIST_INIT(t->indexes, &dict_index_t::indexes);

	for (index = table->first_index();
	     index != NULL;
	     index = index->next()) {

		if (dict_stats_should_ignore_index(index)) {
			continue;
		}

		ut_ad(!dict_index_is_ibuf(index));

		dict_index_t*	idx;

		idx = (dict_index_t*) mem_heap_alloc(heap, sizeof(*idx));

		UNIV_MEM_ASSERT_RW_ABORT(&index->id, sizeof(index->id));
		idx->id = index->id;
		idx->space = index->space;

		idx->name = mem_heap_strdup(heap, index->name);

		idx->table_name = t->name.m_name;

		idx->table = t;

		idx->type = index->type;

		idx->to_be_dropped = 0;

		idx->online_status = ONLINE_INDEX_COMPLETE;
		idx->set_committed(true);

		idx->n_uniq = index->n_uniq;

		idx->fields = (dict_field_t*) mem_heap_alloc(
			heap, idx->n_uniq * sizeof(idx->fields[0]));

		for (ulint i = 0; i < idx->n_uniq; i++) {
			idx->fields[i].name = mem_heap_strdup(
				heap, index->fields[i].name);
		}

		/* hook idx into t->indexes */
		UT_LIST_ADD_LAST(t->indexes, idx);

		idx->stat_n_diff_key_vals = (ib_uint64_t*) mem_heap_alloc(
			heap,
			idx->n_uniq * sizeof(idx->stat_n_diff_key_vals[0]));

		idx->stat_n_sample_sizes = (ib_uint64_t*) mem_heap_alloc(
			heap,
			idx->n_uniq * sizeof(idx->stat_n_sample_sizes[0]));

		idx->stat_n_non_null_key_vals = (ib_uint64_t*) mem_heap_alloc(
			heap,
			idx->n_uniq * sizeof(idx->stat_n_non_null_key_vals[0]));
		ut_d(idx->magic_n = DICT_INDEX_MAGIC_N);
	}

	ut_d(t->magic_n = DICT_TABLE_MAGIC_N);

	return(t);
}

/*********************************************************************//**
Free the resources occupied by an object returned by
dict_stats_table_clone_create(). */
static
void
dict_stats_table_clone_free(
/*========================*/
	dict_table_t*	t)	/*!< in: dummy table object to free */
{
	dict_table_stats_latch_destroy(t);
	mem_heap_free(t->heap);
}

/*********************************************************************//**
Write all zeros (or 1 where it makes sense) into an index
statistics members. The resulting stats correspond to an empty index.
The caller must own index's table stats latch in X mode
(dict_table_stats_lock(table, RW_X_LATCH)) */
static
void
dict_stats_empty_index(
/*===================*/
	dict_index_t*	index)	/*!< in/out: index */
{
	ut_ad(!(index->type & DICT_FTS));
	ut_ad(!dict_index_is_ibuf(index));

	ulint	n_uniq = index->n_uniq;

	for (ulint i = 0; i < n_uniq; i++) {
		index->stat_n_diff_key_vals[i] = 0;
		index->stat_n_sample_sizes[i] = 1;
		index->stat_n_non_null_key_vals[i] = 0;
	}

	index->stat_index_size = 1;
	index->stat_n_leaf_pages = 1;
}

/*********************************************************************//**
Write all zeros (or 1 where it makes sense) into a table and its indexes'
statistics members. The resulting stats correspond to an empty table. */
static
void
dict_stats_empty_table(
/*===================*/
	dict_table_t*	table)	/*!< in/out: table */
{
	/* Zero the stats members */

	dict_table_stats_lock(table, RW_X_LATCH);

	table->stat_n_rows = 0;
	table->stat_clustered_index_size = 1;
	/* 1 page for each index, not counting the clustered */
	table->stat_sum_of_other_index_sizes
		= UT_LIST_GET_LEN(table->indexes) - 1;
	table->stat_modified_counter = 0;

	dict_index_t*	index;

	for (index = table->first_index();
	     index != NULL;
	     index = index->next()) {

		if (index->type & DICT_FTS) {
			continue;
		}

		ut_ad(!dict_index_is_ibuf(index));

		dict_stats_empty_index(index);
	}

	table->stat_initialized = TRUE;

	dict_table_stats_unlock(table, RW_X_LATCH);
}

/*********************************************************************//**
Check whether index's stats are initialized (assert if they are not). */
static
void
dict_stats_assert_initialized_index(
/*================================*/
	const dict_index_t*	index)	/*!< in: index */
{
	UNIV_MEM_ASSERT_RW_ABORT(
		index->stat_n_diff_key_vals,
		index->n_uniq * sizeof(index->stat_n_diff_key_vals[0]));

	UNIV_MEM_ASSERT_RW_ABORT(
		index->stat_n_sample_sizes,
		index->n_uniq * sizeof(index->stat_n_sample_sizes[0]));

	UNIV_MEM_ASSERT_RW_ABORT(
		index->stat_n_non_null_key_vals,
		index->n_uniq * sizeof(index->stat_n_non_null_key_vals[0]));

	UNIV_MEM_ASSERT_RW_ABORT(
		&index->stat_index_size,
		sizeof(index->stat_index_size));

	UNIV_MEM_ASSERT_RW_ABORT(
		&index->stat_n_leaf_pages,
		sizeof(index->stat_n_leaf_pages));
}

/*********************************************************************//**
Check whether table's stats are initialized (assert if they are not). */
static
void
dict_stats_assert_initialized(
/*==========================*/
	const dict_table_t*	table)	/*!< in: table */
{
	ut_a(table->stat_initialized);

	UNIV_MEM_ASSERT_RW_ABORT(&table->stats_last_recalc,
			   sizeof(table->stats_last_recalc));

	UNIV_MEM_ASSERT_RW_ABORT(&table->stat_persistent,
			   sizeof(table->stat_persistent));

	UNIV_MEM_ASSERT_RW_ABORT(&table->stats_auto_recalc,
			   sizeof(table->stats_auto_recalc));

	UNIV_MEM_ASSERT_RW_ABORT(&table->stats_sample_pages,
			   sizeof(table->stats_sample_pages));

	UNIV_MEM_ASSERT_RW_ABORT(&table->stat_n_rows,
			   sizeof(table->stat_n_rows));

	UNIV_MEM_ASSERT_RW_ABORT(&table->stat_clustered_index_size,
			   sizeof(table->stat_clustered_index_size));

	UNIV_MEM_ASSERT_RW_ABORT(&table->stat_sum_of_other_index_sizes,
			   sizeof(table->stat_sum_of_other_index_sizes));

	UNIV_MEM_ASSERT_RW_ABORT(&table->stat_modified_counter,
			   sizeof(table->stat_modified_counter));

	UNIV_MEM_ASSERT_RW_ABORT(&table->stats_bg_flag,
			   sizeof(table->stats_bg_flag));

	for (const dict_index_t* index = table->first_index();
	     index != NULL;
	     index = index->next()) {

		if (!dict_stats_should_ignore_index(index)) {
			dict_stats_assert_initialized_index(index);
		}
	}
}

#define INDEX_EQ(i1, i2) \
	((i1) != NULL \
	 && (i2) != NULL \
	 && (i1)->space == (i2)->space \
	 && (i1)->id == (i2)->id \
	 && strcmp((i1)->name, (i2)->name) == 0)

/*********************************************************************//**
Copy table and index statistics from one table to another, including index
stats. Extra indexes in src are ignored and extra indexes in dst are
initialized to correspond to an empty index. */
static
void
dict_stats_copy(
/*============*/
	dict_table_t*		dst,	/*!< in/out: destination table */
	const dict_table_t*	src)	/*!< in: source table */
{
	dst->stats_last_recalc = src->stats_last_recalc;
	dst->stat_n_rows = src->stat_n_rows;
	dst->stat_clustered_index_size = src->stat_clustered_index_size;
	dst->stat_sum_of_other_index_sizes = src->stat_sum_of_other_index_sizes;
	dst->stat_modified_counter = src->stat_modified_counter;

	dict_index_t*	dst_idx;
	const dict_index_t*	src_idx;

	for (dst_idx = dst->first_index(),
	     src_idx = src->first_index();
	     dst_idx != NULL;
	     dst_idx = dst_idx->next(),
	     (src_idx != NULL
	      && (src_idx = src_idx->next()))) {

		if (dict_stats_should_ignore_index(dst_idx)) {
			if (!(dst_idx->type & DICT_FTS)) {
				dict_stats_empty_index(dst_idx);
			}
			continue;
		}

		ut_ad(!dict_index_is_ibuf(dst_idx));

		if (!INDEX_EQ(src_idx, dst_idx)) {
			for (src_idx = src->first_index();
			     src_idx != NULL;
			     src_idx = src_idx->next()) {

				if (INDEX_EQ(src_idx, dst_idx)) {
					break;
				}
			}
		}

		if (!INDEX_EQ(src_idx, dst_idx)) {
			dict_stats_empty_index(dst_idx);
			continue;
		}

		ulint	n_copy_el;

		if (dst_idx->n_uniq > src_idx->n_uniq) {
			n_copy_el = src_idx->n_uniq;
			/* Since src is smaller some elements in dst
			will remain untouched by the following memmove(),
			thus we init all of them here. */
			dict_stats_empty_index(dst_idx);
		} else {
			n_copy_el = dst_idx->n_uniq;
		}

		memmove(dst_idx->stat_n_diff_key_vals,
			src_idx->stat_n_diff_key_vals,
			n_copy_el * sizeof(dst_idx->stat_n_diff_key_vals[0]));

		memmove(dst_idx->stat_n_sample_sizes,
			src_idx->stat_n_sample_sizes,
			n_copy_el * sizeof(dst_idx->stat_n_sample_sizes[0]));

		memmove(dst_idx->stat_n_non_null_key_vals,
			src_idx->stat_n_non_null_key_vals,
			n_copy_el * sizeof(dst_idx->stat_n_non_null_key_vals[0]));

		dst_idx->stat_index_size = src_idx->stat_index_size;

		dst_idx->stat_n_leaf_pages = src_idx->stat_n_leaf_pages;
	}

	dst->stat_initialized = TRUE;
}

/** Duplicate the stats of a table and its indexes.
This function creates a dummy dict_table_t object and copies the input
table's stats into it. The returned table object is not in the dictionary
cache and cannot be accessed by any other threads. In addition to the
members copied in dict_stats_table_clone_create() this function initializes
the following:
dict_table_t::stat_initialized
dict_table_t::stat_persistent
dict_table_t::stat_n_rows
dict_table_t::stat_clustered_index_size
dict_table_t::stat_sum_of_other_index_sizes
dict_table_t::stat_modified_counter
dict_index_t::stat_n_diff_key_vals[]
dict_index_t::stat_n_sample_sizes[]
dict_index_t::stat_n_non_null_key_vals[]
dict_index_t::stat_index_size
dict_index_t::stat_n_leaf_pages
The returned object should be freed with dict_stats_snapshot_free()
when no longer needed.
@param[in]	table	table whose stats to copy
@return incomplete table object */
static
dict_table_t*
dict_stats_snapshot_create(
	dict_table_t*	table)
{
	mutex_enter(&dict_sys->mutex);

	dict_table_stats_lock(table, RW_S_LATCH);

	dict_stats_assert_initialized(table);

	dict_table_t*	t;

	t = dict_stats_table_clone_create(table);

	dict_stats_copy(t, table);

	t->stat_persistent = table->stat_persistent;
	t->stats_auto_recalc = table->stats_auto_recalc;
	t->stats_sample_pages = table->stats_sample_pages;
	t->stats_bg_flag = table->stats_bg_flag;

	dict_table_stats_unlock(table, RW_S_LATCH);

	mutex_exit(&dict_sys->mutex);

	return(t);
}

/*********************************************************************//**
Free the resources occupied by an object returned by
dict_stats_snapshot_create(). */
static
void
dict_stats_snapshot_free(
/*=====================*/
	dict_table_t*	t)	/*!< in: dummy table object to free */
{
	dict_stats_table_clone_free(t);
}

/*********************************************************************//**
Calculates new estimates for index statistics. This function is
relatively quick and is used to calculate transient statistics that
are not saved on disk. This was the only way to calculate statistics
before the Persistent Statistics feature was introduced. */
static
void
dict_stats_update_transient_for_index(
/*==================================*/
	dict_index_t*	index)	/*!< in/out: index */
{
	if (srv_force_recovery >= SRV_FORCE_NO_TRX_UNDO
	    && (srv_force_recovery >= SRV_FORCE_NO_LOG_REDO
		|| !index->is_clustered())) {
		/* If we have set a high innodb_force_recovery
		level, do not calculate statistics, as a badly
		corrupted index can cause a crash in it.
		Initialize some bogus index cardinality
		statistics, so that the data can be queried in
		various means, also via secondary indexes. */
		dict_stats_empty_index(index);
#if defined UNIV_DEBUG || defined UNIV_IBUF_DEBUG
	} else if (ibuf_debug && !index->is_clustered()) {
		dict_stats_empty_index(index);
#endif /* UNIV_DEBUG || UNIV_IBUF_DEBUG */
	} else {
		mtr_t	mtr;
		ulint	size;

		mtr_start(&mtr);
		dict_disable_redo_if_temporary(index->table, &mtr);

		mtr_s_lock(dict_index_get_lock(index), &mtr);

		size = btr_get_size(index, BTR_TOTAL_SIZE, &mtr);

		if (size != ULINT_UNDEFINED) {
			index->stat_index_size = size;

			size = btr_get_size(
				index, BTR_N_LEAF_PAGES, &mtr);
		}

		mtr_commit(&mtr);

		switch (size) {
		case ULINT_UNDEFINED:
			dict_stats_empty_index(index);
			return;
		case 0:
			/* The root node of the tree is a leaf */
			size = 1;
		}

		index->stat_n_leaf_pages = size;

		/* We don't handle the return value since it will be false
		only when some thread is dropping the table and we don't
		have to empty the statistics of the to be dropped index */
		btr_estimate_number_of_different_key_vals(index);
	}
}

/*********************************************************************//**
Calculates new estimates for table and index statistics. This function
is relatively quick and is used to calculate transient statistics that
are not saved on disk.
This was the only way to calculate statistics before the
Persistent Statistics feature was introduced. */
static
void
dict_stats_update_transient(
/*========================*/
	dict_table_t*	table)	/*!< in/out: table */
{
	dict_index_t*	index;
	ulint		sum_of_index_sizes	= 0;

	/* Find out the sizes of the indexes and how many different values
	for the key they approximately have */

	index = table->first_index();

	if (dict_table_is_discarded(table)) {
		/* Nothing to do. */
		dict_stats_empty_table(table);
		return;
	} else if (index == NULL) {
		/* Table definition is corrupt */

		ib::warn() << "Table " << table->name
			<< " has no indexes. Cannot calculate statistics.";
		dict_stats_empty_table(table);
		return;
	}

	for (; index != NULL; index = index->next()) {

		ut_ad(!dict_index_is_ibuf(index));

		if (index->type & DICT_FTS || dict_index_is_spatial(index)) {
			continue;
		}

		dict_stats_empty_index(index);

		if (dict_stats_should_ignore_index(index)) {
			continue;
		}

		dict_stats_update_transient_for_index(index);

		sum_of_index_sizes += index->stat_index_size;
	}

	index = table->first_index();

	table->stat_n_rows = index->stat_n_diff_key_vals[
		dict_index_get_n_unique(index) - 1];

	table->stat_clustered_index_size = index->stat_index_size;

	table->stat_sum_of_other_index_sizes = sum_of_index_sizes
		- index->stat_index_size;

	table->stats_last_recalc = ut_time();

	table->stat_modified_counter = 0;

	table->stat_initialized = TRUE;
}

/* @{ Pseudo code about the relation between the following functions

let N = N_SAMPLE_PAGES(index)

dict_stats_analyze_index()
  for each n_prefix
    search for good enough level:
      dict_stats_analyze_index_level() // only called if level has <= N pages
        // full scan of the level in one mtr
        collect statistics about the given level
      if we are not satisfied with the level, search next lower level
    we have found a good enough level here
    dict_stats_analyze_index_for_n_prefix(that level, stats collected above)
      // full scan of the level in one mtr
      dive below some records and analyze the leaf page there:
      dict_stats_analyze_index_below_cur()
@} */

/*********************************************************************//**
Find the total number and the number of distinct keys on a given level in
an index. Each of the 1..n_uniq prefixes are looked up and the results are
saved in the array n_diff[0] .. n_diff[n_uniq - 1]. The total number of
records on the level is saved in total_recs.
Also, the index of the last record in each group of equal records is saved
in n_diff_boundaries[0..n_uniq - 1], records indexing starts from the leftmost
record on the level and continues cross pages boundaries, counting from 0. */
static
void
dict_stats_analyze_index_level(
/*===========================*/
	dict_index_t*	index,		/*!< in: index */
	ulint		level,		/*!< in: level */
	ib_uint64_t*	n_diff,		/*!< out: array for number of
					distinct keys for all prefixes */
	ib_uint64_t*	total_recs,	/*!< out: total number of records */
	ib_uint64_t*	total_pages,	/*!< out: total number of pages */
	boundaries_t*	n_diff_boundaries,/*!< out: boundaries of the groups
					of distinct keys */
	mtr_t*		mtr)		/*!< in/out: mini-transaction */
{
	ulint		n_uniq;
	mem_heap_t*	heap;
	btr_pcur_t	pcur;
	const page_t*	page;
	const rec_t*	rec;
	const rec_t*	prev_rec;
	bool		prev_rec_is_copied;
	byte*		prev_rec_buf = NULL;
	ulint		prev_rec_buf_size = 0;
	ulint*		rec_offsets;
	ulint*		prev_rec_offsets;
	ulint		i;

	DEBUG_PRINTF("    %s(table=%s, index=%s, level=%lu)\n", __func__,
		     index->table->name, index->name, level);

	ut_ad(mtr_memo_contains(mtr, dict_index_get_lock(index),
				MTR_MEMO_SX_LOCK));

	n_uniq = dict_index_get_n_unique(index);

	/* elements in the n_diff array are 0..n_uniq-1 (inclusive) */
	memset(n_diff, 0x0, n_uniq * sizeof(n_diff[0]));

	/* Allocate space for the offsets header (the allocation size at
	offsets[0] and the REC_OFFS_HEADER_SIZE bytes), and n_fields + 1,
	so that this will never be less than the size calculated in
	rec_get_offsets_func(). */
	i = (REC_OFFS_HEADER_SIZE + 1 + 1) + index->n_fields;

	heap = mem_heap_create((2 * sizeof *rec_offsets) * i);
	rec_offsets = static_cast<ulint*>(
		mem_heap_alloc(heap, i * sizeof *rec_offsets));
	prev_rec_offsets = static_cast<ulint*>(
		mem_heap_alloc(heap, i * sizeof *prev_rec_offsets));
	rec_offs_set_n_alloc(rec_offsets, i);
	rec_offs_set_n_alloc(prev_rec_offsets, i);

	/* reset the dynamic arrays n_diff_boundaries[0..n_uniq-1] */
	if (n_diff_boundaries != NULL) {
		for (i = 0; i < n_uniq; i++) {
			n_diff_boundaries[i].erase(
				n_diff_boundaries[i].begin(),
				n_diff_boundaries[i].end());
		}
	}

	/* Position pcur on the leftmost record on the leftmost page
	on the desired level. */

	btr_pcur_open_at_index_side(
		true, index, BTR_SEARCH_TREE | BTR_ALREADY_S_LATCHED,
		&pcur, true, level, mtr);
	btr_pcur_move_to_next_on_page(&pcur);

	page = btr_pcur_get_page(&pcur);

	/* The page must not be empty, except when
	it is the root page (and the whole index is empty). */
	ut_ad(btr_pcur_is_on_user_rec(&pcur) || page_is_leaf(page));
	ut_ad(btr_pcur_get_rec(&pcur)
	      == page_rec_get_next_const(page_get_infimum_rec(page)));

	/* check that we are indeed on the desired level */
	ut_a(btr_page_get_level(page, mtr) == level);

	/* there should not be any pages on the left */
	ut_a(btr_page_get_prev(page, mtr) == FIL_NULL);

	/* check whether the first record on the leftmost page is marked
	as such, if we are on a non-leaf level */
	ut_a((level == 0)
	     == !(REC_INFO_MIN_REC_FLAG & rec_get_info_bits(
			  btr_pcur_get_rec(&pcur), page_is_comp(page))));

	prev_rec = NULL;
	prev_rec_is_copied = false;

	/* no records by default */
	*total_recs = 0;

	*total_pages = 0;

	/* iterate over all user records on this level
	and compare each two adjacent ones, even the last on page
	X and the fist on page X+1 */
	for (;
	     btr_pcur_is_on_user_rec(&pcur);
	     btr_pcur_move_to_next_user_rec(&pcur, mtr)) {

		bool	rec_is_last_on_page;

		rec = btr_pcur_get_rec(&pcur);

		/* If rec and prev_rec are on different pages, then prev_rec
		must have been copied, because we hold latch only on the page
		where rec resides. */
		if (prev_rec != NULL
		    && page_align(rec) != page_align(prev_rec)) {

			ut_a(prev_rec_is_copied);
		}

		rec_is_last_on_page =
			page_rec_is_supremum(page_rec_get_next_const(rec));

		/* increment the pages counter at the end of each page */
		if (rec_is_last_on_page) {

			(*total_pages)++;
		}

		/* Skip delete-marked records on the leaf level. If we
		do not skip them, then ANALYZE quickly after DELETE
		could count them or not (purge may have already wiped
		them away) which brings non-determinism. We skip only
		leaf-level delete marks because delete marks on
		non-leaf level do not make sense. */

		if (level == 0 && (srv_stats_include_delete_marked ? 0:
		    rec_get_deleted_flag(
			    rec,
			    page_is_comp(btr_pcur_get_page(&pcur))))) {

			if (rec_is_last_on_page
			    && !prev_rec_is_copied
			    && prev_rec != NULL) {
				/* copy prev_rec */

				prev_rec_offsets = rec_get_offsets(
					prev_rec, index, prev_rec_offsets,
					n_uniq, &heap);

				prev_rec = rec_copy_prefix_to_buf(
					prev_rec, index,
					rec_offs_n_fields(prev_rec_offsets),
					&prev_rec_buf, &prev_rec_buf_size);

				prev_rec_is_copied = true;
			}

			continue;
		}
		rec_offsets = rec_get_offsets(
			rec, index, rec_offsets, n_uniq, &heap);

		(*total_recs)++;

		if (prev_rec != NULL) {
			ulint	matched_fields;

			prev_rec_offsets = rec_get_offsets(
				prev_rec, index, prev_rec_offsets,
				n_uniq, &heap);

			cmp_rec_rec_with_match(rec,
					       prev_rec,
					       rec_offsets,
					       prev_rec_offsets,
					       index,
					       FALSE,
					       &matched_fields);

			for (i = matched_fields; i < n_uniq; i++) {

				if (n_diff_boundaries != NULL) {
					/* push the index of the previous
					record, that is - the last one from
					a group of equal keys */

					ib_uint64_t	idx;

					/* the index of the current record
					is total_recs - 1, the index of the
					previous record is total_recs - 2;
					we know that idx is not going to
					become negative here because if we
					are in this branch then there is a
					previous record and thus
					total_recs >= 2 */
					idx = *total_recs - 2;

					n_diff_boundaries[i].push_back(idx);
				}

				/* increment the number of different keys
				for n_prefix=i+1 (e.g. if i=0 then we increment
				for n_prefix=1 which is stored in n_diff[0]) */
				n_diff[i]++;
			}
		} else {
			/* this is the first non-delete marked record */
			for (i = 0; i < n_uniq; i++) {
				n_diff[i] = 1;
			}
		}

		if (rec_is_last_on_page) {
			/* end of a page has been reached */

			/* we need to copy the record instead of assigning
			like prev_rec = rec; because when we traverse the
			records on this level at some point we will jump from
			one page to the next and then rec and prev_rec will
			be on different pages and
			btr_pcur_move_to_next_user_rec() will release the
			latch on the page that prev_rec is on */
			prev_rec = rec_copy_prefix_to_buf(
				rec, index, rec_offs_n_fields(rec_offsets),
				&prev_rec_buf, &prev_rec_buf_size);
			prev_rec_is_copied = true;

		} else {
			/* still on the same page, the next call to
			btr_pcur_move_to_next_user_rec() will not jump
			on the next page, we can simply assign pointers
			instead of copying the records like above */

			prev_rec = rec;
			prev_rec_is_copied = false;
		}
	}

	/* if *total_pages is left untouched then the above loop was not
	entered at all and there is one page in the whole tree which is
	empty or the loop was entered but this is level 0, contains one page
	and all records are delete-marked */
	if (*total_pages == 0) {

		ut_ad(level == 0);
		ut_ad(*total_recs == 0);

		*total_pages = 1;
	}

	/* if there are records on this level and boundaries
	should be saved */
	if (*total_recs > 0 && n_diff_boundaries != NULL) {

		/* remember the index of the last record on the level as the
		last one from the last group of equal keys; this holds for
		all possible prefixes */
		for (i = 0; i < n_uniq; i++) {
			ib_uint64_t	idx;

			idx = *total_recs - 1;

			n_diff_boundaries[i].push_back(idx);
		}
	}

	/* now in n_diff_boundaries[i] there are exactly n_diff[i] integers,
	for i=0..n_uniq-1 */

#ifdef UNIV_STATS_DEBUG
	for (i = 0; i < n_uniq; i++) {

		DEBUG_PRINTF("    %s(): total recs: " UINT64PF
			     ", total pages: " UINT64PF
			     ", n_diff[%lu]: " UINT64PF "\n",
			     __func__, *total_recs,
			     *total_pages,
			     i, n_diff[i]);

#if 0
		if (n_diff_boundaries != NULL) {
			ib_uint64_t	j;

			DEBUG_PRINTF("    %s(): boundaries[%lu]: ",
				     __func__, i);

			for (j = 0; j < n_diff[i]; j++) {
				ib_uint64_t	idx;

				idx = n_diff_boundaries[i][j];

				DEBUG_PRINTF(UINT64PF "=" UINT64PF ", ",
					     j, idx);
			}
			DEBUG_PRINTF("\n");
		}
#endif
	}
#endif /* UNIV_STATS_DEBUG */

	/* Release the latch on the last page, because that is not done by
	btr_pcur_close(). This function works also for non-leaf pages. */
	btr_leaf_page_release(btr_pcur_get_block(&pcur), BTR_SEARCH_LEAF, mtr);

	btr_pcur_close(&pcur);
	ut_free(prev_rec_buf);
	mem_heap_free(heap);
}

/* aux enum for controlling the behavior of dict_stats_scan_page() @{ */
enum page_scan_method_t {
	COUNT_ALL_NON_BORING_AND_SKIP_DEL_MARKED,/* scan all records on
				the given page and count the number of
				distinct ones, also ignore delete marked
				records */
	QUIT_ON_FIRST_NON_BORING,/* quit when the first record that differs
				from its right neighbor is found */
	COUNT_ALL_NON_BORING_INCLUDE_DEL_MARKED/* scan all records on
				the given page and count the number of
				distinct ones, include delete marked
				records */
};
/* @} */

/** Scan a page, reading records from left to right and counting the number
of distinct records (looking only at the first n_prefix
columns) and the number of external pages pointed by records from this page.
If scan_method is QUIT_ON_FIRST_NON_BORING then the function
will return as soon as it finds a record that does not match its neighbor
to the right, which means that in the case of QUIT_ON_FIRST_NON_BORING the
returned n_diff can either be 0 (empty page), 1 (the whole page has all keys
equal) or 2 (the function found a non-boring record and returned).
@param[out]	out_rec			record, or NULL
@param[out]	offsets1		rec_get_offsets() working space (must
be big enough)
@param[out]	offsets2		rec_get_offsets() working space (must
be big enough)
@param[in]	index			index of the page
@param[in]	page			the page to scan
@param[in]	n_prefix		look at the first n_prefix columns
@param[in]	scan_method		scan to the end of the page or not
@param[out]	n_diff			number of distinct records encountered
@param[out]	n_external_pages	if this is non-NULL then it will be set
to the number of externally stored pages which were encountered
@return offsets1 or offsets2 (the offsets of *out_rec),
or NULL if the page is empty and does not contain user records. */
UNIV_INLINE
ulint*
dict_stats_scan_page(
	const rec_t**		out_rec,
	ulint*			offsets1,
	ulint*			offsets2,
	const dict_index_t*	index,
	const page_t*		page,
	ulint			n_prefix,
	page_scan_method_t	scan_method,
	ib_uint64_t*		n_diff,
	ib_uint64_t*		n_external_pages)
{
	ulint*		offsets_rec		= offsets1;
	ulint*		offsets_next_rec	= offsets2;
	const rec_t*	rec;
	const rec_t*	next_rec;
	/* A dummy heap, to be passed to rec_get_offsets().
	Because offsets1,offsets2 should be big enough,
	this memory heap should never be used. */
	mem_heap_t*	heap			= NULL;
	const rec_t*	(*get_next)(const rec_t*);

	if (scan_method == COUNT_ALL_NON_BORING_AND_SKIP_DEL_MARKED) {
		get_next = page_rec_get_next_non_del_marked;
	} else {
		get_next = page_rec_get_next_const;
	}

	const bool	should_count_external_pages = n_external_pages != NULL;

	if (should_count_external_pages) {
		*n_external_pages = 0;
	}

	rec = get_next(page_get_infimum_rec(page));

	if (page_rec_is_supremum(rec)) {
		/* the page is empty or contains only delete-marked records */
		*n_diff = 0;
		*out_rec = NULL;
		return(NULL);
	}

	offsets_rec = rec_get_offsets(rec, index, offsets_rec,
				      ULINT_UNDEFINED, &heap);

	if (should_count_external_pages) {
		*n_external_pages += lob::btr_rec_get_externally_stored_len(
			rec, offsets_rec);
	}

	next_rec = get_next(rec);

	*n_diff = 1;

	while (!page_rec_is_supremum(next_rec)) {

		ulint	matched_fields;

		offsets_next_rec = rec_get_offsets(next_rec, index,
						   offsets_next_rec,
						   ULINT_UNDEFINED,
						   &heap);

		/* check whether rec != next_rec when looking at
		the first n_prefix fields */
		cmp_rec_rec_with_match(rec, next_rec,
				       offsets_rec, offsets_next_rec,
				       index, FALSE, &matched_fields);

		if (matched_fields < n_prefix) {
			/* rec != next_rec, => rec is non-boring */

			(*n_diff)++;

			if (scan_method == QUIT_ON_FIRST_NON_BORING) {
				break;
			}
		}

		rec = next_rec;
		{
			/* Assign offsets_rec = offsets_next_rec
			so that offsets_rec matches with rec which
			was just assigned rec = next_rec above.
			Also need to point offsets_next_rec to the
			place where offsets_rec was pointing before
			because we have just 2 placeholders where
			data is actually stored:
			offsets1 and offsets2 and we
			are using them in circular fashion
			(offsets[_next]_rec are just pointers to
			those placeholders). */
			ulint*	offsets_tmp;
			offsets_tmp = offsets_rec;
			offsets_rec = offsets_next_rec;
			offsets_next_rec = offsets_tmp;
		}

		if (should_count_external_pages) {
			*n_external_pages += lob::btr_rec_get_externally_stored_len(
				rec, offsets_rec);
		}

		next_rec = get_next(next_rec);
	}

	/* offsets1,offsets2 should have been big enough */
	ut_a(heap == NULL);
	*out_rec = rec;
	return(offsets_rec);
}

/** Dive below the current position of a cursor and calculate the number of
distinct records on the leaf page, when looking at the fist n_prefix
columns. Also calculate the number of external pages pointed by records
on the leaf page.
@param[in]	cur			cursor
@param[in]	n_prefix		look at the first n_prefix columns
when comparing records
@param[out]	n_diff			number of distinct records
@param[out]	n_external_pages	number of external pages
*/
static
void
dict_stats_analyze_index_below_cur(
	const btr_cur_t*	cur,
	ulint			n_prefix,
	ib_uint64_t*		n_diff,
	ib_uint64_t*		n_external_pages)
{
	dict_index_t*	index;
	buf_block_t*	block;
	const page_t*	page;
	mem_heap_t*	heap;
	const rec_t*	rec;
	ulint*		offsets1;
	ulint*		offsets2;
	ulint*		offsets_rec;
	ulint		size;
	mtr_t		mtr;

	index = btr_cur_get_index(cur);

	/* Allocate offsets for the record and the node pointer, for
	node pointer records. In a secondary index, the node pointer
	record will consist of all index fields followed by a child
	page number.
	Allocate space for the offsets header (the allocation size at
	offsets[0] and the REC_OFFS_HEADER_SIZE bytes), and n_fields + 1,
	so that this will never be less than the size calculated in
	rec_get_offsets_func(). */
	size = (1 + REC_OFFS_HEADER_SIZE) + 1 + dict_index_get_n_fields(index);

	heap = mem_heap_create(size * (sizeof *offsets1 + sizeof *offsets2));

	offsets1 = static_cast<ulint*>(mem_heap_alloc(
			heap, size * sizeof *offsets1));

	offsets2 = static_cast<ulint*>(mem_heap_alloc(
			heap, size * sizeof *offsets2));

	rec_offs_set_n_alloc(offsets1, size);
	rec_offs_set_n_alloc(offsets2, size);

	rec = btr_cur_get_rec(cur);

	offsets_rec = rec_get_offsets(rec, index, offsets1,
				      ULINT_UNDEFINED, &heap);

	page_id_t		page_id(dict_index_get_space(index),
					btr_node_ptr_get_child_page_no(
						rec, offsets_rec));
	const page_size_t	page_size(dict_table_page_size(index->table));

	/* assume no external pages by default - in case we quit from this
	function without analyzing any leaf pages */
	*n_external_pages = 0;

	mtr_start(&mtr);

	/* descend to the leaf level on the B-tree */
	for (;;) {

		block = buf_page_get_gen(page_id, page_size, RW_S_LATCH,
					 NULL /* no guessed block */,
					 BUF_GET, __FILE__, __LINE__, &mtr);

		page = buf_block_get_frame(block);

		if (btr_page_get_level(page, mtr) == 0) {
			/* leaf level */
			break;
		}
		/* else */

		/* search for the first non-boring record on the page */
		offsets_rec = dict_stats_scan_page(
			&rec, offsets1, offsets2, index, page, n_prefix,
			QUIT_ON_FIRST_NON_BORING, n_diff, NULL);

		/* pages on level > 0 are not allowed to be empty */
		ut_a(offsets_rec != NULL);
		/* if page is not empty (offsets_rec != NULL) then n_diff must
		be > 0, otherwise there is a bug in dict_stats_scan_page() */
		ut_a(*n_diff > 0);

		if (*n_diff == 1) {
			mtr_commit(&mtr);

			/* page has all keys equal and the end of the page
			was reached by dict_stats_scan_page(), no need to
			descend to the leaf level */
			mem_heap_free(heap);
			/* can't get an estimate for n_external_pages here
			because we do not dive to the leaf level, assume no
			external pages (*n_external_pages was assigned to 0
			above). */
			return;
		}
		/* else */

		/* when we instruct dict_stats_scan_page() to quit on the
		first non-boring record it finds, then the returned n_diff
		can either be 0 (empty page), 1 (page has all keys equal) or
		2 (non-boring record was found) */
		ut_a(*n_diff == 2);

		/* we have a non-boring record in rec, descend below it */

		page_id.set_page_no(
			btr_node_ptr_get_child_page_no(rec, offsets_rec));
	}

	/* make sure we got a leaf page as a result from the above loop */
	ut_ad(btr_page_get_level(page, &mtr) == 0);

	/* scan the leaf page and find the number of distinct keys,
	when looking only at the first n_prefix columns; also estimate
	the number of externally stored pages pointed by records on this
	page */

	offsets_rec = dict_stats_scan_page(
		&rec, offsets1, offsets2, index, page, n_prefix,
		srv_stats_include_delete_marked ?
		COUNT_ALL_NON_BORING_INCLUDE_DEL_MARKED:
		COUNT_ALL_NON_BORING_AND_SKIP_DEL_MARKED, n_diff,
		n_external_pages);

#if 0
	DEBUG_PRINTF("      %s(): n_diff below page_no=%lu: " UINT64PF "\n",
		     __func__, page_no, n_diff);
#endif

	mtr_commit(&mtr);
	mem_heap_free(heap);
}

/** Input data that is used to calculate dict_index_t::stat_n_diff_key_vals[]
for each n-columns prefix (n from 1 to n_uniq). */
struct n_diff_data_t {
	/** Index of the level on which the descent through the btree
	stopped. level 0 is the leaf level. This is >= 1 because we
	avoid scanning the leaf level because it may contain too many
	pages and doing so is useless when combined with the random dives -
	if we are to scan the leaf level, this means a full scan and we can
	simply do that instead of fiddling with picking random records higher
	in the tree and to dive below them. At the start of the analyzing
	we may decide to do full scan of the leaf level, but then this
	structure is not used in that code path. */
	ulint		level;

	/** Number of records on the level where the descend through the btree
	stopped. When we scan the btree from the root, we stop at some mid
	level, choose some records from it and dive below them towards a leaf
	page to analyze. */
	ib_uint64_t	n_recs_on_level;

	/** Number of different key values that were found on the mid level. */
	ib_uint64_t	n_diff_on_level;

	/** Number of leaf pages that are analyzed. This is also the same as
	the number of records that we pick from the mid level and dive below
	them. */
	ib_uint64_t	n_leaf_pages_to_analyze;

	/** Cumulative sum of the number of different key values that were
	found on all analyzed pages. */
	ib_uint64_t	n_diff_all_analyzed_pages;

	/** Cumulative sum of the number of external pages (stored outside of
	the btree but in the same file segment). */
	ib_uint64_t	n_external_pages_sum;
};

/** Estimate the number of different key values in an index when looking at
the first n_prefix columns. For a given level in an index select
n_diff_data->n_leaf_pages_to_analyze records from that level and dive below
them to the corresponding leaf pages, then scan those leaf pages and save the
sampling results in n_diff_data->n_diff_all_analyzed_pages.
@param[in]	index			index
@param[in]	n_prefix		look at first 'n_prefix' columns when
comparing records
@param[in]	boundaries		a vector that contains
n_diff_data->n_diff_on_level integers each of which represents the index (on
level 'level', counting from left/smallest to right/biggest from 0) of the
last record from each group of distinct keys
@param[in,out]	n_diff_data		n_diff_all_analyzed_pages and
n_external_pages_sum in this structure will be set by this function. The
members level, n_diff_on_level and n_leaf_pages_to_analyze must be set by the
caller in advance - they are used by some calculations inside this function
@param[in,out]	mtr			mini-transaction */
static
void
dict_stats_analyze_index_for_n_prefix(
	dict_index_t*		index,
	ulint			n_prefix,
	const boundaries_t*	boundaries,
	n_diff_data_t*		n_diff_data,
	mtr_t*			mtr)
{
	btr_pcur_t	pcur;
	const page_t*	page;
	ib_uint64_t	rec_idx;
	ib_uint64_t	i;

#if 0
	DEBUG_PRINTF("    %s(table=%s, index=%s, level=%lu, n_prefix=%lu,"
		     " n_diff_on_level=" UINT64PF ")\n",
		     __func__, index->table->name, index->name, level,
		     n_prefix, n_diff_data->n_diff_on_level);
#endif

	ut_ad(mtr_memo_contains(mtr, dict_index_get_lock(index),
				MTR_MEMO_SX_LOCK));

	/* Position pcur on the leftmost record on the leftmost page
	on the desired level. */

	btr_pcur_open_at_index_side(
		true, index, BTR_SEARCH_TREE | BTR_ALREADY_S_LATCHED,
		&pcur, true, n_diff_data->level, mtr);
	btr_pcur_move_to_next_on_page(&pcur);

	page = btr_pcur_get_page(&pcur);

	const rec_t*	first_rec = btr_pcur_get_rec(&pcur);

	/* We shouldn't be scanning the leaf level. The caller of this function
	should have stopped the descend on level 1 or higher. */
	ut_ad(n_diff_data->level > 0);
	ut_ad(!page_is_leaf(page));

	/* The page must not be empty, except when
	it is the root page (and the whole index is empty). */
	ut_ad(btr_pcur_is_on_user_rec(&pcur));
	ut_ad(first_rec == page_rec_get_next_const(page_get_infimum_rec(page)));

	/* check that we are indeed on the desired level */
	ut_a(btr_page_get_level(page, mtr) == n_diff_data->level);

	/* there should not be any pages on the left */
	ut_a(btr_page_get_prev(page, mtr) == FIL_NULL);

	/* check whether the first record on the leftmost page is marked
	as such; we are on a non-leaf level */
	ut_a(rec_get_info_bits(first_rec, page_is_comp(page))
	     & REC_INFO_MIN_REC_FLAG);

	const ib_uint64_t	last_idx_on_level = boundaries->at(
		static_cast<unsigned>(n_diff_data->n_diff_on_level - 1));

	rec_idx = 0;

	n_diff_data->n_diff_all_analyzed_pages = 0;
	n_diff_data->n_external_pages_sum = 0;

	for (i = 0; i < n_diff_data->n_leaf_pages_to_analyze; i++) {
		/* there are n_diff_on_level elements
		in 'boundaries' and we divide those elements
		into n_leaf_pages_to_analyze segments, for example:

		let n_diff_on_level=100, n_leaf_pages_to_analyze=4, then:
		segment i=0:  [0, 24]
		segment i=1: [25, 49]
		segment i=2: [50, 74]
		segment i=3: [75, 99] or

		let n_diff_on_level=1, n_leaf_pages_to_analyze=1, then:
		segment i=0: [0, 0] or

		let n_diff_on_level=2, n_leaf_pages_to_analyze=2, then:
		segment i=0: [0, 0]
		segment i=1: [1, 1] or

		let n_diff_on_level=13, n_leaf_pages_to_analyze=7, then:
		segment i=0:  [0,  0]
		segment i=1:  [1,  2]
		segment i=2:  [3,  4]
		segment i=3:  [5,  6]
		segment i=4:  [7,  8]
		segment i=5:  [9, 10]
		segment i=6: [11, 12]

		then we select a random record from each segment and dive
		below it */
		const ib_uint64_t	n_diff = n_diff_data->n_diff_on_level;
		const ib_uint64_t	n_pick
			= n_diff_data->n_leaf_pages_to_analyze;

		const ib_uint64_t	left = n_diff * i / n_pick;
		const ib_uint64_t	right = n_diff * (i + 1) / n_pick - 1;

		ut_a(left <= right);
		ut_a(right <= last_idx_on_level);

		/* we do not pass (left, right) because we do not want to ask
		ut_rnd_interval() to work with too big numbers since
		ib_uint64_t could be bigger than ulint */
		const ulint	rnd = ut_rnd_interval(
			0, static_cast<ulint>(right - left));

		const ib_uint64_t	dive_below_idx
			= boundaries->at(static_cast<unsigned>(left + rnd));

#if 0
		DEBUG_PRINTF("    %s(): dive below record with index="
			     UINT64PF "\n", __func__, dive_below_idx);
#endif

		/* seek to the record with index dive_below_idx */
		while (rec_idx < dive_below_idx
		       && btr_pcur_is_on_user_rec(&pcur)) {

			btr_pcur_move_to_next_user_rec(&pcur, mtr);
			rec_idx++;
		}

		/* if the level has finished before the record we are
		searching for, this means that the B-tree has changed in
		the meantime, quit our sampling and use whatever stats
		we have collected so far */
		if (rec_idx < dive_below_idx) {

			ut_ad(!btr_pcur_is_on_user_rec(&pcur));
			break;
		}

		/* it could be that the tree has changed in such a way that
		the record under dive_below_idx is the supremum record, in
		this case rec_idx == dive_below_idx and pcur is positioned
		on the supremum, we do not want to dive below it */
		if (!btr_pcur_is_on_user_rec(&pcur)) {
			break;
		}

		ut_a(rec_idx == dive_below_idx);

		ib_uint64_t	n_diff_on_leaf_page;
		ib_uint64_t	n_external_pages;

		dict_stats_analyze_index_below_cur(btr_pcur_get_btr_cur(&pcur),
						   n_prefix,
						   &n_diff_on_leaf_page,
						   &n_external_pages);

		/* We adjust n_diff_on_leaf_page here to avoid counting
		one value twice - once as the last on some page and once
		as the first on another page. Consider the following example:
		Leaf level:
		page: (2,2,2,2,3,3)
		... many pages like (3,3,3,3,3,3) ...
		page: (3,3,3,3,5,5)
		... many pages like (5,5,5,5,5,5) ...
		page: (5,5,5,5,8,8)
		page: (8,8,8,8,9,9)
		our algo would (correctly) get an estimate that there are
		2 distinct records per page (average). Having 4 pages below
		non-boring records, it would (wrongly) estimate the number
		of distinct records to 8. */
		if (n_diff_on_leaf_page > 0) {
			n_diff_on_leaf_page--;
		}

		n_diff_data->n_diff_all_analyzed_pages += n_diff_on_leaf_page;

		n_diff_data->n_external_pages_sum += n_external_pages;
	}

	btr_pcur_close(&pcur);
}

/** Set dict_index_t::stat_n_diff_key_vals[] and stat_n_sample_sizes[].
@param[in]	n_diff_data	input data to use to derive the results
@param[in,out]	index		index whose stat_n_diff_key_vals[] to set */
UNIV_INLINE
void
dict_stats_index_set_n_diff(
	const n_diff_data_t*	n_diff_data,
	dict_index_t*		index)
{
	for (ulint n_prefix = dict_index_get_n_unique(index);
	     n_prefix >= 1;
	     n_prefix--) {
		/* n_diff_all_analyzed_pages can be 0 here if
		all the leaf pages sampled contained only
		delete-marked records. In this case we should assign
		0 to index->stat_n_diff_key_vals[n_prefix - 1], which
		the formula below does. */

		const n_diff_data_t*	data = &n_diff_data[n_prefix - 1];

		ut_ad(data->n_leaf_pages_to_analyze > 0);
		ut_ad(data->n_recs_on_level > 0);

		ib_uint64_t	n_ordinary_leaf_pages;

		if (data->level == 1) {
			/* If we know the number of records on level 1, then
			this number is the same as the number of pages on
			level 0 (leaf). */
			n_ordinary_leaf_pages = data->n_recs_on_level;
		} else {
			/* If we analyzed D ordinary leaf pages and found E
			external pages in total linked from those D ordinary
			leaf pages, then this means that the ratio
			ordinary/external is D/E. Then the ratio ordinary/total
			is D / (D + E). Knowing that the total number of pages
			is T (including ordinary and external) then we estimate
			that the total number of ordinary leaf pages is
			T * D / (D + E). */
			n_ordinary_leaf_pages
				= index->stat_n_leaf_pages
				* data->n_leaf_pages_to_analyze
				/ (data->n_leaf_pages_to_analyze
				   + data->n_external_pages_sum);
		}

		/* See REF01 for an explanation of the algorithm */
		index->stat_n_diff_key_vals[n_prefix - 1]
			= n_ordinary_leaf_pages

			* data->n_diff_on_level
			/ data->n_recs_on_level

			* data->n_diff_all_analyzed_pages
			/ data->n_leaf_pages_to_analyze;

		index->stat_n_sample_sizes[n_prefix - 1]
			= data->n_leaf_pages_to_analyze;

		DEBUG_PRINTF("    %s(): n_diff=" UINT64PF " for n_prefix=%lu"
			     " (%lu"
			     " * " UINT64PF " / " UINT64PF
			     " * " UINT64PF " / " UINT64PF ")\n",
			     __func__,
			     index->stat_n_diff_key_vals[n_prefix - 1],
			     n_prefix,
			     index->stat_n_leaf_pages,
			     data->n_diff_on_level,
			     data->n_recs_on_level,
			     data->n_diff_all_analyzed_pages,
			     data->n_leaf_pages_to_analyze);
	}
}

/*********************************************************************//**
Calculates new statistics for a given index and saves them to the index
members stat_n_diff_key_vals[], stat_n_sample_sizes[], stat_index_size and
stat_n_leaf_pages. This function could be slow. */
static
void
dict_stats_analyze_index(
/*=====================*/
	dict_index_t*	index)	/*!< in/out: index to analyze */
{
	ulint		root_level;
	ulint		level;
	bool		level_is_analyzed;
	ulint		n_uniq;
	ulint		n_prefix;
	ib_uint64_t	total_recs;
	ib_uint64_t	total_pages;
	mtr_t		mtr;
	ulint		size;
	DBUG_ENTER("dict_stats_analyze_index");

	DBUG_PRINT("info", ("index: %s, online status: %d", index->name(),
			    dict_index_get_online_status(index)));

	/* Disable update statistic for Rtree */
	if (dict_index_is_spatial(index)) {
		DBUG_VOID_RETURN;
	}

	DEBUG_PRINTF("  %s(index=%s)\n", __func__, index->name());

	dict_stats_empty_index(index);

	mtr_start(&mtr);

	mtr_s_lock(dict_index_get_lock(index), &mtr);

	size = btr_get_size(index, BTR_TOTAL_SIZE, &mtr);

	if (size != ULINT_UNDEFINED) {
		index->stat_index_size = size;
		size = btr_get_size(index, BTR_N_LEAF_PAGES, &mtr);
	}

	/* Release the X locks on the root page taken by btr_get_size() */
	mtr_commit(&mtr);

	switch (size) {
	case ULINT_UNDEFINED:
		dict_stats_assert_initialized_index(index);
		DBUG_VOID_RETURN;
	case 0:
		/* The root node of the tree is a leaf */
		size = 1;
	}

	index->stat_n_leaf_pages = size;

	mtr_start(&mtr);

	mtr_sx_lock(dict_index_get_lock(index), &mtr);

	root_level = btr_height_get(index, &mtr);

	n_uniq = dict_index_get_n_unique(index);

	/* If the tree has just one level (and one page) or if the user
	has requested to sample too many pages then do full scan.

	For each n-column prefix (for n=1..n_uniq) N_SAMPLE_PAGES(index)
	will be sampled, so in total N_SAMPLE_PAGES(index) * n_uniq leaf
	pages will be sampled. If that number is bigger than the total
	number of leaf pages then do full scan of the leaf level instead
	since it will be faster and will give better results. */

	if (root_level == 0
	    || N_SAMPLE_PAGES(index) * n_uniq > index->stat_n_leaf_pages) {

		if (root_level == 0) {
			DEBUG_PRINTF("  %s(): just one page,"
				     " doing full scan\n", __func__);
		} else {
			DEBUG_PRINTF("  %s(): too many pages requested for"
				     " sampling, doing full scan\n", __func__);
		}

		/* do full scan of level 0; save results directly
		into the index */

		dict_stats_analyze_index_level(index,
					       0 /* leaf level */,
					       index->stat_n_diff_key_vals,
					       &total_recs,
					       &total_pages,
					       NULL /* boundaries not needed */,
					       &mtr);

		for (ulint i = 0; i < n_uniq; i++) {
			index->stat_n_sample_sizes[i] = total_pages;
		}

		mtr_commit(&mtr);

		dict_stats_assert_initialized_index(index);
		DBUG_VOID_RETURN;
	}

	/* For each level that is being scanned in the btree, this contains the
	number of different key values for all possible n-column prefixes. */
	ib_uint64_t*	n_diff_on_level = UT_NEW_ARRAY(
		ib_uint64_t, n_uniq, mem_key_dict_stats_n_diff_on_level);

	/* For each level that is being scanned in the btree, this contains the
	index of the last record from each group of equal records (when
	comparing only the first n columns, n=1..n_uniq). */
	boundaries_t*	n_diff_boundaries = UT_NEW_ARRAY_NOKEY(boundaries_t,
							       n_uniq);

	/* For each n-column prefix this array contains the input data that is
	used to calculate dict_index_t::stat_n_diff_key_vals[]. */
	n_diff_data_t*	n_diff_data = UT_NEW_ARRAY_NOKEY(n_diff_data_t, n_uniq);

	/* total_recs is also used to estimate the number of pages on one
	level below, so at the start we have 1 page (the root) */
	total_recs = 1;

	/* Here we use the following optimization:
	If we find that level L is the first one (searching from the
	root) that contains at least D distinct keys when looking at
	the first n_prefix columns, then:
	if we look at the first n_prefix-1 columns then the first
	level that contains D distinct keys will be either L or a
	lower one.
	So if we find that the first level containing D distinct
	keys (on n_prefix columns) is L, we continue from L when
	searching for D distinct keys on n_prefix-1 columns. */
	level = root_level;
	level_is_analyzed = false;

	for (n_prefix = n_uniq; n_prefix >= 1; n_prefix--) {

		DEBUG_PRINTF("  %s(): searching level with >=%llu"
			     " distinct records, n_prefix=%lu\n",
			     __func__, N_DIFF_REQUIRED(index), n_prefix);

		/* Commit the mtr to release the tree S lock to allow
		other threads to do some work too. */
		mtr_commit(&mtr);
		mtr_start(&mtr);
		mtr_sx_lock(dict_index_get_lock(index), &mtr);
		if (root_level != btr_height_get(index, &mtr)) {
			/* Just quit if the tree has changed beyond
			recognition here. The old stats from previous
			runs will remain in the values that we have
			not calculated yet. Initially when the index
			object is created the stats members are given
			some sensible values so leaving them untouched
			here even the first time will not cause us to
			read uninitialized memory later. */
			break;
		}

		/* check whether we should pick the current level;
		we pick level 1 even if it does not have enough
		distinct records because we do not want to scan the
		leaf level because it may contain too many records */
		if (level_is_analyzed
		    && (n_diff_on_level[n_prefix - 1] >= N_DIFF_REQUIRED(index)
			|| level == 1)) {

			goto found_level;
		}

		/* search for a level that contains enough distinct records */

		if (level_is_analyzed && level > 1) {

			/* if this does not hold we should be on
			"found_level" instead of here */
			ut_ad(n_diff_on_level[n_prefix - 1]
			      < N_DIFF_REQUIRED(index));

			level--;
			level_is_analyzed = false;
		}

		/* descend into the tree, searching for "good enough" level */
		for (;;) {

			/* make sure we do not scan the leaf level
			accidentally, it may contain too many pages */
			ut_ad(level > 0);

			/* scanning the same level twice is an optimization
			bug */
			ut_ad(!level_is_analyzed);

			/* Do not scan if this would read too many pages.
			Here we use the following fact:
			the number of pages on level L equals the number
			of records on level L+1, thus we deduce that the
			following call would scan total_recs pages, because
			total_recs is left from the previous iteration when
			we scanned one level upper or we have not scanned any
			levels yet in which case total_recs is 1. */
			if (total_recs > N_SAMPLE_PAGES(index)) {

				/* if the above cond is true then we are
				not at the root level since on the root
				level total_recs == 1 (set before we
				enter the n-prefix loop) and cannot
				be > N_SAMPLE_PAGES(index) */
				ut_a(level != root_level);

				/* step one level back and be satisfied with
				whatever it contains */
				level++;
				level_is_analyzed = true;

				break;
			}

			dict_stats_analyze_index_level(index,
						       level,
						       n_diff_on_level,
						       &total_recs,
						       &total_pages,
						       n_diff_boundaries,
						       &mtr);

			level_is_analyzed = true;

			if (level == 1
			    || n_diff_on_level[n_prefix - 1]
			    >= N_DIFF_REQUIRED(index)) {
				/* we have reached the last level we could scan
				or we found a good level with many distinct
				records */
				break;
			}

			level--;
			level_is_analyzed = false;
		}
found_level:

		DEBUG_PRINTF("  %s(): found level %lu that has " UINT64PF
			     " distinct records for n_prefix=%lu\n",
			     __func__, level, n_diff_on_level[n_prefix - 1],
			     n_prefix);
		/* here we are either on level 1 or the level that we are on
		contains >= N_DIFF_REQUIRED distinct keys or we did not scan
		deeper levels because they would contain too many pages */

		ut_ad(level > 0);

		ut_ad(level_is_analyzed);

		/* if any of these is 0 then there is exactly one page in the
		B-tree and it is empty and we should have done full scan and
		should not be here */
		ut_ad(total_recs > 0);
		ut_ad(n_diff_on_level[n_prefix - 1] > 0);

		ut_ad(N_SAMPLE_PAGES(index) > 0);

		n_diff_data_t*	data = &n_diff_data[n_prefix - 1];

		data->level = level;

		data->n_recs_on_level = total_recs;

		data->n_diff_on_level = n_diff_on_level[n_prefix - 1];

		data->n_leaf_pages_to_analyze = std::min(
			N_SAMPLE_PAGES(index),
			n_diff_on_level[n_prefix - 1]);

		/* pick some records from this level and dive below them for
		the given n_prefix */

		dict_stats_analyze_index_for_n_prefix(
			index, n_prefix, &n_diff_boundaries[n_prefix - 1],
			data, &mtr);
	}

	mtr_commit(&mtr);

	UT_DELETE_ARRAY(n_diff_boundaries);

	UT_DELETE_ARRAY(n_diff_on_level);

	/* n_prefix == 0 means that the above loop did not end up prematurely
	due to tree being changed and so n_diff_data[] is set up. */
	if (n_prefix == 0) {
		dict_stats_index_set_n_diff(n_diff_data, index);
	}

	UT_DELETE_ARRAY(n_diff_data);

	dict_stats_assert_initialized_index(index);
	DBUG_VOID_RETURN;
}

/*********************************************************************//**
Calculates new estimates for table and index statistics. This function
is relatively slow and is used to calculate persistent statistics that
will be saved on disk.
@return DB_SUCCESS or error code */
static
dberr_t
dict_stats_update_persistent(
/*=========================*/
	dict_table_t*	table)		/*!< in/out: table */
{
	dict_index_t*	index;

	DEBUG_PRINTF("%s(table=%s)\n", __func__, table->name);

	dict_table_stats_lock(table, RW_X_LATCH);

	/* analyze the clustered index first */

	index = table->first_index();

	if (index == NULL
	    || index->is_corrupted()
	    || (index->type | DICT_UNIQUE) != (DICT_CLUSTERED | DICT_UNIQUE)) {

		/* Table definition is corrupt */
		dict_table_stats_unlock(table, RW_X_LATCH);
		dict_stats_empty_table(table);

		return(DB_CORRUPTION);
	}

	ut_ad(!dict_index_is_ibuf(index));

	dict_stats_analyze_index(index);

	ulint	n_unique = dict_index_get_n_unique(index);

	table->stat_n_rows = index->stat_n_diff_key_vals[n_unique - 1];

	table->stat_clustered_index_size = index->stat_index_size;

	/* analyze other indexes from the table, if any */

	table->stat_sum_of_other_index_sizes = 0;

	for (index = index->next(); index != NULL; index = index->next()) {

		ut_ad(!dict_index_is_ibuf(index));

		if (index->type & DICT_FTS || dict_index_is_spatial(index)) {
			continue;
		}

		dict_stats_empty_index(index);

		if (dict_stats_should_ignore_index(index)) {
			continue;
		}

		if (!(table->stats_bg_flag & BG_STAT_SHOULD_QUIT)) {
			dict_stats_analyze_index(index);
		}

		table->stat_sum_of_other_index_sizes
			+= index->stat_index_size;
	}

	table->stats_last_recalc = ut_time();

	table->stat_modified_counter = 0;

	table->stat_initialized = TRUE;

	dict_stats_assert_initialized(table);

	dict_table_stats_unlock(table, RW_X_LATCH);

	return(DB_SUCCESS);
}

#include "mysql_com.h"

/** Save an individual index's statistic into the persistent statistics
storage.
@param[in]	index			index to be updated
@param[in]	last_update		timestamp of the stat
@param[in]	stat_name		name of the stat
@param[in]	stat_value		value of the stat
@param[in]	sample_size		n pages sampled or NULL
@param[in]	stat_description	description of the stat
@param[in,out]	trx			in case of NULL the function will
allocate and free the trx object. If it is not NULL then it will be
rolled back only in the case of error, but not freed.
@return DB_SUCCESS or error code */
static
dberr_t
dict_stats_save_index_stat(
	dict_index_t*	index,
	lint		last_update,
	const char*	stat_name,
	ib_uint64_t	stat_value,
	ib_uint64_t*	sample_size,
	const char*	stat_description,
	trx_t*		trx)
{
	dberr_t		ret;
	pars_info_t*	pinfo;
	char		db_utf8[MAX_DB_UTF8_LEN];
	char		table_utf8[MAX_TABLE_UTF8_LEN];

	ut_ad(rw_lock_own(dict_operation_lock, RW_LOCK_X));

	dict_fs2utf8(index->table->name.m_name, db_utf8, sizeof(db_utf8),
		     table_utf8, sizeof(table_utf8));

	pinfo = pars_info_create();
	pars_info_add_str_literal(pinfo, "database_name", db_utf8);
	pars_info_add_str_literal(pinfo, "table_name", table_utf8);
	pars_info_add_str_literal(pinfo, "index_name", index->name);
	UNIV_MEM_ASSERT_RW_ABORT(&last_update, 4);
	pars_info_add_int4_literal(pinfo, "last_update", last_update);
	UNIV_MEM_ASSERT_RW_ABORT(stat_name, strlen(stat_name));
	pars_info_add_str_literal(pinfo, "stat_name", stat_name);
	UNIV_MEM_ASSERT_RW_ABORT(&stat_value, 8);
	pars_info_add_ull_literal(pinfo, "stat_value", stat_value);
	if (sample_size != NULL) {
		UNIV_MEM_ASSERT_RW_ABORT(sample_size, 8);
		pars_info_add_ull_literal(pinfo, "sample_size", *sample_size);
	} else {
		pars_info_add_literal(pinfo, "sample_size", NULL,
				      UNIV_SQL_NULL, DATA_FIXBINARY, 0);
	}
	UNIV_MEM_ASSERT_RW_ABORT(stat_description, strlen(stat_description));
	pars_info_add_str_literal(pinfo, "stat_description",
				  stat_description);

	ret = dict_stats_exec_sql(
		pinfo,
		"PROCEDURE INDEX_STATS_SAVE () IS\n"
		"BEGIN\n"

		"DELETE FROM \"" INDEX_STATS_NAME "\"\n"
		"WHERE\n"
		"database_name = :database_name AND\n"
		"table_name = :table_name AND\n"
		"index_name = :index_name AND\n"
		"stat_name = :stat_name;\n"

		"INSERT INTO \"" INDEX_STATS_NAME "\"\n"
		"VALUES\n"
		"(\n"
		":database_name,\n"
		":table_name,\n"
		":index_name,\n"
		":last_update,\n"
		":stat_name,\n"
		":stat_value,\n"
		":sample_size,\n"
		":stat_description\n"
		");\n"
		"END;", trx);

	if (ret != DB_SUCCESS) {
		ib::error() << "Cannot save index statistics for table "
			<< index->table->name
			<< ", index " << index->name
			<< ", stat name \"" << stat_name << "\": "
			<< ut_strerr(ret);
	}

	return(ret);
}

/** Save the table's statistics into the persistent statistics storage.
@param[in]	table_orig	table whose stats to save
@param[in]	only_for_index	if this is non-NULL, then stats for indexes
that are not equal to it will not be saved, if NULL, then all indexes' stats
are saved
@return DB_SUCCESS or error code */
static
dberr_t
dict_stats_save(
	dict_table_t*		table_orig,
	const index_id_t*	only_for_index)
{
	pars_info_t*	pinfo;
	lint		now;
	dberr_t		ret;
	dict_table_t*	table;
	char		db_utf8[MAX_DB_UTF8_LEN];
	char		table_utf8[MAX_TABLE_UTF8_LEN];

	table = dict_stats_snapshot_create(table_orig);

	dict_fs2utf8(table->name.m_name, db_utf8, sizeof(db_utf8),
		     table_utf8, sizeof(table_utf8));

	rw_lock_x_lock(dict_operation_lock);

	/* MySQL's timestamp is 4 byte, so we use
	pars_info_add_int4_literal() which takes a lint arg, so "now" is
	lint */
	now = (lint) ut_time();

	pinfo = pars_info_create();

	pars_info_add_str_literal(pinfo, "database_name", db_utf8);
	pars_info_add_str_literal(pinfo, "table_name", table_utf8);
	pars_info_add_int4_literal(pinfo, "last_update", now);
	pars_info_add_ull_literal(pinfo, "n_rows", table->stat_n_rows);
	pars_info_add_ull_literal(pinfo, "clustered_index_size",
		table->stat_clustered_index_size);
	pars_info_add_ull_literal(pinfo, "sum_of_other_index_sizes",
		table->stat_sum_of_other_index_sizes);

	ret = dict_stats_exec_sql(
		pinfo,
		"PROCEDURE TABLE_STATS_SAVE () IS\n"
		"BEGIN\n"

		"DELETE FROM \"" TABLE_STATS_NAME "\"\n"
		"WHERE\n"
		"database_name = :database_name AND\n"
		"table_name = :table_name;\n"

		"INSERT INTO \"" TABLE_STATS_NAME "\"\n"
		"VALUES\n"
		"(\n"
		":database_name,\n"
		":table_name,\n"
		":last_update,\n"
		":n_rows,\n"
		":clustered_index_size,\n"
		":sum_of_other_index_sizes\n"
		");\n"
		"END;", NULL);

	if (ret != DB_SUCCESS) {
		ib::error() << "Cannot save table statistics for table "
			<< table->name << ": " << ut_strerr(ret);

		rw_lock_x_unlock(dict_operation_lock);

		dict_stats_snapshot_free(table);

		return(ret);
	}

	trx_t*	trx = trx_allocate_for_background();

	if (srv_read_only_mode) {
		trx_start_internal_read_only(trx);
	} else {
		trx_start_internal(trx);
	}

	dict_index_t*	index;
	index_map_t	indexes(
		(ut_strcmp_functor()),
		index_map_t_allocator(mem_key_dict_stats_index_map_t));

	/* Below we do all the modifications in innodb_index_stats in a single
	transaction for performance reasons. Modifying more than one row in a
	single transaction may deadlock with other transactions if they
	lock the rows in different order. Other transaction could be for
	example when we DROP a table and do
	DELETE FROM innodb_index_stats WHERE database_name = '...'
	AND table_name = '...'; which will affect more than one row. To
	prevent deadlocks we always lock the rows in the same order - the
	order of the PK, which is (database_name, table_name, index_name,
	stat_name). This is why below we sort the indexes by name and then
	for each index, do the mods ordered by stat_name. */

	for (index = table->first_index();
	     index != NULL;
	     index = index->next()) {

		indexes[index->name] = index;
	}

	index_map_t::const_iterator	it;

	for (it = indexes.begin(); it != indexes.end(); ++it) {

		index = it->second;

		if (only_for_index != NULL
		    && index->space != only_for_index->m_space_id
		    && index->id != only_for_index->m_index_id) {
			continue;
		}

		if (dict_stats_should_ignore_index(index)) {
			continue;
		}

		ut_ad(!dict_index_is_ibuf(index));

		for (ulint i = 0; i < index->n_uniq; i++) {

			char	stat_name[16];
			char	stat_description[1024];
			ulint	j;

			snprintf(stat_name, sizeof(stat_name),
				    "n_diff_pfx%02" ULINTPFS, i + 1);

			/* craft a string that contains the column names */
			snprintf(stat_description,
				    sizeof(stat_description),
				    "%s", index->fields[0].name());
			for (j = 1; j <= i; j++) {
				size_t	len;

				len = strlen(stat_description);

				snprintf(stat_description + len,
					    sizeof(stat_description) - len,
					    ",%s", index->fields[j].name());
			}

			ret = dict_stats_save_index_stat(
				index, now, stat_name,
				index->stat_n_diff_key_vals[i],
				&index->stat_n_sample_sizes[i],
				stat_description, trx);

			if (ret != DB_SUCCESS) {
				goto end;
			}
		}

		ret = dict_stats_save_index_stat(index, now, "n_leaf_pages",
						 index->stat_n_leaf_pages,
						 NULL,
						 "Number of leaf pages "
						 "in the index", trx);
		if (ret != DB_SUCCESS) {
			goto end;
		}

		ret = dict_stats_save_index_stat(index, now, "size",
						 index->stat_index_size,
						 NULL,
						 "Number of pages "
						 "in the index", trx);
		if (ret != DB_SUCCESS) {
			goto end;
		}
	}

	trx_commit_for_mysql(trx);

end:
	trx_free_for_background(trx);

	rw_lock_x_unlock(dict_operation_lock);

	dict_stats_snapshot_free(table);

	return(ret);
}

/*********************************************************************//**
Called for the row that is selected by
SELECT ... FROM mysql.innodb_table_stats WHERE table='...'
The second argument is a pointer to the table and the fetched stats are
written to it.
@return non-NULL dummy */
static
ibool
dict_stats_fetch_table_stats_step(
/*==============================*/
	void*	node_void,	/*!< in: select node */
	void*	table_void)	/*!< out: table */
{
	sel_node_t*	node = (sel_node_t*) node_void;
	dict_table_t*	table = (dict_table_t*) table_void;
	que_common_t*	cnode;
	int		i;

	/* this should loop exactly 3 times - for
	n_rows,clustered_index_size,sum_of_other_index_sizes */
	for (cnode = static_cast<que_common_t*>(node->select_list), i = 0;
	     cnode != NULL;
	     cnode = static_cast<que_common_t*>(que_node_get_next(cnode)),
	     i++) {

		const byte*	data;
		dfield_t*	dfield = que_node_get_val(cnode);
		dtype_t*	type = dfield_get_type(dfield);
		ulint		len = dfield_get_len(dfield);

		data = static_cast<const byte*>(dfield_get_data(dfield));

		switch (i) {
		case 0: /* mysql.innodb_table_stats.n_rows */

			ut_a(dtype_get_mtype(type) == DATA_INT);
			ut_a(len == 8);

			table->stat_n_rows = mach_read_from_8(data);

			break;

		case 1: /* mysql.innodb_table_stats.clustered_index_size */

			ut_a(dtype_get_mtype(type) == DATA_INT);
			ut_a(len == 8);

			table->stat_clustered_index_size
				= (ulint) mach_read_from_8(data);

			break;

		case 2: /* mysql.innodb_table_stats.sum_of_other_index_sizes */

			ut_a(dtype_get_mtype(type) == DATA_INT);
			ut_a(len == 8);

			table->stat_sum_of_other_index_sizes
				= (ulint) mach_read_from_8(data);

			break;

		default:

			/* someone changed SELECT
			n_rows,clustered_index_size,sum_of_other_index_sizes
			to select more columns from innodb_table_stats without
			adjusting here */
			ut_error;
		}
	}

	/* if i < 3 this means someone changed the
	SELECT n_rows,clustered_index_size,sum_of_other_index_sizes
	to select less columns from innodb_table_stats without adjusting here;
	if i > 3 we would have ut_error'ed earlier */
	ut_a(i == 3 /*n_rows,clustered_index_size,sum_of_other_index_sizes*/);

	/* XXX this is not used but returning non-NULL is necessary */
	return(TRUE);
}

/** Aux struct used to pass a table and a boolean to
dict_stats_fetch_index_stats_step(). */
struct index_fetch_t {
	dict_table_t*	table;	/*!< table whose indexes are to be modified */
	bool		stats_were_modified; /*!< will be set to true if at
				least one index stats were modified */
};

/*********************************************************************//**
Called for the rows that are selected by
SELECT ... FROM mysql.innodb_index_stats WHERE table='...'
The second argument is a pointer to the table and the fetched stats are
written to its indexes.
Let a table has N indexes and each index has Ui unique columns for i=1..N,
then mysql.innodb_index_stats will have SUM(Ui) i=1..N rows for that table.
So this function will be called SUM(Ui) times where SUM(Ui) is of magnitude
N*AVG(Ui). In each call it searches for the currently fetched index into
table->indexes linearly, assuming this list is not sorted. Thus, overall,
fetching all indexes' stats from mysql.innodb_index_stats is O(N^2) where N
is the number of indexes.
This can be improved if we sort table->indexes in a temporary area just once
and then search in that sorted list. Then the complexity will be O(N*log(N)).
We assume a table will not have more than 100 indexes, so we go with the
simpler N^2 algorithm.
@return non-NULL dummy */
static
ibool
dict_stats_fetch_index_stats_step(
/*==============================*/
	void*	node_void,	/*!< in: select node */
	void*	arg_void)	/*!< out: table + a flag that tells if we
				modified anything */
{
	sel_node_t*	node = (sel_node_t*) node_void;
	index_fetch_t*	arg = (index_fetch_t*) arg_void;
	dict_table_t*	table = arg->table;
	dict_index_t*	index = NULL;
	que_common_t*	cnode;
	const char*	stat_name = NULL;
	ulint		stat_name_len = ULINT_UNDEFINED;
	ib_uint64_t	stat_value = UINT64_UNDEFINED;
	ib_uint64_t	sample_size = UINT64_UNDEFINED;
	int		i;

	/* this should loop exactly 4 times - for the columns that
	were selected: index_name,stat_name,stat_value,sample_size */
	for (cnode = static_cast<que_common_t*>(node->select_list), i = 0;
	     cnode != NULL;
	     cnode = static_cast<que_common_t*>(que_node_get_next(cnode)),
	     i++) {

		const byte*	data;
		dfield_t*	dfield = que_node_get_val(cnode);
		dtype_t*	type = dfield_get_type(dfield);
		ulint		len = dfield_get_len(dfield);

		data = static_cast<const byte*>(dfield_get_data(dfield));

		switch (i) {
		case 0: /* mysql.innodb_index_stats.index_name */

			ut_a(dtype_get_mtype(type) == DATA_VARMYSQL);

			/* search for index in table's indexes whose name
			matches data; the fetched index name is in data,
			has no terminating '\0' and has length len */
			for (index = table->first_index();
			     index != NULL;
			     index = index->next()) {

				if (index->is_committed()
				    && strlen(index->name) == len
				    && memcmp(index->name, data, len) == 0) {
					/* the corresponding index was found */
					break;
				}
			}

			/* if index is NULL here this means that
			mysql.innodb_index_stats contains more rows than the
			number of indexes in the table; this is ok, we just
			return ignoring those extra rows; in other words
			dict_stats_fetch_index_stats_step() has been called
			for a row from index_stats with unknown index_name
			column */
			if (index == NULL) {

				return(TRUE);
			}

			break;

		case 1: /* mysql.innodb_index_stats.stat_name */

			ut_a(dtype_get_mtype(type) == DATA_VARMYSQL);

			ut_a(index != NULL);

			stat_name = (const char*) data;
			stat_name_len = len;

			break;

		case 2: /* mysql.innodb_index_stats.stat_value */

			ut_a(dtype_get_mtype(type) == DATA_INT);
			ut_a(len == 8);

			ut_a(index != NULL);
			ut_a(stat_name != NULL);
			ut_a(stat_name_len != ULINT_UNDEFINED);

			stat_value = mach_read_from_8(data);

			break;

		case 3: /* mysql.innodb_index_stats.sample_size */

			ut_a(dtype_get_mtype(type) == DATA_INT);
			ut_a(len == 8 || len == UNIV_SQL_NULL);

			ut_a(index != NULL);
			ut_a(stat_name != NULL);
			ut_a(stat_name_len != ULINT_UNDEFINED);
			ut_a(stat_value != UINT64_UNDEFINED);

			if (len == UNIV_SQL_NULL) {
				break;
			}
			/* else */

			sample_size = mach_read_from_8(data);

			break;

		default:

			/* someone changed
			SELECT index_name,stat_name,stat_value,sample_size
			to select more columns from innodb_index_stats without
			adjusting here */
			ut_error;
		}
	}

	/* if i < 4 this means someone changed the
	SELECT index_name,stat_name,stat_value,sample_size
	to select less columns from innodb_index_stats without adjusting here;
	if i > 4 we would have ut_error'ed earlier */
	ut_a(i == 4 /* index_name,stat_name,stat_value,sample_size */);

	ut_a(index != NULL);
	ut_a(stat_name != NULL);
	ut_a(stat_name_len != ULINT_UNDEFINED);
	ut_a(stat_value != UINT64_UNDEFINED);
	/* sample_size could be UINT64_UNDEFINED here, if it is NULL */

#define PFX	"n_diff_pfx"
#define PFX_LEN	10

	if (stat_name_len == 4 /* strlen("size") */
	    && native_strncasecmp("size", stat_name, stat_name_len) == 0) {
		index->stat_index_size = (ulint) stat_value;
		arg->stats_were_modified = true;
	} else if (stat_name_len == 12 /* strlen("n_leaf_pages") */
		   && native_strncasecmp("n_leaf_pages", stat_name, stat_name_len)
		   == 0) {
		index->stat_n_leaf_pages = (ulint) stat_value;
		arg->stats_were_modified = true;
	} else if (stat_name_len > PFX_LEN /* e.g. stat_name=="n_diff_pfx01" */
		   && native_strncasecmp(PFX, stat_name, PFX_LEN) == 0) {

		const char*	num_ptr;
		unsigned long	n_pfx;

		/* point num_ptr into "1" from "n_diff_pfx12..." */
		num_ptr = stat_name + PFX_LEN;

		/* stat_name should have exactly 2 chars appended to PFX
		and they should be digits */
		if (stat_name_len != PFX_LEN + 2
		    || num_ptr[0] < '0' || num_ptr[0] > '9'
		    || num_ptr[1] < '0' || num_ptr[1] > '9') {

			char	db_utf8[MAX_DB_UTF8_LEN];
			char	table_utf8[MAX_TABLE_UTF8_LEN];

			dict_fs2utf8(table->name.m_name,
				     db_utf8, sizeof(db_utf8),
				     table_utf8, sizeof(table_utf8));

			ib::info	out;
			out << "Ignoring strange row from "
				<< INDEX_STATS_NAME_PRINT << " WHERE"
				" database_name = '" << db_utf8
				<< "' AND table_name = '" << table_utf8
				<< "' AND index_name = '" << index->name()
				<< "' AND stat_name = '";
			out.write(stat_name, stat_name_len);
			out << "'; because stat_name is malformed";
			return(TRUE);
		}
		/* else */

		/* extract 12 from "n_diff_pfx12..." into n_pfx
		note that stat_name does not have a terminating '\0' */
		n_pfx = (num_ptr[0] - '0') * 10 + (num_ptr[1] - '0');

		ulint	n_uniq = index->n_uniq;

		if (n_pfx == 0 || n_pfx > n_uniq) {

			char	db_utf8[MAX_DB_UTF8_LEN];
			char	table_utf8[MAX_TABLE_UTF8_LEN];

			dict_fs2utf8(table->name.m_name,
				     db_utf8, sizeof(db_utf8),
				     table_utf8, sizeof(table_utf8));

			ib::info	out;
			out << "Ignoring strange row from "
				<< INDEX_STATS_NAME_PRINT << " WHERE"
				" database_name = '" << db_utf8
				<< "' AND table_name = '" << table_utf8
				<< "' AND index_name = '" << index->name()
				<< "' AND stat_name = '";
			out.write(stat_name, stat_name_len);
			out << "'; because stat_name is out of range, the index"
				" has " << n_uniq << " unique columns";

			return(TRUE);
		}
		/* else */

		index->stat_n_diff_key_vals[n_pfx - 1] = stat_value;

		if (sample_size != UINT64_UNDEFINED) {
			index->stat_n_sample_sizes[n_pfx - 1] = sample_size;
		} else {
			/* hmm, strange... the user must have UPDATEd the
			table manually and SET sample_size = NULL */
			index->stat_n_sample_sizes[n_pfx - 1] = 0;
		}

		index->stat_n_non_null_key_vals[n_pfx - 1] = 0;

		arg->stats_were_modified = true;
	} else {
		/* silently ignore rows with unknown stat_name, the
		user may have developed her own stats */
	}

	/* XXX this is not used but returning non-NULL is necessary */
	return(TRUE);
}

/*********************************************************************//**
Read table's statistics from the persistent statistics storage.
@return DB_SUCCESS or error code */
static
dberr_t
dict_stats_fetch_from_ps(
/*=====================*/
	dict_table_t*	table)	/*!< in/out: table */
{
	index_fetch_t	index_fetch_arg;
	trx_t*		trx;
	pars_info_t*	pinfo;
	dberr_t		ret;
	char		db_utf8[MAX_DB_UTF8_LEN];
	char		table_utf8[MAX_TABLE_UTF8_LEN];

	ut_ad(!mutex_own(&dict_sys->mutex));

	/* Initialize all stats to dummy values before fetching because if
	the persistent storage contains incomplete stats (e.g. missing stats
	for some index) then we would end up with (partially) uninitialized
	stats. */
	dict_stats_empty_table(table);

	trx = trx_allocate_for_background();

	/* Use 'read-uncommitted' so that the SELECTs we execute
	do not get blocked in case some user has locked the rows we
	are SELECTing */

	trx->isolation_level = TRX_ISO_READ_UNCOMMITTED;

	if (srv_read_only_mode) {
		trx_start_internal_read_only(trx);
	} else {
		trx_start_internal(trx);
	}

	dict_fs2utf8(table->name.m_name, db_utf8, sizeof(db_utf8),
		     table_utf8, sizeof(table_utf8));

	pinfo = pars_info_create();

	pars_info_add_str_literal(pinfo, "database_name", db_utf8);

	pars_info_add_str_literal(pinfo, "table_name", table_utf8);

	pars_info_bind_function(pinfo,
			       "fetch_table_stats_step",
			       dict_stats_fetch_table_stats_step,
			       table);

	index_fetch_arg.table = table;
	index_fetch_arg.stats_were_modified = false;
	pars_info_bind_function(pinfo,
				"fetch_index_stats_step",
				dict_stats_fetch_index_stats_step,
				&index_fetch_arg);

	ret = que_eval_sql(pinfo,
			   "PROCEDURE FETCH_STATS () IS\n"
			   "found INT;\n"
			   "DECLARE FUNCTION fetch_table_stats_step;\n"
			   "DECLARE FUNCTION fetch_index_stats_step;\n"
			   "DECLARE CURSOR table_stats_cur IS\n"
			   "  SELECT\n"
			   /* if you change the selected fields, be
			   sure to adjust
			   dict_stats_fetch_table_stats_step() */
			   "  n_rows,\n"
			   "  clustered_index_size,\n"
			   "  sum_of_other_index_sizes\n"
			   "  FROM \"" TABLE_STATS_NAME "\"\n"
			   "  WHERE\n"
			   "  database_name = :database_name AND\n"
			   "  table_name = :table_name;\n"
			   "DECLARE CURSOR index_stats_cur IS\n"
			   "  SELECT\n"
			   /* if you change the selected fields, be
			   sure to adjust
			   dict_stats_fetch_index_stats_step() */
			   "  index_name,\n"
			   "  stat_name,\n"
			   "  stat_value,\n"
			   "  sample_size\n"
			   "  FROM \"" INDEX_STATS_NAME "\"\n"
			   "  WHERE\n"
			   "  database_name = :database_name AND\n"
			   "  table_name = :table_name;\n"

			   "BEGIN\n"

			   "OPEN table_stats_cur;\n"
			   "FETCH table_stats_cur INTO\n"
			   "  fetch_table_stats_step();\n"
			   "IF (SQL % NOTFOUND) THEN\n"
			   "  CLOSE table_stats_cur;\n"
			   "  RETURN;\n"
			   "END IF;\n"
			   "CLOSE table_stats_cur;\n"

			   "OPEN index_stats_cur;\n"
			   "found := 1;\n"
			   "WHILE found = 1 LOOP\n"
			   "  FETCH index_stats_cur INTO\n"
			   "    fetch_index_stats_step();\n"
			   "  IF (SQL % NOTFOUND) THEN\n"
			   "    found := 0;\n"
			   "  END IF;\n"
			   "END LOOP;\n"
			   "CLOSE index_stats_cur;\n"

			   "END;",
			   TRUE, trx);
	/* pinfo is freed by que_eval_sql() */

	trx_commit_for_mysql(trx);

	trx_free_for_background(trx);

	if (!index_fetch_arg.stats_were_modified) {
		return(DB_STATS_DO_NOT_EXIST);
	}

	return(ret);
}

/*********************************************************************//**
Fetches or calculates new estimates for index statistics. */
void
dict_stats_update_for_index(
/*========================*/
	dict_index_t*	index)	/*!< in/out: index */
{
	DBUG_ENTER("dict_stats_update_for_index");

	ut_ad(!mutex_own(&dict_sys->mutex));

	if (dict_stats_is_persistent_enabled(index->table)) {
		dict_table_stats_lock(index->table, RW_X_LATCH);
		dict_stats_analyze_index(index);
		dict_table_stats_unlock(index->table, RW_X_LATCH);
		index_id_t	index_id(index->space, index->id);
		dict_stats_save(index->table, &index_id);
		DBUG_VOID_RETURN;
	}

	dict_table_stats_lock(index->table, RW_X_LATCH);
	dict_stats_update_transient_for_index(index);
	dict_table_stats_unlock(index->table, RW_X_LATCH);

	DBUG_VOID_RETURN;
}

/*********************************************************************//**
Calculates new estimates for table and index statistics. The statistics
are used in query optimization.
@return DB_SUCCESS or error code */
dberr_t
dict_stats_update(
/*==============*/
	dict_table_t*		table,	/*!< in/out: table */
	dict_stats_upd_option_t	stats_upd_option)
					/*!< in: whether to (re) calc
					the stats or to fetch them from
					the persistent statistics
					storage */
{
	ut_ad(!mutex_own(&dict_sys->mutex));

	if (table->ibd_file_missing) {

		ib::warn() << "Cannot calculate statistics for table "
			<< table->name
			<< " because the .ibd file is missing. "
			<< TROUBLESHOOTING_MSG;

		dict_stats_empty_table(table);
		return(DB_TABLESPACE_DELETED);
	} else if (srv_force_recovery >= SRV_FORCE_NO_IBUF_MERGE) {
		/* If we have set a high innodb_force_recovery level, do
		not calculate statistics, as a badly corrupted index can
		cause a crash in it. */
		dict_stats_empty_table(table);
		return(DB_SUCCESS);
	}

	switch (stats_upd_option) {
		dberr_t	err;

	case DICT_STATS_RECALC_PERSISTENT:

		if (srv_read_only_mode) {
			break;
		}

		/* Persistent recalculation requested, called from
		1) ANALYZE TABLE, or
		2) the auto recalculation background thread, or
		3) open table if stats do not exist on disk and auto recalc
		   is enabled */

		/* InnoDB internal tables (e.g. SYS_TABLES) cannot have
		persistent stats enabled */
		ut_a(strchr(table->name.m_name, '/') != NULL);

		err = dict_stats_update_persistent(table);

		if (err != DB_SUCCESS) {
			return(err);
		}

		return(dict_stats_save(table, NULL));

	case DICT_STATS_RECALC_TRANSIENT:
		break;

	case DICT_STATS_EMPTY_TABLE:

		dict_stats_empty_table(table);

		/* If table is using persistent stats,
		then save the stats on disk */

		if (dict_stats_is_persistent_enabled(table)) {
			return(dict_stats_save(table, NULL));
		}

		return(DB_SUCCESS);

	case DICT_STATS_FETCH_ONLY_IF_NOT_IN_MEMORY:

		/* fetch requested, either fetch from persistent statistics
		storage or use the old method */

		if (table->stat_initialized) {
			return(DB_SUCCESS);
		}

		/* InnoDB internal tables (e.g. SYS_TABLES) cannot have
		persistent stats enabled */
		ut_a(strchr(table->name.m_name, '/') != NULL);

		/* Create a dummy table object with the same name and
		indexes, suitable for fetching the stats into it. */
		dict_table_t*	t = dict_stats_table_clone_create(table);

		err = dict_stats_fetch_from_ps(t);

		t->stats_last_recalc = table->stats_last_recalc;
		t->stat_modified_counter = 0;

		switch (err) {
		case DB_SUCCESS:

			dict_table_stats_lock(table, RW_X_LATCH);

			dict_stats_copy(table, t);

			dict_stats_assert_initialized(table);

			dict_table_stats_unlock(table, RW_X_LATCH);

			dict_stats_table_clone_free(t);

			return(DB_SUCCESS);
		case DB_STATS_DO_NOT_EXIST:

			dict_stats_table_clone_free(t);

			if (srv_read_only_mode) {
				break;
			}

			if (dict_stats_auto_recalc_is_enabled(table)) {
				return(dict_stats_update(
						table,
						DICT_STATS_RECALC_PERSISTENT));
			}

			ib::info() << "Trying to use table " << table->name
				<< " which has persistent statistics enabled,"
				" but auto recalculation turned off and the"
				" statistics do not exist in "
				TABLE_STATS_NAME_PRINT
				" and " INDEX_STATS_NAME_PRINT
				". Please either run \"ANALYZE TABLE "
				<< table->name << ";\" manually or enable the"
				" auto recalculation with \"ALTER TABLE "
				<< table->name << " STATS_AUTO_RECALC=1;\"."
				" InnoDB will now use transient statistics for "
				<< table->name << ".";

			break;
		default:

			dict_stats_table_clone_free(t);

			ib::error() << "Error fetching persistent statistics"
				" for table "
				<< table->name
				<< " from " TABLE_STATS_NAME_PRINT " and "
				INDEX_STATS_NAME_PRINT ": " << ut_strerr(err)
				<< ". Using transient stats method instead.";

			break;
		}
	/* no "default:" in order to produce a compilation warning
	about unhandled enumeration value */
	}

	dict_table_stats_lock(table, RW_X_LATCH);

	dict_stats_update_transient(table);

	dict_table_stats_unlock(table, RW_X_LATCH);

	return(DB_SUCCESS);
}

/*********************************************************************//**
Removes the information for a particular index's stats from the persistent
storage if it exists and if there is data stored for this index.
This function creates its own trx and commits it.
A note from Marko why we cannot edit user and sys_* tables in one trx:
marko: The problem is that ibuf merges should be disabled while we are
rolling back dict transactions.
marko: If ibuf merges are not disabled, we need to scan the *.ibd files.
But we shouldn't open *.ibd files before we have rolled back dict
transactions and opened the SYS_* records for the *.ibd files.
@return DB_SUCCESS or error code */
dberr_t
dict_stats_drop_index(
/*==================*/
	const char*	db_and_table,/*!< in: db and table, e.g. 'db/table' */
	const char*	iname,	/*!< in: index name */
	char*		errstr, /*!< out: error message if != DB_SUCCESS
				is returned */
	ulint		errstr_sz)/*!< in: size of the errstr buffer */
{
	char		db_utf8[MAX_DB_UTF8_LEN];
	char		table_utf8[MAX_TABLE_UTF8_LEN];
	pars_info_t*	pinfo;
	dberr_t		ret;

	ut_ad(!mutex_own(&dict_sys->mutex));

	/* skip indexes whose table names do not contain a database name
	e.g. if we are dropping an index from SYS_TABLES */
	if (strchr(db_and_table, '/') == NULL) {

		return(DB_SUCCESS);
	}

	dict_fs2utf8(db_and_table, db_utf8, sizeof(db_utf8),
		     table_utf8, sizeof(table_utf8));

	pinfo = pars_info_create();

	pars_info_add_str_literal(pinfo, "database_name", db_utf8);

	pars_info_add_str_literal(pinfo, "table_name", table_utf8);

	pars_info_add_str_literal(pinfo, "index_name", iname);

	rw_lock_x_lock(dict_operation_lock);

	ret = dict_stats_exec_sql(
		pinfo,
		"PROCEDURE DROP_INDEX_STATS () IS\n"
		"BEGIN\n"
		"DELETE FROM \"" INDEX_STATS_NAME "\" WHERE\n"
		"database_name = :database_name AND\n"
		"table_name = :table_name AND\n"
		"index_name = :index_name;\n"
		"END;\n", NULL);

	rw_lock_x_unlock(dict_operation_lock);

	if (ret == DB_STATS_DO_NOT_EXIST) {
		ret = DB_SUCCESS;
	}

	if (ret != DB_SUCCESS) {
		snprintf(errstr, errstr_sz,
			    "Unable to delete statistics for index %s"
			    " from %s%s: %s. They can be deleted later using"
			    " DELETE FROM %s WHERE"
			    " database_name = '%s' AND"
			    " table_name = '%s' AND"
			    " index_name = '%s';",
			    iname,
			    INDEX_STATS_NAME_PRINT,
			    (ret == DB_LOCK_WAIT_TIMEOUT
			     ? " because the rows are locked"
			     : ""),
			    ut_strerr(ret),
			    INDEX_STATS_NAME_PRINT,
			    db_utf8,
			    table_utf8,
			    iname);

		ut_print_timestamp(stderr);
		fprintf(stderr, " InnoDB: %s\n", errstr);
	}

	return(ret);
}

/*********************************************************************//**
Executes
DELETE FROM mysql.innodb_table_stats
WHERE database_name = '...' AND table_name = '...';
Creates its own transaction and commits it.
@return DB_SUCCESS or error code */
UNIV_INLINE
dberr_t
dict_stats_delete_from_table_stats(
/*===============================*/
	const char*	database_name,	/*!< in: database name, e.g. 'db' */
	const char*	table_name)	/*!< in: table name, e.g. 'table' */
{
	pars_info_t*	pinfo;
	dberr_t		ret;

	ut_ad(rw_lock_own(dict_operation_lock, RW_LOCK_X));

	pinfo = pars_info_create();

	pars_info_add_str_literal(pinfo, "database_name", database_name);
	pars_info_add_str_literal(pinfo, "table_name", table_name);

	ret = dict_stats_exec_sql(
		pinfo,
		"PROCEDURE DELETE_FROM_TABLE_STATS () IS\n"
		"BEGIN\n"
		"DELETE FROM \"" TABLE_STATS_NAME "\" WHERE\n"
		"database_name = :database_name AND\n"
		"table_name = :table_name;\n"
		"END;\n", NULL);

	return(ret);
}

/*********************************************************************//**
Executes
DELETE FROM mysql.innodb_index_stats
WHERE database_name = '...' AND table_name = '...';
Creates its own transaction and commits it.
@return DB_SUCCESS or error code */
UNIV_INLINE
dberr_t
dict_stats_delete_from_index_stats(
/*===============================*/
	const char*	database_name,	/*!< in: database name, e.g. 'db' */
	const char*	table_name)	/*!< in: table name, e.g. 'table' */
{
	pars_info_t*	pinfo;
	dberr_t		ret;

	ut_ad(rw_lock_own(dict_operation_lock, RW_LOCK_X));

	pinfo = pars_info_create();

	pars_info_add_str_literal(pinfo, "database_name", database_name);
	pars_info_add_str_literal(pinfo, "table_name", table_name);

	ret = dict_stats_exec_sql(
		pinfo,
		"PROCEDURE DELETE_FROM_INDEX_STATS () IS\n"
		"BEGIN\n"
		"DELETE FROM \"" INDEX_STATS_NAME "\" WHERE\n"
		"database_name = :database_name AND\n"
		"table_name = :table_name;\n"
		"END;\n", NULL);

	return(ret);
}

/*********************************************************************//**
Removes the statistics for a table and all of its indexes from the
persistent statistics storage if it exists and if there is data stored for
the table. This function creates its own transaction and commits it.
@return DB_SUCCESS or error code */
dberr_t
dict_stats_drop_table(
/*==================*/
	const char*	db_and_table,	/*!< in: db and table, e.g. 'db/table' */
	char*		errstr,		/*!< out: error message
					if != DB_SUCCESS is returned */
	ulint		errstr_sz)	/*!< in: size of errstr buffer */
{
	char		db_utf8[MAX_DB_UTF8_LEN];
	char		table_utf8[MAX_TABLE_UTF8_LEN];
	dberr_t		ret;

	ut_ad(rw_lock_own(dict_operation_lock, RW_LOCK_X));
	/* WL#9536 TODO: Once caller don't hold dict sys mutex, clean
	this and following(exit&enter) up */
	ut_ad(mutex_own(&dict_sys->mutex));

	/* skip tables that do not contain a database name
	e.g. if we are dropping SYS_TABLES */
	if (strchr(db_and_table, '/') == NULL) {

		return(DB_SUCCESS);
	}

	/* skip innodb_table_stats and innodb_index_stats themselves */
	if (strcmp(db_and_table, TABLE_STATS_NAME) == 0
	    || strcmp(db_and_table, INDEX_STATS_NAME) == 0) {

		return(DB_SUCCESS);
	}

	dict_fs2utf8(db_and_table, db_utf8, sizeof(db_utf8),
		     table_utf8, sizeof(table_utf8));

	mutex_exit(&dict_sys->mutex);

	ret = dict_stats_delete_from_table_stats(db_utf8, table_utf8);

	if (ret == DB_SUCCESS) {
		ret = dict_stats_delete_from_index_stats(db_utf8, table_utf8);
	}

	mutex_enter(&dict_sys->mutex);

	if (ret == DB_STATS_DO_NOT_EXIST) {
		ret = DB_SUCCESS;
	}

	if (ret != DB_SUCCESS) {

		snprintf(errstr, errstr_sz,
			    "Unable to delete statistics for table %s.%s: %s."
			    " They can be deleted later using"

			    " DELETE FROM %s WHERE"
			    " database_name = '%s' AND"
			    " table_name = '%s';"

			    " DELETE FROM %s WHERE"
			    " database_name = '%s' AND"
			    " table_name = '%s';",

			    db_utf8, table_utf8,
			    ut_strerr(ret),

			    INDEX_STATS_NAME_PRINT,
			    db_utf8, table_utf8,

			    TABLE_STATS_NAME_PRINT,
			    db_utf8, table_utf8);
	}

	return(ret);
}

/*********************************************************************//**
Executes
UPDATE mysql.innodb_table_stats SET
database_name = '...', table_name = '...'
WHERE database_name = '...' AND table_name = '...';
Creates its own transaction and commits it.
@return DB_SUCCESS or error code */
UNIV_INLINE
dberr_t
dict_stats_rename_table_in_table_stats(
/*===================================*/
	const char*	old_dbname_utf8,/*!< in: database name, e.g. 'olddb' */
	const char*	old_tablename_utf8,/*!< in: table name, e.g. 'oldtable' */
	const char*	new_dbname_utf8,/*!< in: database name, e.g. 'newdb' */
	const char*	new_tablename_utf8)/*!< in: table name, e.g. 'newtable' */
{
	pars_info_t*	pinfo;
	dberr_t		ret;

	ut_ad(rw_lock_own(dict_operation_lock, RW_LOCK_X));

	pinfo = pars_info_create();

	pars_info_add_str_literal(pinfo, "old_dbname_utf8", old_dbname_utf8);
	pars_info_add_str_literal(pinfo, "old_tablename_utf8", old_tablename_utf8);
	pars_info_add_str_literal(pinfo, "new_dbname_utf8", new_dbname_utf8);
	pars_info_add_str_literal(pinfo, "new_tablename_utf8", new_tablename_utf8);

	ret = dict_stats_exec_sql(
		pinfo,
		"PROCEDURE RENAME_TABLE_IN_TABLE_STATS () IS\n"
		"BEGIN\n"
		"UPDATE \"" TABLE_STATS_NAME "\" SET\n"
		"database_name = :new_dbname_utf8,\n"
		"table_name = :new_tablename_utf8\n"
		"WHERE\n"
		"database_name = :old_dbname_utf8 AND\n"
		"table_name = :old_tablename_utf8;\n"
		"END;\n", NULL);

	return(ret);
}

/*********************************************************************//**
Executes
UPDATE mysql.innodb_index_stats SET
database_name = '...', table_name = '...'
WHERE database_name = '...' AND table_name = '...';
Creates its own transaction and commits it.
@return DB_SUCCESS or error code */
UNIV_INLINE
dberr_t
dict_stats_rename_table_in_index_stats(
/*===================================*/
	const char*	old_dbname_utf8,/*!< in: database name, e.g. 'olddb' */
	const char*	old_tablename_utf8,/*!< in: table name, e.g. 'oldtable' */
	const char*	new_dbname_utf8,/*!< in: database name, e.g. 'newdb' */
	const char*	new_tablename_utf8)/*!< in: table name, e.g. 'newtable' */
{
	pars_info_t*	pinfo;
	dberr_t		ret;

	ut_ad(rw_lock_own(dict_operation_lock, RW_LOCK_X));

	pinfo = pars_info_create();

	pars_info_add_str_literal(pinfo, "old_dbname_utf8", old_dbname_utf8);
	pars_info_add_str_literal(pinfo, "old_tablename_utf8", old_tablename_utf8);
	pars_info_add_str_literal(pinfo, "new_dbname_utf8", new_dbname_utf8);
	pars_info_add_str_literal(pinfo, "new_tablename_utf8", new_tablename_utf8);

	ret = dict_stats_exec_sql(
		pinfo,
		"PROCEDURE RENAME_TABLE_IN_INDEX_STATS () IS\n"
		"BEGIN\n"
		"UPDATE \"" INDEX_STATS_NAME "\" SET\n"
		"database_name = :new_dbname_utf8,\n"
		"table_name = :new_tablename_utf8\n"
		"WHERE\n"
		"database_name = :old_dbname_utf8 AND\n"
		"table_name = :old_tablename_utf8;\n"
		"END;\n", NULL);

	return(ret);
}

/*********************************************************************//**
Renames a table in InnoDB persistent stats storage.
This function creates its own transaction and commits it.
@return DB_SUCCESS or error code */
dberr_t
dict_stats_rename_table(
/*====================*/
	const char*	old_name,	/*!< in: old name, e.g. 'db/table' */
	const char*	new_name,	/*!< in: new name, e.g. 'db/table' */
	char*		errstr,		/*!< out: error string if != DB_SUCCESS
					is returned */
	size_t		errstr_sz)	/*!< in: errstr size */
{
	char		old_db_utf8[MAX_DB_UTF8_LEN];
	char		new_db_utf8[MAX_DB_UTF8_LEN];
	char		old_table_utf8[MAX_TABLE_UTF8_LEN];
	char		new_table_utf8[MAX_TABLE_UTF8_LEN];
	dberr_t		ret;

	ut_ad(!rw_lock_own(dict_operation_lock, RW_LOCK_X));

	/* skip innodb_table_stats and innodb_index_stats themselves */
	if (strcmp(old_name, TABLE_STATS_NAME) == 0
	    || strcmp(old_name, INDEX_STATS_NAME) == 0
	    || strcmp(new_name, TABLE_STATS_NAME) == 0
	    || strcmp(new_name, INDEX_STATS_NAME) == 0) {

		return(DB_SUCCESS);
	}

	dict_fs2utf8(old_name, old_db_utf8, sizeof(old_db_utf8),
		     old_table_utf8, sizeof(old_table_utf8));

	dict_fs2utf8(new_name, new_db_utf8, sizeof(new_db_utf8),
		     new_table_utf8, sizeof(new_table_utf8));

	rw_lock_x_lock(dict_operation_lock);

	ulint	n_attempts = 0;
	do {
		n_attempts++;

		ret = dict_stats_rename_table_in_table_stats(
			old_db_utf8, old_table_utf8,
			new_db_utf8, new_table_utf8);

		if (ret == DB_DUPLICATE_KEY) {
			dict_stats_delete_from_table_stats(
				new_db_utf8, new_table_utf8);
		}

		if (ret == DB_STATS_DO_NOT_EXIST) {
			ret = DB_SUCCESS;
		}

		if (ret != DB_SUCCESS) {
			rw_lock_x_unlock(dict_operation_lock);
			os_thread_sleep(200000 /* 0.2 sec */);
			rw_lock_x_lock(dict_operation_lock);
		}
	} while ((ret == DB_DEADLOCK
		  || ret == DB_DUPLICATE_KEY
		  || ret == DB_LOCK_WAIT_TIMEOUT)
		 && n_attempts < 5);

	if (ret != DB_SUCCESS) {
		snprintf(errstr, errstr_sz,
			    "Unable to rename statistics from"
			    " %s.%s to %s.%s in %s: %s."
			    " They can be renamed later using"

			    " UPDATE %s SET"
			    " database_name = '%s',"
			    " table_name = '%s'"
			    " WHERE"
			    " database_name = '%s' AND"
			    " table_name = '%s';",

			    old_db_utf8, old_table_utf8,
			    new_db_utf8, new_table_utf8,
			    TABLE_STATS_NAME_PRINT,
			    ut_strerr(ret),

			    TABLE_STATS_NAME_PRINT,
			    new_db_utf8, new_table_utf8,
			    old_db_utf8, old_table_utf8);
		rw_lock_x_unlock(dict_operation_lock);
		return(ret);
	}
	/* else */

	n_attempts = 0;
	do {
		n_attempts++;

		ret = dict_stats_rename_table_in_index_stats(
			old_db_utf8, old_table_utf8,
			new_db_utf8, new_table_utf8);

		if (ret == DB_DUPLICATE_KEY) {
			dict_stats_delete_from_index_stats(
				new_db_utf8, new_table_utf8);
		}

		if (ret == DB_STATS_DO_NOT_EXIST) {
			ret = DB_SUCCESS;
		}

		if (ret != DB_SUCCESS) {
			rw_lock_x_unlock(dict_operation_lock);
			os_thread_sleep(200000 /* 0.2 sec */);
			rw_lock_x_lock(dict_operation_lock);
		}
	} while ((ret == DB_DEADLOCK
		  || ret == DB_DUPLICATE_KEY
		  || ret == DB_LOCK_WAIT_TIMEOUT)
		 && n_attempts < 5);

	rw_lock_x_unlock(dict_operation_lock);

	if (ret != DB_SUCCESS) {
		snprintf(errstr, errstr_sz,
			    "Unable to rename statistics from"
			    " %s.%s to %s.%s in %s: %s."
			    " They can be renamed later using"

			    " UPDATE %s SET"
			    " database_name = '%s',"
			    " table_name = '%s'"
			    " WHERE"
			    " database_name = '%s' AND"
			    " table_name = '%s';",

			    old_db_utf8, old_table_utf8,
			    new_db_utf8, new_table_utf8,
			    INDEX_STATS_NAME_PRINT,
			    ut_strerr(ret),

			    INDEX_STATS_NAME_PRINT,
			    new_db_utf8, new_table_utf8,
			    old_db_utf8, old_table_utf8);
	}

	return(ret);
}

/*********************************************************************//**
Renames an index in InnoDB persistent stats storage.
This function creates its own transaction and commits it.
@return DB_SUCCESS or error code. DB_STATS_DO_NOT_EXIST will be returned
if the persistent stats do not exist. */
dberr_t
dict_stats_rename_index(
/*====================*/
	const dict_table_t*	table,		/*!< in: table whose index
						is renamed */
	const char*		old_index_name,	/*!< in: old index name */
	const char*		new_index_name)	/*!< in: new index name */
{
	rw_lock_x_lock(dict_operation_lock);

	char	dbname_utf8[MAX_DB_UTF8_LEN];
	char	tablename_utf8[MAX_TABLE_UTF8_LEN];

	dict_fs2utf8(table->name.m_name, dbname_utf8, sizeof(dbname_utf8),
		     tablename_utf8, sizeof(tablename_utf8));

	pars_info_t*	pinfo;

	pinfo = pars_info_create();

	pars_info_add_str_literal(pinfo, "dbname_utf8", dbname_utf8);
	pars_info_add_str_literal(pinfo, "tablename_utf8", tablename_utf8);
	pars_info_add_str_literal(pinfo, "new_index_name", new_index_name);
	pars_info_add_str_literal(pinfo, "old_index_name", old_index_name);

	dberr_t	ret;

	ret = dict_stats_exec_sql(
		pinfo,
		"PROCEDURE RENAME_INDEX_IN_INDEX_STATS () IS\n"
		"BEGIN\n"
		"UPDATE \"" INDEX_STATS_NAME "\" SET\n"
		"index_name = :new_index_name\n"
		"WHERE\n"
		"database_name = :dbname_utf8 AND\n"
		"table_name = :tablename_utf8 AND\n"
		"index_name = :old_index_name;\n"
		"END;\n", NULL);

	rw_lock_x_unlock(dict_operation_lock);

	return(ret);
}

/** Evict the stats tables if they loaded in tablespace cache and also
close the stats .ibd files. We have to close stats tables because
8.0 stats tables will use the same name. We load the stats from 5.7
with a suffix "_backup57" and migrate the statistics. */
void
dict_stats_evict_tablespaces()
{
	ut_ad(srv_is_upgrade_mode);

	space_id_t	space_id_index_stats =
                fil_space_get_id_by_name(INDEX_STATS_NAME);

	space_id_t	space_id_table_stats =
                fil_space_get_id_by_name(TABLE_STATS_NAME);

	trx_t*		trx = trx_allocate_for_background();

	trx_start_internal(trx);

	if (space_id_index_stats != SPACE_UNKNOWN) {

                dberr_t err;

		err = fil_close_tablespace(trx, space_id_index_stats);

                if (err != DB_SUCCESS) {

			ib::info()
				<< "dict_stats_evict_tablespace: "
				<< " fil_close_tablespace("
				<< space_id_index_stats << ") failed! "
				<< ut_strerr(err);
		}
	}

	if (space_id_table_stats != SPACE_UNKNOWN) {
                dberr_t err;

		err = fil_close_tablespace(trx, space_id_table_stats);

                if (err != DB_SUCCESS) {

			ib::info()
				<< "dict_stats_evict_tablespace: "
				<< " fil_close_tablespace("
				<< space_id_index_stats << ") failed! "
				<< ut_strerr(err);
		}
	}

	trx_commit_for_mysql(trx);

	trx_free_for_background(trx);
}

TableStatsRecord::TableStatsRecord()
{
	m_heap = nullptr;
}

TableStatsRecord::~TableStatsRecord()
{
	if (m_heap != nullptr) {
		mem_heap_free(m_heap);
	}
}

ib_uint64_t TableStatsRecord::get_n_rows() const
{
	return(m_n_rows);
}

void TableStatsRecord::set_n_rows(
	ib_uint64_t no_of_rows)
{
	m_n_rows = no_of_rows;
}

ulint TableStatsRecord::get_clustered_index_size() const
{
	return(m_clustered_index_size);
}

void TableStatsRecord::set_clustered_index_size(ulint clust_size)
{
	m_clustered_index_size = clust_size;
}

ulint TableStatsRecord::get_sum_of_other_index_size() const
{
	return(m_sum_of_other_index_sizes);
}

void TableStatsRecord::set_sum_of_other_index_size(
		ulint sum_of_other_index_size)
{
	m_sum_of_other_index_sizes = sum_of_other_index_size;
}

char* TableStatsRecord::get_db_name() const
{
	return(m_db_name);
}

void TableStatsRecord::set_db_name(
	const byte*	data,
	ulint		len)
{
	if (m_heap == nullptr) {
		m_heap = mem_heap_create(MAX_DATABASE_NAME_LEN + 1);
	}

	m_db_name = static_cast<char*>(
			mem_heap_dup(m_heap, data, len + 1));
	m_db_name[len] = '\0';
}

char* TableStatsRecord::get_tbl_name() const
{
	return(m_tbl_name);
}

void TableStatsRecord::set_tbl_name(
	const byte*	data,
	ulint		len)
{
	if (m_heap == nullptr) {
		m_heap = mem_heap_create(MAX_TABLE_NAME_LEN + 1);
	}

	m_tbl_name = static_cast<char*>(
			mem_heap_dup(m_heap, data, len + 1));
	m_tbl_name[len] = '\0';
}

void TableStatsRecord::set_data(
	const byte*	data,
	ulint		col_offset,
	ulint		len)
{
	dict_table_t*	table = dict_sys->table_stats;
	dict_index_t*	index = table->first_index();
	ulint		value;
	ib_uint64_t	n_row;
	ulint		index_col_offset = index->get_col_no(col_offset);

	switch(index_col_offset) {
		case DB_NAME_COL_NO:
			set_db_name(data, len);
			break;
		case TABLE_NAME_COL_NO:
			set_tbl_name(data, len);
			break;
		case N_ROWS_COL_NO:
			n_row = mach_read_from_8(data);
			set_n_rows(n_row);
			break;
		case CLUST_INDEX_SIZE_COL_NO:
			value = mach_read_from_8(data);
			set_clustered_index_size(value);
			break;
		case SUM_OF_OTHER_INDEX_SIZE_COL_NO:
			value = mach_read_from_8(data);
			set_sum_of_other_index_size(value);
			break;
		default:
			break;
	}
}

/* tests @{ */
#ifdef UNIV_COMPILE_TEST_FUNCS
/* save/fetch aux macros @{ */
#define TEST_DATABASE_NAME		"foobardb"
#define TEST_TABLE_NAME			"test_dict_stats"

#define TEST_N_ROWS			111
#define TEST_CLUSTERED_INDEX_SIZE	222
#define TEST_SUM_OF_OTHER_INDEX_SIZES	333

#define TEST_IDX1_NAME			"tidx1"
#define TEST_IDX1_COL1_NAME		"tidx1_col1"
#define TEST_IDX1_INDEX_SIZE		123
#define TEST_IDX1_N_LEAF_PAGES		234
#define TEST_IDX1_N_DIFF1		50
#define TEST_IDX1_N_DIFF1_SAMPLE_SIZE	500

#define TEST_IDX2_NAME			"tidx2"
#define TEST_IDX2_COL1_NAME		"tidx2_col1"
#define TEST_IDX2_COL2_NAME		"tidx2_col2"
#define TEST_IDX2_COL3_NAME		"tidx2_col3"
#define TEST_IDX2_COL4_NAME		"tidx2_col4"
#define TEST_IDX2_INDEX_SIZE		321
#define TEST_IDX2_N_LEAF_PAGES		432
#define TEST_IDX2_N_DIFF1		60
#define TEST_IDX2_N_DIFF1_SAMPLE_SIZE	600
#define TEST_IDX2_N_DIFF2		61
#define TEST_IDX2_N_DIFF2_SAMPLE_SIZE	610
#define TEST_IDX2_N_DIFF3		62
#define TEST_IDX2_N_DIFF3_SAMPLE_SIZE	620
#define TEST_IDX2_N_DIFF4		63
#define TEST_IDX2_N_DIFF4_SAMPLE_SIZE	630
/* @} */

/* test_dict_stats_save() @{ */
void
test_dict_stats_save()
{
	dict_table_t	table;
	dict_index_t	index1;
	dict_field_t	index1_fields[1];
	ib_uint64_t	index1_stat_n_diff_key_vals[1];
	ib_uint64_t	index1_stat_n_sample_sizes[1];
	dict_index_t	index2;
	dict_field_t	index2_fields[4];
	ib_uint64_t	index2_stat_n_diff_key_vals[4];
	ib_uint64_t	index2_stat_n_sample_sizes[4];
	dberr_t		ret;

	/* craft a dummy dict_table_t */
	table.name.m_name = (char*) (TEST_DATABASE_NAME "/" TEST_TABLE_NAME);
	table.stat_n_rows = TEST_N_ROWS;
	table.stat_clustered_index_size = TEST_CLUSTERED_INDEX_SIZE;
	table.stat_sum_of_other_index_sizes = TEST_SUM_OF_OTHER_INDEX_SIZES;
	UT_LIST_INIT(table.indexes, &dict_index_t::indexes);
	UT_LIST_ADD_LAST(table.indexes, &index1);
	UT_LIST_ADD_LAST(table.indexes, &index2);
	ut_d(table.magic_n = DICT_TABLE_MAGIC_N);
	ut_d(index1.magic_n = DICT_INDEX_MAGIC_N);

	index1.name = TEST_IDX1_NAME;
	index1.table = &table;
	index1.cached = 1;
	index1.n_uniq = 1;
	index1.fields = index1_fields;
	index1.stat_n_diff_key_vals = index1_stat_n_diff_key_vals;
	index1.stat_n_sample_sizes = index1_stat_n_sample_sizes;
	index1.stat_index_size = TEST_IDX1_INDEX_SIZE;
	index1.stat_n_leaf_pages = TEST_IDX1_N_LEAF_PAGES;
	index1_fields[0].name = TEST_IDX1_COL1_NAME;
	index1_stat_n_diff_key_vals[0] = TEST_IDX1_N_DIFF1;
	index1_stat_n_sample_sizes[0] = TEST_IDX1_N_DIFF1_SAMPLE_SIZE;

	ut_d(index2.magic_n = DICT_INDEX_MAGIC_N);
	index2.name = TEST_IDX2_NAME;
	index2.table = &table;
	index2.cached = 1;
	index2.n_uniq = 4;
	index2.fields = index2_fields;
	index2.stat_n_diff_key_vals = index2_stat_n_diff_key_vals;
	index2.stat_n_sample_sizes = index2_stat_n_sample_sizes;
	index2.stat_index_size = TEST_IDX2_INDEX_SIZE;
	index2.stat_n_leaf_pages = TEST_IDX2_N_LEAF_PAGES;
	index2_fields[0].name = TEST_IDX2_COL1_NAME;
	index2_fields[1].name = TEST_IDX2_COL2_NAME;
	index2_fields[2].name = TEST_IDX2_COL3_NAME;
	index2_fields[3].name = TEST_IDX2_COL4_NAME;
	index2_stat_n_diff_key_vals[0] = TEST_IDX2_N_DIFF1;
	index2_stat_n_diff_key_vals[1] = TEST_IDX2_N_DIFF2;
	index2_stat_n_diff_key_vals[2] = TEST_IDX2_N_DIFF3;
	index2_stat_n_diff_key_vals[3] = TEST_IDX2_N_DIFF4;
	index2_stat_n_sample_sizes[0] = TEST_IDX2_N_DIFF1_SAMPLE_SIZE;
	index2_stat_n_sample_sizes[1] = TEST_IDX2_N_DIFF2_SAMPLE_SIZE;
	index2_stat_n_sample_sizes[2] = TEST_IDX2_N_DIFF3_SAMPLE_SIZE;
	index2_stat_n_sample_sizes[3] = TEST_IDX2_N_DIFF4_SAMPLE_SIZE;

	ret = dict_stats_save(&table, NULL);

	ut_a(ret == DB_SUCCESS);

	printf("\nOK: stats saved successfully, now go ahead and read"
	       " what's inside %s and %s:\n\n",
	       TABLE_STATS_NAME_PRINT,
	       INDEX_STATS_NAME_PRINT);

	printf("SELECT COUNT(*) = 1 AS table_stats_saved_successfully\n"
	       "FROM %s\n"
	       "WHERE\n"
	       "database_name = '%s' AND\n"
	       "table_name = '%s' AND\n"
	       "n_rows = %d AND\n"
	       "clustered_index_size = %d AND\n"
	       "sum_of_other_index_sizes = %d;\n"
	       "\n",
	       TABLE_STATS_NAME_PRINT,
	       TEST_DATABASE_NAME,
	       TEST_TABLE_NAME,
	       TEST_N_ROWS,
	       TEST_CLUSTERED_INDEX_SIZE,
	       TEST_SUM_OF_OTHER_INDEX_SIZES);

	printf("SELECT COUNT(*) = 3 AS tidx1_stats_saved_successfully\n"
	       "FROM %s\n"
	       "WHERE\n"
	       "database_name = '%s' AND\n"
	       "table_name = '%s' AND\n"
	       "index_name = '%s' AND\n"
	       "(\n"
	       " (stat_name = 'size' AND stat_value = %d AND"
	       "  sample_size IS NULL) OR\n"
	       " (stat_name = 'n_leaf_pages' AND stat_value = %d AND"
	       "  sample_size IS NULL) OR\n"
	       " (stat_name = 'n_diff_pfx01' AND stat_value = %d AND"
	       "  sample_size = '%d' AND stat_description = '%s')\n"
	       ");\n"
	       "\n",
	       INDEX_STATS_NAME_PRINT,
	       TEST_DATABASE_NAME,
	       TEST_TABLE_NAME,
	       TEST_IDX1_NAME,
	       TEST_IDX1_INDEX_SIZE,
	       TEST_IDX1_N_LEAF_PAGES,
	       TEST_IDX1_N_DIFF1,
	       TEST_IDX1_N_DIFF1_SAMPLE_SIZE,
	       TEST_IDX1_COL1_NAME);

	printf("SELECT COUNT(*) = 6 AS tidx2_stats_saved_successfully\n"
	       "FROM %s\n"
	       "WHERE\n"
	       "database_name = '%s' AND\n"
	       "table_name = '%s' AND\n"
	       "index_name = '%s' AND\n"
	       "(\n"
	       " (stat_name = 'size' AND stat_value = %d AND"
	       "  sample_size IS NULL) OR\n"
	       " (stat_name = 'n_leaf_pages' AND stat_value = %d AND"
	       "  sample_size IS NULL) OR\n"
	       " (stat_name = 'n_diff_pfx01' AND stat_value = %d AND"
	       "  sample_size = '%d' AND stat_description = '%s') OR\n"
	       " (stat_name = 'n_diff_pfx02' AND stat_value = %d AND"
	       "  sample_size = '%d' AND stat_description = '%s,%s') OR\n"
	       " (stat_name = 'n_diff_pfx03' AND stat_value = %d AND"
	       "  sample_size = '%d' AND stat_description = '%s,%s,%s') OR\n"
	       " (stat_name = 'n_diff_pfx04' AND stat_value = %d AND"
	       "  sample_size = '%d' AND stat_description = '%s,%s,%s,%s')\n"
	       ");\n"
	       "\n",
	       INDEX_STATS_NAME_PRINT,
	       TEST_DATABASE_NAME,
	       TEST_TABLE_NAME,
	       TEST_IDX2_NAME,
	       TEST_IDX2_INDEX_SIZE,
	       TEST_IDX2_N_LEAF_PAGES,
	       TEST_IDX2_N_DIFF1,
	       TEST_IDX2_N_DIFF1_SAMPLE_SIZE, TEST_IDX2_COL1_NAME,
	       TEST_IDX2_N_DIFF2,
	       TEST_IDX2_N_DIFF2_SAMPLE_SIZE,
	       TEST_IDX2_COL1_NAME, TEST_IDX2_COL2_NAME,
	       TEST_IDX2_N_DIFF3,
	       TEST_IDX2_N_DIFF3_SAMPLE_SIZE,
	       TEST_IDX2_COL1_NAME, TEST_IDX2_COL2_NAME, TEST_IDX2_COL3_NAME,
	       TEST_IDX2_N_DIFF4,
	       TEST_IDX2_N_DIFF4_SAMPLE_SIZE,
	       TEST_IDX2_COL1_NAME, TEST_IDX2_COL2_NAME, TEST_IDX2_COL3_NAME,
	       TEST_IDX2_COL4_NAME);
}
/* @} */

/* test_dict_stats_fetch_from_ps() @{ */
void
test_dict_stats_fetch_from_ps()
{
	dict_table_t	table;
	dict_index_t	index1;
	ib_uint64_t	index1_stat_n_diff_key_vals[1];
	ib_uint64_t	index1_stat_n_sample_sizes[1];
	dict_index_t	index2;
	ib_uint64_t	index2_stat_n_diff_key_vals[4];
	ib_uint64_t	index2_stat_n_sample_sizes[4];
	dberr_t		ret;

	/* craft a dummy dict_table_t */
	table.name.m_name = (char*) (TEST_DATABASE_NAME "/" TEST_TABLE_NAME);
	UT_LIST_INIT(table.indexes, &dict_index_t::indexes);
	UT_LIST_ADD_LAST(table.indexes, &index1);
	UT_LIST_ADD_LAST(table.indexes, &index2);
	ut_d(table.magic_n = DICT_TABLE_MAGIC_N);

	index1.name = TEST_IDX1_NAME;
	ut_d(index1.magic_n = DICT_INDEX_MAGIC_N);
	index1.cached = 1;
	index1.n_uniq = 1;
	index1.stat_n_diff_key_vals = index1_stat_n_diff_key_vals;
	index1.stat_n_sample_sizes = index1_stat_n_sample_sizes;

	index2.name = TEST_IDX2_NAME;
	ut_d(index2.magic_n = DICT_INDEX_MAGIC_N);
	index2.cached = 1;
	index2.n_uniq = 4;
	index2.stat_n_diff_key_vals = index2_stat_n_diff_key_vals;
	index2.stat_n_sample_sizes = index2_stat_n_sample_sizes;

	ret = dict_stats_fetch_from_ps(&table);

	ut_a(ret == DB_SUCCESS);

	ut_a(table.stat_n_rows == TEST_N_ROWS);
	ut_a(table.stat_clustered_index_size == TEST_CLUSTERED_INDEX_SIZE);
	ut_a(table.stat_sum_of_other_index_sizes
	     == TEST_SUM_OF_OTHER_INDEX_SIZES);

	ut_a(index1.stat_index_size == TEST_IDX1_INDEX_SIZE);
	ut_a(index1.stat_n_leaf_pages == TEST_IDX1_N_LEAF_PAGES);
	ut_a(index1_stat_n_diff_key_vals[0] == TEST_IDX1_N_DIFF1);
	ut_a(index1_stat_n_sample_sizes[0] == TEST_IDX1_N_DIFF1_SAMPLE_SIZE);

	ut_a(index2.stat_index_size == TEST_IDX2_INDEX_SIZE);
	ut_a(index2.stat_n_leaf_pages == TEST_IDX2_N_LEAF_PAGES);
	ut_a(index2_stat_n_diff_key_vals[0] == TEST_IDX2_N_DIFF1);
	ut_a(index2_stat_n_sample_sizes[0] == TEST_IDX2_N_DIFF1_SAMPLE_SIZE);
	ut_a(index2_stat_n_diff_key_vals[1] == TEST_IDX2_N_DIFF2);
	ut_a(index2_stat_n_sample_sizes[1] == TEST_IDX2_N_DIFF2_SAMPLE_SIZE);
	ut_a(index2_stat_n_diff_key_vals[2] == TEST_IDX2_N_DIFF3);
	ut_a(index2_stat_n_sample_sizes[2] == TEST_IDX2_N_DIFF3_SAMPLE_SIZE);
	ut_a(index2_stat_n_diff_key_vals[3] == TEST_IDX2_N_DIFF4);
	ut_a(index2_stat_n_sample_sizes[3] == TEST_IDX2_N_DIFF4_SAMPLE_SIZE);

	printf("OK: fetch successful\n");
}
/* @} */

/* test_dict_stats_all() @{ */
void
test_dict_stats_all()
{
	test_dict_stats_save();

	test_dict_stats_fetch_from_ps();
}
/* @} */

#endif /* UNIV_ENABLE_UNIT_TEST_DICT_STATS */
/* @} */
