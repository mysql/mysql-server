/*****************************************************************************

Copyright (c) 2009, 2011, Oracle and/or its affiliates. All Rights Reserved.

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
@file dict/dict0stats.cc
Code used for calculating and manipulating table statistics.

Created Jan 06, 2010 Vasil Dimov
*******************************************************/

#ifndef UNIV_HOTBACKUP

#include "univ.i"

#include "btr0btr.h" /* btr_get_size() */
#include "btr0cur.h" /* btr_estimate_number_of_different_key_vals() */
#include "dict0dict.h" /* dict_table_get_first_index() */
#include "dict0mem.h" /* DICT_TABLE_MAGIC_N */
#include "dict0stats.h"
#include "data0type.h" /* dtype_t */
#include "db0err.h" /* db_err */
#include "dyn0dyn.h" /* dyn_array* */
#include "pars0pars.h" /* pars_info_create() */
#include "pars0types.h" /* pars_info_t */
#include "que0que.h" /* que_eval_sql() */
#include "rem0cmp.h" /* REC_MAX_N_FIELDS,cmp_rec_rec_with_match() */
#include "row0sel.h" /* sel_node_struct */
#include "row0types.h" /* sel_node_t */
#include "trx0trx.h" /* trx_create() */
#include "trx0roll.h" /* trx_rollback_to_savepoint() */
#include "ut0rnd.h" /* ut_rnd_interval() */

#include "ha_prototypes.h" /* innobase_strcasecmp() */

/* Sampling algorithm description @{

The algorithm is controlled by one number - srv_stats_persistent_sample_pages,
let it be A, which is the number of leaf pages to analyze for a given index
for each n-prefix (if the index is on 3 columns, then 3*A pages will be
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

For each n-prefix start from the root level and full scan subsequent lower
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

Let the number of different key values found in page i be Pi (i=1..A)
Let the number of different key values in the whole level LA be V.
Then the total number of different key values in the whole tree is:
V * (P1 + P2 + ... PA) / A.

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

/* number of distinct records on a given level that are required to stop
descending to lower levels and fetch
srv_stats_persistent_sample_pages records from that level */
#define N_DIFF_REQUIRED	(srv_stats_persistent_sample_pages * 10)

/** Open handles on the stats tables. Currently this is used to increase the
reference count of the stats tables. */
typedef struct dict_stats_struct {
	dict_table_t*	table_stats;	/*!< Handle to open TABLE_STATS_NAME */
	dict_table_t*	index_stats;	/*!< Handle to open INDEX_STATS_NAME */
} dict_stats_t;

/*********************************************************************//**
Calculates new estimates for table and index statistics. This function
is relatively quick and is used to calculate transient statistics that
are not saved on disk.
This was the only way to calculate statistics before the
Persistent Statistics feature was introduced.
dict_stats_update_transient() @{ */
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

	index = dict_table_get_first_index(table);

	if (index == NULL) {
		/* Table definition is corrupt */
		ut_print_timestamp(stderr);
		fprintf(stderr, "InnoDB: table %s has no indexes. "
			"Cannot calculate statistics.\n", table->name);
		return;
	}

	do {

		if (index->type & DICT_FTS) {
			index = dict_table_get_next_index(index);
			continue;
		}

		if (UNIV_LIKELY
		    (srv_force_recovery < SRV_FORCE_NO_IBUF_MERGE
		     || (srv_force_recovery < SRV_FORCE_NO_LOG_REDO
			 && dict_index_is_clust(index)))) {
			ulint	size;
			size = btr_get_size(index, BTR_TOTAL_SIZE);

			index->stat_index_size = size;

			sum_of_index_sizes += size;

			size = btr_get_size(index, BTR_N_LEAF_PAGES);

			if (size == 0) {
				/* The root node of the tree is a leaf */
				size = 1;
			}

			index->stat_n_leaf_pages = size;

			btr_estimate_number_of_different_key_vals(index);
		} else {
			/* If we have set a high innodb_force_recovery
			level, do not calculate statistics, as a badly
			corrupted index can cause a crash in it.
			Initialize some bogus index cardinality
			statistics, so that the data can be queried in
			various means, also via secondary indexes. */
			ulint	i;

			sum_of_index_sizes++;
			index->stat_index_size = index->stat_n_leaf_pages = 1;

			for (i = dict_index_get_n_unique(index); i; ) {
				index->stat_n_diff_key_vals[i--] = 1;
			}

			memset(index->stat_n_non_null_key_vals, 0,
			       (1 + dict_index_get_n_unique(index))
			       * sizeof(*index->stat_n_non_null_key_vals));
		}

		index = dict_table_get_next_index(index);
	} while (index);

	index = dict_table_get_first_index(table);

	table->stat_n_rows = index->stat_n_diff_key_vals[
		dict_index_get_n_unique(index)];

	table->stat_clustered_index_size = index->stat_index_size;

	table->stat_sum_of_other_index_sizes = sum_of_index_sizes
		- index->stat_index_size;

	table->stat_modified_counter = 0;

	table->stat_initialized = TRUE;
}
/* @} */

/*********************************************************************//**
Checks whether the persistent statistics storage exists and that all
tables have the proper structure.
dict_stats_persistent_storage_check() @{
@return TRUE if exists and all tables are ok */
static
ibool
dict_stats_persistent_storage_check(
/*================================*/
	ibool	caller_has_dict_sys_mutex)	/*!< in: TRUE if the caller
						owns dict_sys->mutex */
{
	/* definition for the table TABLE_STATS_NAME */
	dict_col_meta_t	table_stats_columns[] = {
		{"database_name", DATA_VARMYSQL,
			DATA_NOT_NULL, 192 /* NAME_LEN from mysql_com.h */},

		{"table_name", DATA_VARMYSQL,
			DATA_NOT_NULL, 192 /* NAME_LEN from mysql_com.h */},

		{"stats_timestamp", DATA_FIXBINARY,
			DATA_NOT_NULL, 4},

		{"n_rows", DATA_INT,
			DATA_NOT_NULL | DATA_UNSIGNED, 8},

		{"clustered_index_size", DATA_INT,
			DATA_NOT_NULL | DATA_UNSIGNED, 8},

		{"sum_of_other_index_sizes", DATA_INT,
			DATA_NOT_NULL | DATA_UNSIGNED, 8}
	};
	dict_table_schema_t	table_stats_schema = {
		TABLE_STATS_NAME,
		UT_ARR_SIZE(table_stats_columns),
		table_stats_columns
	};

	/* definition for the table INDEX_STATS_NAME */
	dict_col_meta_t	index_stats_columns[] = {
		{"database_name", DATA_VARMYSQL,
			DATA_NOT_NULL, 192 /* NAME_LEN from mysql_com.h */},

		{"table_name", DATA_VARMYSQL,
			DATA_NOT_NULL, 192 /* NAME_LEN from mysql_com.h */},

		{"index_name", DATA_VARMYSQL,
			DATA_NOT_NULL, 192 /* NAME_LEN from mysql_com.h */},

		{"stat_timestamp", DATA_FIXBINARY,
			DATA_NOT_NULL, 4},

		{"stat_name", DATA_VARMYSQL,
			DATA_NOT_NULL, 64*3},

		{"stat_value", DATA_INT,
			DATA_NOT_NULL | DATA_UNSIGNED, 8},

		{"sample_size", DATA_INT,
			DATA_UNSIGNED, 8},

		{"stat_description", DATA_VARMYSQL,
			DATA_NOT_NULL, 1024*3}
	};
	dict_table_schema_t	index_stats_schema = {
		INDEX_STATS_NAME,
		UT_ARR_SIZE(index_stats_columns),
		index_stats_columns
	};

	char		errstr[512];
	enum db_err	ret;

	if (!caller_has_dict_sys_mutex) {
		mutex_enter(&(dict_sys->mutex));
	}

	ut_ad(mutex_own(&dict_sys->mutex));

	/* first check table_stats */
	ret = dict_table_schema_check(&table_stats_schema, errstr,
				      sizeof(errstr));
	if (ret == DB_SUCCESS) {
		/* if it is ok, then check index_stats */
		ret = dict_table_schema_check(&index_stats_schema, errstr,
					      sizeof(errstr));
	}

	if (!caller_has_dict_sys_mutex) {
		mutex_exit(&(dict_sys->mutex));
	}

	if (ret != DB_SUCCESS && ret != DB_TABLE_NOT_FOUND) {

		ut_print_timestamp(stderr);
		fprintf(stderr, " InnoDB: %s\n", errstr);
	}
	/* We return silently if some of the tables are not present because
	this code is executed during open table. By design we check if the
	persistent statistics storage is present and whether there are stats
	for the table being opened and if so, then we use them, otherwise we
	silently switch back to using the transient stats. */

	return(ret == DB_SUCCESS);
}
/* @} */

/* @{ Pseudo code about the relation between the following functions

let N = srv_stats_persistent_sample_pages

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
saved in the array n_diff[]. Notice that n_diff[] must be able to store
n_uniq+1 numbers because the results are saved in
n_diff[1] .. n_diff[n_uniq]. The total number of records on the level is
saved in total.
Also, the index of the last record in each group of equal records is saved
in n_diff_boundaries[1..n_uniq], records indexing starts from the leftmost
record on the level and continues cross pages boundaries, counting from 0.
dict_stats_analyze_index_level() @{ */
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
	dyn_array_t*	n_diff_boundaries)/*!< out: boundaries of the groups
					of distinct keys */
{
	ulint		n_uniq;
	mem_heap_t*	heap;
	dtuple_t*	dtuple;
	btr_pcur_t	pcur;
	mtr_t		mtr;
	const page_t*	page;
	const rec_t*	rec;
	const rec_t*	prev_rec;
	byte*		prev_rec_buf = NULL;
	ulint		prev_rec_buf_size = 0;
	ulint		i;

	DEBUG_PRINTF("    %s(table=%s, index=%s, level=%lu)\n", __func__,
		     index->table->name, index->name, level);

	n_uniq = dict_index_get_n_unique(index);

	/* elements in the n_diff array are 1..n_uniq (inclusive) */
	memset(n_diff, 0x0, (n_uniq + 1) * sizeof(*n_diff));

	heap = mem_heap_create(256);

	/* reset the dynamic arrays n_diff_boundaries[1..n_uniq];
	n_diff_boundaries[0] is ignored to follow the same convention
	as n_diff[] */
	if (n_diff_boundaries != NULL) {
		for (i = 1; i <= n_uniq; i++) {
			dyn_array_free(&n_diff_boundaries[i]);

			dyn_array_create(&n_diff_boundaries[i]);
		}
	}

	/* craft a record that is always smaller than the others,
	this way we are sure that the cursor pcur will be positioned
	on the leftmost record on the leftmost page on the desired level */
	dtuple = dtuple_create(heap, dict_index_get_n_unique(index));
	dict_table_copy_types(dtuple, index->table);
	dtuple_set_info_bits(dtuple, REC_INFO_MIN_REC_FLAG);

	mtr_start(&mtr);

	btr_pcur_open_low(index, level, dtuple, PAGE_CUR_LE, BTR_SEARCH_LEAF,
			  &pcur, __FILE__, __LINE__, &mtr);

	page = btr_pcur_get_page(&pcur);

	/* check that we are indeed on the desired level */
	ut_a(btr_page_get_level(page, &mtr) == level);

	/* there should not be any pages on the left */
	ut_a(btr_page_get_prev(page, &mtr) == FIL_NULL);

	/* check whether the first record on the leftmost page is marked
	as such, if we are on a non-leaf level */
	ut_a(level == 0
	     || (REC_INFO_MIN_REC_FLAG & rec_get_info_bits(
		     page_rec_get_next_const(page_get_infimum_rec(page)),
		     page_is_comp(page))));

	if (btr_pcur_is_before_first_on_page(&pcur)) {
		btr_pcur_move_to_next_on_page(&pcur);
	}

	if (btr_pcur_is_after_last_on_page(&pcur)) {
		btr_pcur_move_to_prev_on_page(&pcur);
	}

	prev_rec = NULL;

	/* no records by default */
	*total_recs = 0;

	*total_pages = 0;

	/* iterate over all user records on this level
	and compare each two adjacent ones, even the last on page
	X and the fist on page X+1 */
	for (;
	     btr_pcur_is_on_user_rec(&pcur);
	     btr_pcur_move_to_next_user_rec(&pcur, &mtr)) {

		ulint	matched_fields = 0;
		ulint	matched_bytes = 0;
		ulint	offsets_rec_onstack[REC_OFFS_NORMAL_SIZE];
		ulint*	offsets_rec;

		rec_offs_init(offsets_rec_onstack);

		rec = btr_pcur_get_rec(&pcur);

		/* increment the pages counter at the end of each page */
		if (page_rec_is_supremum(page_rec_get_next_const(rec))) {

			(*total_pages)++;
		}

		/* skip delete-marked records */
		if (rec_get_deleted_flag(rec, page_is_comp(
				btr_pcur_get_page(&pcur)))) {

			continue;
		}

		offsets_rec = rec_get_offsets(rec, index, offsets_rec_onstack,
					      n_uniq, &heap);

		(*total_recs)++;

		if (prev_rec != NULL) {

			ulint	offsets_prev_rec_onstack[REC_OFFS_NORMAL_SIZE];
			ulint*	offsets_prev_rec;

			rec_offs_init(offsets_prev_rec_onstack);

			offsets_prev_rec = rec_get_offsets(
				prev_rec, index, offsets_prev_rec_onstack,
				n_uniq, &heap);

			cmp_rec_rec_with_match(rec,
					       prev_rec,
					       offsets_rec,
					       offsets_prev_rec,
					       index,
					       FALSE,
					       &matched_fields,
					       &matched_bytes);

			for (i = matched_fields + 1; i <= n_uniq; i++) {

				if (n_diff_boundaries != NULL) {
					/* push the index of the previous
					record, that is - the last one from
					a group of equal keys */

					void*		p;
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

					p = dyn_array_push(
						&n_diff_boundaries[i],
						sizeof(ib_uint64_t));

					memcpy(p, &idx, sizeof(ib_uint64_t));
				}

				/* increment the number of different keys
				for n_prefix=i */
				n_diff[i]++;
			}
		} else {
			/* this is the first non-delete marked record */
			for (i = 1; i <= n_uniq; i++) {
				n_diff[i] = 1;
			}
		}

		if (page_rec_is_supremum(page_rec_get_next_const(rec))) {
			/* end of a page has been reached */

			/* we need to copy the record instead of assigning
			like prev_rec = rec; because when we traverse the
			records on this level at some point we will jump from
			one page to the next and then rec and prev_rec will
			be on different pages and
			btr_pcur_move_to_next_user_rec() will release the
			latch on the page that prev_rec is on */
			prev_rec = rec_copy_prefix_to_buf(
				rec, index, rec_offs_n_fields(offsets_rec),
				&prev_rec_buf, &prev_rec_buf_size);

		} else {
			/* still on the same page, the next call to
			btr_pcur_move_to_next_user_rec() will not jump
			on the next page, we can simply assign pointers
			instead of copying the records like above */

			prev_rec = rec;
		}
	}

	/* if *total_pages is left untouched then the above loop was not
	entered at all and there is one page in the whole tree which is
	empty */
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
		for (i = 1; i <= n_uniq; i++) {
			void*		p;
			ib_uint64_t	idx;

			idx = *total_recs - 1;

			p = dyn_array_push(&n_diff_boundaries[i],
					   sizeof(ib_uint64_t));

			memcpy(p, &idx, sizeof(ib_uint64_t));
		}
	}

	/* now in n_diff_boundaries[i] there are exactly n_diff[i] integers,
	for i=1..n_uniq */

#ifdef UNIV_STATS_DEBUG
	for (i = 1; i <= n_uniq; i++) {

		DEBUG_PRINTF("    %s(): total recs: %llu, total pages: %llu, "
			     "n_diff[%lu]: %lld\n",
			     __func__, *total_recs, *total_pages,
			     i, n_diff[i]);

		if (n_diff_boundaries != NULL) {
			ib_uint64_t	j;

			printf("boundaries: ");
			for (j = 0; j < n_diff[i]; j++) {
				ib_uint64_t	idx;

				idx = *(ib_uint64_t*) dyn_array_get_element(
					&n_diff_boundaries[i],
					j * sizeof(ib_uint64_t));

				printf(UINT64PF "=" UINT64PF ", ", j, idx);
			}
			printf("\n");
		}
	}
#endif /* UNIV_STATS_DEBUG */

	btr_pcur_close(&pcur);

	mtr_commit(&mtr);

	if (prev_rec_buf != NULL) {

		mem_free(prev_rec_buf);
	}

	mem_heap_free(heap);
}
/* @} */

/* aux enum for controlling the behavior of dict_stats_scan_page() @{ */
typedef enum page_scan_method_enum {
	COUNT_ALL_NON_BORING,	/* scan all records on the given page
				and count the number of distinct ones */
	QUIT_ON_FIRST_NON_BORING/* quit when the first record that differs
				from its right neighbor is found */
} page_scan_method_t;
/* @} */

/*********************************************************************//**
Scan a page, reading records from left to right and counting the number
of distinct records on that page (looking only at the first n_prefix
columns). If scan_method is QUIT_ON_FIRST_NON_BORING then the function
will return as soon as it finds a record that does not match its neighbor
to the right, which means that in the case of QUIT_ON_FIRST_NON_BORING the
returned n_diff can either be 0 (empty page), 1 (the whole page has all keys
equal) or 2 (the function found a non-boring record and returned).
@return offsets1 or offsets2 (the offsets of *out_rec),
or NULL if the page is empty and does not contain user records.
dict_stats_scan_page() @{ */
UNIV_INLINE __attribute__((nonnull))
ulint*
dict_stats_scan_page(
/*=================*/
	const rec_t**		out_rec,	/*!< out: record, or NULL */
	ulint*			offsets1,	/*!< out: rec_get_offsets()
						working space (must be big
						enough) */
	ulint*			offsets2,	/*!< out: rec_get_offsets()
						working space (must be big
						enough) */
	dict_index_t*		index,		/*!< in: index of the page */
	const page_t*		page,		/*!< in: the page to scan */
	ulint			n_prefix,	/*!< in: look at the first
						n_prefix columns */
	page_scan_method_t	scan_method,	/*!< in: scan to the end of
						the page or not */
	ib_uint64_t*		n_diff)		/*!< out: number of distinct
						records encountered */
{
	ulint*		offsets_rec		= offsets1;
	ulint*		offsets_next_rec	= offsets2;
	const rec_t*	rec;
	const rec_t*	next_rec;
	/* A dummy heap, to be passed to rec_get_offsets().
	Because offsets1,offsets2 should be big enough,
	this memory heap should never be used. */
	mem_heap_t*	heap			= NULL;

	rec = page_rec_get_next_const(page_get_infimum_rec(page));

	if (page_rec_is_supremum(rec)) {
		/* the page is empty */
		*n_diff = 0;
		*out_rec = NULL;
		return(NULL);
	}

	offsets_rec = rec_get_offsets(rec, index, offsets_rec,
				      ULINT_UNDEFINED, &heap);

	next_rec = page_rec_get_next_const(rec);

	*n_diff = 1;

	while (!page_rec_is_supremum(next_rec)) {

		ulint	matched_fields = 0;
		ulint	matched_bytes = 0;

		offsets_next_rec = rec_get_offsets(next_rec, index,
						   offsets_next_rec,
						   ULINT_UNDEFINED,
						   &heap);

		/* check whether rec != next_rec when looking at
		the first n_prefix fields */
		cmp_rec_rec_with_match(rec, next_rec,
				       offsets_rec, offsets_next_rec,
				       index, FALSE, &matched_fields,
				       &matched_bytes);


		if (matched_fields < n_prefix) {
			/* rec != next_rec, => rec is non-boring */

			(*n_diff)++;

			if (scan_method == QUIT_ON_FIRST_NON_BORING) {
				goto func_exit;
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
			offsets_onstack1 and offsets_onstack2 and we
			are using them in circular fashion
			(offsets[_next]_rec are just pointers to
			those placeholders). */
			ulint*	offsets_tmp;
			offsets_tmp = offsets_rec;
			offsets_rec = offsets_next_rec;
			offsets_next_rec = offsets_tmp;
		}
		next_rec = page_rec_get_next_const(next_rec);
	}

func_exit:
	/* offsets1,offsets2 should have been big enough */
	ut_a(heap == NULL);
	*out_rec = rec;
	return(offsets_rec);
}
/* @} */

/*********************************************************************//**
Dive below the current position of a cursor and calculate the number of
distinct records on the leaf page, when looking at the fist n_prefix
columns.
dict_stats_analyze_index_below_cur() @{
@return number of distinct records on the leaf page */
static
ib_uint64_t
dict_stats_analyze_index_below_cur(
/*===============================*/
	const btr_cur_t*cur,		/*!< in: cursor */
	ulint		n_prefix,	/*!< in: look at the first n_prefix
					columns when comparing records */
	mtr_t*		mtr)		/*!< in/out: mini-transaction */
{
	dict_index_t*	index;
	ulint		space;
	ulint		zip_size;
	buf_block_t*	block;
	ulint		page_no;
	const page_t*	page;
	mem_heap_t*	heap;
	const rec_t*	rec;
	ulint*		offsets1;
	ulint*		offsets2;
	ulint*		offsets_rec;
	ulint		root_height;
	ib_uint64_t	n_diff; /* the result */
	ulint		size;

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

	root_height = btr_page_get_level(btr_root_get(index, mtr), mtr);

	space = dict_index_get_space(index);
	zip_size = dict_table_zip_size(index->table);

	rec = btr_cur_get_rec(cur);

	offsets_rec = rec_get_offsets(rec, index, offsets1,
				      ULINT_UNDEFINED, &heap);

	page_no = btr_node_ptr_get_child_page_no(rec, offsets_rec);

	/* descend to the leaf level on the B-tree */
	for (;;) {

		block = buf_page_get_gen(space, zip_size, page_no, RW_S_LATCH,
					 NULL /* no guessed block */,
					 BUF_GET, __FILE__, __LINE__, mtr);

		page = buf_block_get_frame(block);

		if (btr_page_get_level(page, mtr) == 0) {
			/* leaf level */
			break;
		}
		/* else */

		/* search for the first non-boring record on the page */
		offsets_rec = dict_stats_scan_page(
			&rec, offsets1, offsets2, index, page, n_prefix,
			QUIT_ON_FIRST_NON_BORING, &n_diff);

		/* pages on level > 0 are not allowed to be empty */
		ut_a(offsets_rec != NULL);
		/* if page is not empty (rec != NULL) then n_diff must
		be > 0, otherwise there is a bug in dict_stats_scan_page() */
		ut_a(n_diff > 0);

		if (n_diff == 1) {
			/* page has all keys equal and the end of the page
			was reached by dict_stats_scan_page(), no need to
			descend to the leaf level */
			mem_heap_free(heap);
			return(1);
		}
		/* else */

		/* when we instruct dict_stats_scan_page() to quit on the
		first non-boring record it finds, then the returned n_diff
		can either be 0 (empty page), 1 (page has all keys equal) or
		2 (non-boring record was found) */
		ut_a(n_diff == 2);

		/* we have a non-boring record in rec, descend below it */

		page_no = btr_node_ptr_get_child_page_no(rec, offsets_rec);
	}

	/* make sure we got a leaf page as a result from the above loop */
	ut_ad(btr_page_get_level(page, mtr) == 0);

	/* scan the leaf page and find the number of distinct keys,
	when looking only at the first n_prefix columns */

	offsets_rec = dict_stats_scan_page(
		&rec, offsets1, offsets2, index, page, n_prefix,
		COUNT_ALL_NON_BORING, &n_diff);

	if (root_height > 0) {

		/* empty pages are allowed only if the whole B-tree is empty
		and contains a single empty page */
		ut_a(offsets_rec != NULL);
	}

#if 0
	DEBUG_PRINTF("      %s(): n_diff below page_no=%lu: %llu\n",
		     __func__, page_no, n_diff);
#endif

	mem_heap_free(heap);

	return(n_diff);
}
/* @} */

/*********************************************************************//**
For a given level in an index select srv_stats_persistent_sample_pages
(or less) records from that level and dive below them to the corresponding
leaf pages, then scan those leaf pages and save the sampling results in
index->stat_n_diff_key_vals[n_prefix] and the number of pages scanned in
index->stat_n_sample_sizes[n_prefix].
dict_stats_analyze_index_for_n_prefix() @{ */
static
void
dict_stats_analyze_index_for_n_prefix(
/*==================================*/
	dict_index_t*	index,			/*!< in/out: index */
	ulint		level,			/*!< in: level,
						must be >= 1 */
	ib_uint64_t	total_recs_on_level,	/*!< in: total number of
						records on the given level */
	ulint		n_prefix,		/*!< in: look at first
						n_prefix columns when
						comparing records */
	ib_uint64_t	n_diff_for_this_prefix,	/*!< in: number of distinct
						records on the given level,
						when looking at the first
						n_prefix columns */

	dyn_array_t*	boundaries)		/*!< in: array that contains
						n_diff_for_this_prefix
						integers each of which
						represents the index (on the
						level, counting from
						left/smallest to right/biggest
						from 0) of the last record
						from each group of distinct
						keys */
{
	mem_heap_t*	heap;
	dtuple_t*	dtuple;
	btr_pcur_t	pcur;
	mtr_t		mtr;
	const page_t*	page;
	ib_uint64_t	rec_idx;
	ib_uint64_t	last_idx_on_level;
	ib_uint64_t	n_recs_to_dive_below;
	ib_uint64_t	n_diff_sum_of_all_analyzed_pages;
	ib_uint64_t	i;

#if 0
	DEBUG_PRINTF("    %s(table=%s, index=%s, level=%lu, n_prefix=%lu, "
		     "n_diff_for_this_prefix=%llu)\n",
		     __func__, index->table->name, index->name, level,
		     n_prefix, n_diff_for_this_prefix);
#endif

	/* if some of those is 0 then this means that there is exactly one
	page in the B-tree and it is empty and we should have done full scan
	and should not be here */
	ut_ad(total_recs_on_level > 0);
	ut_ad(n_diff_for_this_prefix > 0);

	/* this is configured to be min 1, someone has changed the code */
	ut_ad(srv_stats_persistent_sample_pages > 0);

	heap = mem_heap_create(256);

	/* craft a record that is always smaller than the others,
	this way we are sure that the cursor pcur will be positioned
	on the leftmost record on the leftmost page on the desired level */
	dtuple = dtuple_create(heap, dict_index_get_n_unique(index));
	dict_table_copy_types(dtuple, index->table);
	dtuple_set_info_bits(dtuple, REC_INFO_MIN_REC_FLAG);

	mtr_start(&mtr);

	btr_pcur_open_low(index, level, dtuple, PAGE_CUR_LE, BTR_SEARCH_LEAF,
			  &pcur, __FILE__, __LINE__, &mtr);

	page = btr_pcur_get_page(&pcur);

	/* check that we are indeed on the desired level */
	ut_a(btr_page_get_level(page, &mtr) == level);

	/* there should not be any pages on the left */
	ut_a(btr_page_get_prev(page, &mtr) == FIL_NULL);

	/* check whether the first record on the leftmost page is marked
	as such, if we are on a non-leaf level */
	ut_a(level == 0 || REC_INFO_MIN_REC_FLAG
	     & rec_get_info_bits(page_rec_get_next_const(
					 page_get_infimum_rec(page)),
				 page_is_comp(page)));

	if (btr_pcur_is_before_first_on_page(&pcur)) {
		btr_pcur_move_to_next_on_page(&pcur);
	}

	if (btr_pcur_is_after_last_on_page(&pcur)) {
		btr_pcur_move_to_prev_on_page(&pcur);
	}

	last_idx_on_level = *(ib_uint64_t*) dyn_array_get_element(boundaries,
		(ulint) ((n_diff_for_this_prefix - 1) * sizeof(ib_uint64_t)));

	rec_idx = 0;

	n_diff_sum_of_all_analyzed_pages = 0;

	n_recs_to_dive_below = ut_min(srv_stats_persistent_sample_pages,
				      n_diff_for_this_prefix);

	for (i = 0; i < n_recs_to_dive_below; i++) {
		ib_uint64_t	left;
		ib_uint64_t	right;
		ulint		rnd;
		ib_uint64_t	dive_below_idx;

		/* there are n_diff_for_this_prefix elements
		in the array boundaries[] and we divide those elements
		into n_recs_to_dive_below segments, for example:

		let n_diff_for_this_prefix=100, n_recs_to_dive_below=4, then:
		segment i=0:  [0, 24]
		segment i=1: [25, 49]
		segment i=2: [50, 74]
		segment i=3: [75, 99] or

		let n_diff_for_this_prefix=1, n_recs_to_dive_below=1, then:
		segment i=0: [0, 0] or

		let n_diff_for_this_prefix=2, n_recs_to_dive_below=2, then:
		segment i=0: [0, 0]
		segment i=1: [1, 1] or

		let n_diff_for_this_prefix=13, n_recs_to_dive_below=7, then:
		segment i=0:  [0,  0]
		segment i=1:  [1,  2]
		segment i=2:  [3,  4]
		segment i=3:  [5,  6]
		segment i=4:  [7,  8]
		segment i=5:  [9, 10]
		segment i=6: [11, 12]

		then we select a random record from each segment and dive
		below it */
		left = n_diff_for_this_prefix * i / n_recs_to_dive_below;
		right = n_diff_for_this_prefix * (i + 1)
			/ n_recs_to_dive_below - 1;

		ut_a(left <= right);
		ut_a(right <= last_idx_on_level);

		/* we do not pass (left, right) because we do not want to ask
		ut_rnd_interval() to work with too big numbers since
		ib_uint64_t could be bigger than ulint */
		rnd = ut_rnd_interval(0, (ulint) (right - left));

		dive_below_idx = *(ib_uint64_t*) dyn_array_get_element(
			boundaries, (ulint) ((left + rnd)
					     * sizeof(ib_uint64_t)));

#if 0
		DEBUG_PRINTF("    %s(): dive below rec_idx=%llu\n",
			     __func__, dive_below_idx);
#endif

		/* seek to the record with index dive_below_idx */
		while (rec_idx < dive_below_idx
		       && btr_pcur_is_on_user_rec(&pcur)) {

			btr_pcur_move_to_next_user_rec(&pcur, &mtr);
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

		ut_a(rec_idx == dive_below_idx);

		n_diff_sum_of_all_analyzed_pages
			+= dict_stats_analyze_index_below_cur(
				btr_pcur_get_btr_cur(&pcur), n_prefix, &mtr);
	}

	index->stat_n_diff_key_vals[n_prefix]
		= total_recs_on_level * n_diff_sum_of_all_analyzed_pages
		/ n_recs_to_dive_below;

	index->stat_n_sample_sizes[n_prefix] = n_recs_to_dive_below;

	DEBUG_PRINTF("    %s(): n_diff=%llu for n_prefix=%lu\n",
		     __func__, index->stat_n_diff_key_vals[n_prefix],
		     n_prefix);

	btr_pcur_close(&pcur);

	mtr_commit(&mtr);

	mem_heap_free(heap);
}
/* @} */

/*********************************************************************//**
Calculates new statistics for a given index and saves them to the index
members stat_n_diff_key_vals[], stat_n_sample_sizes[], stat_index_size and
stat_n_leaf_pages. This function could be slow.
dict_stats_analyze_index() @{
@return DB_SUCCESS or error code */
static
enum db_err
dict_stats_analyze_index(
/*=====================*/
	dict_index_t*	index)	/*!< in/out: index to analyze */
{
	ulint		root_level;
	ulint		level;
	ibool		level_is_analyzed;
	ulint		n_uniq;
	ulint		n_prefix;
	ib_uint64_t*	n_diff_on_level;
	ib_uint64_t	total_recs;
	ib_uint64_t	total_pages;
	dyn_array_t*	n_diff_boundaries;
	mtr_t		mtr;
	ulint		i;

	DEBUG_PRINTF("  %s(index=%s)\n", __func__, index->name);

	index->stat_index_size = btr_get_size(index, BTR_TOTAL_SIZE);

	index->stat_n_leaf_pages = btr_get_size(index, BTR_N_LEAF_PAGES);
	if (index->stat_n_leaf_pages == 0) {
		/* The root node of the tree is a leaf */
		index->stat_n_leaf_pages = 1;
	}

	mtr_start(&mtr);

	mtr_s_lock(dict_index_get_lock(index), &mtr);

	root_level = btr_page_get_level(btr_root_get(index, &mtr), &mtr);

	mtr_commit(&mtr);

	n_uniq = dict_index_get_n_unique(index);

	/* if the tree has just one level (and one page) or if the user
	has requested to sample too many pages then do full scan */
	if (root_level == 0
	    /* for each n-column prefix (for n=1..n_uniq)
	    srv_stats_persistent_sample_pages will be sampled, so in total
	    srv_stats_persistent_sample_pages * n_uniq leaf pages will be
	    sampled. If that number is bigger than the total number of leaf
	    pages then do full scan of the leaf level instead since it will
	    be faster and will give better results. */
	    || srv_stats_persistent_sample_pages * n_uniq
	       > index->stat_n_leaf_pages) {

		if (root_level == 0) {
			DEBUG_PRINTF("  %s(): just one page, "
				     "doing full scan\n", __func__);
		} else {
			DEBUG_PRINTF("  %s(): too many pages requested for "
				     "sampling, doing full scan\n", __func__);
		}

		/* do full scan of level 0; save results directly
		into the index */

		dict_stats_analyze_index_level(index,
					       0 /* leaf level */,
					       index->stat_n_diff_key_vals,
					       &total_recs,
					       &total_pages,
					       NULL /*boundaries not needed*/);

		for (i = 1; i <= n_uniq; i++) {
			index->stat_n_sample_sizes[i] = total_pages;
		}

		return(DB_SUCCESS);
	}
	/* else */

	/* set to zero */
	n_diff_on_level = (ib_uint64_t*) mem_zalloc((n_uniq + 1)
						    * sizeof(ib_uint64_t));

	n_diff_boundaries = (dyn_array_t*) mem_alloc((n_uniq + 1)
						     * sizeof(dyn_array_t));

	for (i = 1; i <= n_uniq; i++) {
		/* initialize the dynamic arrays, the first one
		(index=0) is ignored to follow the same indexing
		scheme as n_diff_on_level[] */
		dyn_array_create(&n_diff_boundaries[i]);
	}

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
	level = (long) root_level;
	level_is_analyzed = FALSE;
	for (n_prefix = n_uniq; n_prefix >= 1; n_prefix--) {

		DEBUG_PRINTF("  %s(): searching level with >=%llu "
			     "distinct records, n_prefix=%lu\n",
			     __func__, N_DIFF_REQUIRED, n_prefix);

		/* check whether we should pick the current level;
		we pick level 1 even if it does not have enough
		distinct records because we do not want to scan the
		leaf level because it may contain too many records */
		if (level_is_analyzed
		    && (n_diff_on_level[n_prefix] >= N_DIFF_REQUIRED
			|| level == 1)) {

			goto found_level;
		}
		/* else */

		/* search for a level that contains enough distinct records */

		if (level_is_analyzed && level > 1) {

			/* if this does not hold we should be on
			"found_level" instead of here */
			ut_ad(n_diff_on_level[n_prefix] < N_DIFF_REQUIRED);

			level--;
			level_is_analyzed = FALSE;
		}

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
			if (total_recs > srv_stats_persistent_sample_pages) {

				/* if the above cond is true then we are not
				at the root level since on the root level
				total_recs == 1 and cannot
				be > srv_stats_persistent_sample_pages */
				ut_a(level != root_level);

				/* step one level back and be satisfied with
				whatever it contains */
				level++;
				level_is_analyzed = TRUE;

				break;
			}

			dict_stats_analyze_index_level(index,
						       level,
						       n_diff_on_level,
						       &total_recs,
						       &total_pages,
						       n_diff_boundaries);

			level_is_analyzed = TRUE;

			if (n_diff_on_level[n_prefix] >= N_DIFF_REQUIRED
			    || level == 1) {
				/* we found a good level with many distinct
				records or we have reached the last level we
				could scan */
				break;
			}
			/* else */

			level--;
			level_is_analyzed = FALSE;
		}
found_level:

		DEBUG_PRINTF("  %s(): found level %lu that has %llu "
			     "distinct records for n_prefix=%lu\n",
			     __func__, level, n_diff_on_level[n_prefix],
			     n_prefix);

		/* here we are either on level 1 or the level that we are on
		contains >= N_DIFF_REQUIRED distinct keys or we did not scan
		deeper levels because they would contain too many pages */

		ut_ad(level > 0);

		ut_ad(level_is_analyzed);

		/* pick some records from this level and dive below them for
		the given n_prefix */

		dict_stats_analyze_index_for_n_prefix(
			index, level, total_recs, n_prefix,
			n_diff_on_level[n_prefix],
			&n_diff_boundaries[n_prefix]);
	}

	for (i = 1; i <= n_uniq; i++) {
		dyn_array_free(&n_diff_boundaries[i]);
	}

	mem_free(n_diff_boundaries);

	mem_free(n_diff_on_level);

	return(DB_SUCCESS);
}
/* @} */

/*********************************************************************//**
Calculates new estimates for table and index statistics. This function
is relatively slow and is used to calculate persistent statistics that
will be saved on disk.
dict_stats_update_persistent() @{
@return DB_SUCCESS or error code */
static
enum db_err
dict_stats_update_persistent(
/*=========================*/
	dict_table_t*	table)		/*!< in/out: table */
{
	dict_index_t*	index;

	DEBUG_PRINTF("%s(table=%s)\n", __func__, table->name);

	/* XXX quit if interrupted, e.g. SIGTERM */

	/* analyze the clustered index first */

	index = dict_table_get_first_index(table);

	if (index == NULL) {
		/* Table definition is corrupt */
		return(DB_CORRUPTION);
	}

	dict_stats_analyze_index(index);

	table->stat_n_rows
		= index->stat_n_diff_key_vals[dict_index_get_n_unique(index)];

	table->stat_clustered_index_size = index->stat_index_size;

	/* analyze other indexes from the table, if any */

	table->stat_sum_of_other_index_sizes = 0;

	for (index = dict_table_get_next_index(index);
	     index != NULL;
	     index = dict_table_get_next_index(index)) {

		if (index->type & DICT_FTS) {
			continue;
		}

		dict_stats_analyze_index(index);

		table->stat_sum_of_other_index_sizes
			+= index->stat_index_size;
	}

	table->stat_modified_counter = 0;

	table->stat_initialized = TRUE;

	return(DB_SUCCESS);
}
/* @} */

/*********************************************************************//**
Save an individual index's statistic into the persistent statistics
storage.
dict_stats_save_index_stat() @{
@return DB_SUCCESS or error code */
static
enum db_err
dict_stats_save_index_stat(
/*=======================*/
	dict_index_t*	index,		/*!< in: index */
	lint		stat_timestamp,	/*!< in: timestamp of the stat */
	const char*	stat_name,	/*!< in: name of the stat */
	ib_uint64_t	stat_value,	/*!< in: value of the stat */
	ib_uint64_t*	sample_size,	/*!< in: n pages sampled or NULL */
	const char*	stat_description,/*!< in: description of the stat */
	trx_t*		trx,		/*!< in/out: transaction to use */
	ibool		caller_has_dict_sys_mutex)/*!< in: TRUE if the caller
					owns dict_sys->mutex */
{
	pars_info_t*	pinfo;
	enum db_err	ret;

	pinfo = pars_info_create();

	pars_info_add_literal(pinfo, "database_name", index->table->name,
			      dict_get_db_name_len(index->table->name),
			      DATA_VARCHAR, 0);

	pars_info_add_str_literal(pinfo, "table_name",
				  dict_remove_db_name(index->table->name));

	pars_info_add_str_literal(pinfo, "index_name", index->name);

	pars_info_add_int4_literal(pinfo, "stat_timestamp", stat_timestamp);

	pars_info_add_str_literal(pinfo, "stat_name", stat_name);

	pars_info_add_ull_literal(pinfo, "stat_value", stat_value);

	if (sample_size != NULL) {
		pars_info_add_ull_literal(pinfo, "sample_size", *sample_size);
	} else {
		pars_info_add_literal(pinfo, "sample_size", NULL,
				      UNIV_SQL_NULL, DATA_FIXBINARY, 0);
	}

	pars_info_add_str_literal(pinfo, "stat_description",
				  stat_description);

	ret = que_eval_sql(pinfo,
			   "PROCEDURE INDEX_STATS_SAVE () IS\n"
			   "dummy CHAR;\n"
			   "BEGIN\n"

			   "SELECT database_name INTO dummy\n"
			   "FROM \"" INDEX_STATS_NAME "\"\n"
			   "WHERE\n"
			   "database_name = :database_name AND\n"
			   "table_name = :table_name AND\n"
			   "index_name = :index_name AND\n"
			   "stat_name = :stat_name\n"
			   "FOR UPDATE;\n"

			   "IF (SQL % NOTFOUND) THEN\n"
			   "  INSERT INTO \"" INDEX_STATS_NAME "\"\n"
			   "  VALUES\n"
			   "  (\n"
			   "  :database_name,\n"
			   "  :table_name,\n"
			   "  :index_name,\n"
			   "  :stat_timestamp,\n"
			   "  :stat_name,\n"
			   "  :stat_value,\n"
			   "  :sample_size,\n"
			   "  :stat_description\n"
			   "  );\n"
			   "ELSE\n"
			   "  UPDATE \"" INDEX_STATS_NAME "\" SET\n"
			   "  stat_value = :stat_value,\n"
			   "  sample_size = :sample_size,\n"
			   "  stat_description = :stat_description\n"
			   "  WHERE\n"
			   "  database_name = :database_name AND\n"
			   "  table_name = :table_name AND\n"
			   "  index_name = :index_name AND\n"
			   "  stat_name = :stat_name;\n"
			   "END IF;\n"
			   "END;",
		!caller_has_dict_sys_mutex, trx);

	/* pinfo is freed by que_eval_sql() */

	if (ret != DB_SUCCESS) {
		ut_print_timestamp(stderr);
		fprintf(stderr,
			" InnoDB: Error while trying to save index "
			"statistics for table %s, index %s, "
			"stat name %s: %s\n",
			index->table->name, index->name,
			stat_name, ut_strerr(ret));
	}

	return(ret);
}
/* @} */

/*********************************************************************//**
Save the table's statistics into the persistent statistics storage.
dict_stats_save() @{
@return DB_SUCCESS or error code */
static
enum db_err
dict_stats_save(
/*============*/
	dict_table_t*	table,		/*!< in: table */
	ibool		caller_has_dict_sys_mutex)/*!< in: TRUE if the caller
					owns dict_sys->mutex */
{
	trx_t*		trx;
	pars_info_t*	pinfo;
	dict_index_t*	index;
	lint		now;
	enum db_err	ret;

	/* MySQL's timestamp is 4 byte, so we use
	pars_info_add_int4_literal() which takes a lint arg, so "now" is
	lint */
	now = (lint) ut_time();

	trx = trx_allocate_for_background();

	/* Use 'read-uncommitted' so that the SELECTs we execute
	do not get blocked in case some user has locked the rows we
	are SELECTing */

	trx->isolation_level = TRX_ISO_READ_UNCOMMITTED;

	trx_start_if_not_started(trx);

	pinfo = pars_info_create();

	pars_info_add_literal(pinfo, "database_name", table->name,
			      dict_get_db_name_len(table->name),
			      DATA_VARCHAR, 0);

	pars_info_add_str_literal(pinfo, "table_name",
				  dict_remove_db_name(table->name));

	pars_info_add_int4_literal(pinfo, "stats_timestamp", now);

	pars_info_add_ull_literal(pinfo, "n_rows", table->stat_n_rows);

	pars_info_add_ull_literal(pinfo, "clustered_index_size",
				     table->stat_clustered_index_size);

	pars_info_add_ull_literal(pinfo, "sum_of_other_index_sizes",
				     table->stat_sum_of_other_index_sizes);

	ret = que_eval_sql(pinfo,
			   "PROCEDURE TABLE_STATS_SAVE () IS\n"
			   "dummy CHAR;\n"
			   "BEGIN\n"

			   "SELECT database_name INTO dummy\n"
			   "FROM \"" TABLE_STATS_NAME "\"\n"
			   "WHERE\n"
			   "database_name = :database_name AND\n"
			   "table_name = :table_name\n"
			   "FOR UPDATE;\n"

			   "IF (SQL % NOTFOUND) THEN\n"
			   "  INSERT INTO \"" TABLE_STATS_NAME "\"\n"
			   "  VALUES\n"
			   "  (\n"
			   "  :database_name,\n"
			   "  :table_name,\n"
			   "  :stats_timestamp,\n"
			   "  :n_rows,\n"
			   "  :clustered_index_size,\n"
			   "  :sum_of_other_index_sizes\n"
			   "  );\n"
			   "ELSE\n"
			   "  UPDATE \"" TABLE_STATS_NAME "\" SET\n"
			   "  stats_timestamp = :stats_timestamp,\n"
			   "  n_rows = :n_rows,\n"
			   "  clustered_index_size = :clustered_index_size,\n"
			   "  sum_of_other_index_sizes = "
			   "    :sum_of_other_index_sizes\n"
			   "  WHERE\n"
			   "  database_name = :database_name AND\n"
			   "  table_name = :table_name;\n"
			   "END IF;\n"
			   "END;",
			   !caller_has_dict_sys_mutex, trx);

	/* pinfo is freed by que_eval_sql() */

	if (ret != DB_SUCCESS) {

		ut_print_timestamp(stderr);
		fprintf(stderr,
			" InnoDB: Error while trying to save table "
			"statistics for table %s: %s\n",
			table->name, ut_strerr(ret));

		goto end_rollback;
	}

	for (index = dict_table_get_first_index(table);
	     index != NULL;
	     index = dict_table_get_next_index(index)) {

		ib_uint64_t	stat_n_diff_key_vals[REC_MAX_N_FIELDS];
		ib_uint64_t	stat_n_sample_sizes[REC_MAX_N_FIELDS];
		ulint		n_uniq;
		ulint		i;

		ret = dict_stats_save_index_stat(index, now, "size",
						 index->stat_index_size,
						 NULL,
						 "Number of pages "
						 "in the index",
						 trx,
						 caller_has_dict_sys_mutex);
		if (ret != DB_SUCCESS) {
			goto end_rollback;
		}

		ret = dict_stats_save_index_stat(index, now, "n_leaf_pages",
						 index->stat_n_leaf_pages,
						 NULL,
						 "Number of leaf pages "
						 "in the index",
						 trx,
						 caller_has_dict_sys_mutex);
		if (ret != DB_SUCCESS) {
			goto end_rollback;
		}

		n_uniq = dict_index_get_n_unique(index);

		ut_ad(n_uniq + 1 <= UT_ARR_SIZE(stat_n_diff_key_vals));

		memcpy(stat_n_diff_key_vals, index->stat_n_diff_key_vals,
		       (n_uniq + 1) * sizeof(index->stat_n_diff_key_vals[0]));

		ut_ad(n_uniq + 1 <= UT_ARR_SIZE(stat_n_sample_sizes));

		memcpy(stat_n_sample_sizes, index->stat_n_sample_sizes,
		       (n_uniq + 1) * sizeof(index->stat_n_sample_sizes[0]));

		for (i = 1; i <= n_uniq; i++) {

			char	stat_name[16];
			char	stat_description[1024];
			ulint	j;

			ut_snprintf(stat_name, sizeof(stat_name),
				    "n_diff_pfx%02lu", i);

			/* craft a string that contains the columns names */
			ut_snprintf(stat_description,
				    sizeof(stat_description),
				    "%s", index->fields[0].name);
			for (j = 2; j <= i; j++) {
				size_t	len;

				len = strlen(stat_description);

				ut_snprintf(stat_description + len,
					    sizeof(stat_description) - len,
					    ",%s", index->fields[j - 1].name);
			}

			ret = dict_stats_save_index_stat(
				index, now, stat_name,
				stat_n_diff_key_vals[i],
				&stat_n_sample_sizes[i],
				stat_description, trx,
				caller_has_dict_sys_mutex);

			if (ret != DB_SUCCESS) {
				goto end_rollback;
			}
		}
	}

	trx_commit_for_mysql(trx);
	ret = DB_SUCCESS;
	goto end_free;

end_rollback:

	trx->op_info = "rollback of internal transaction on stats tables";
	trx_rollback_to_savepoint(trx, NULL);
	trx->op_info = "";
	ut_a(trx->error_state == DB_SUCCESS);

end_free:

	trx_free_for_background(trx);

	return(ret);
}
/* @} */

/*********************************************************************//**
Called for the row that is selected by
SELECT ... FROM mysql.innodb_table_stats WHERE table='...'
The second argument is a pointer to the table and the fetched stats are
written to it.
dict_stats_fetch_table_stats_step() @{
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
/* @} */

/** Aux struct used to pass a table and a boolean to
dict_stats_fetch_index_stats_step(). */
typedef struct index_fetch_struct {
	dict_table_t*	table;	/*!< table whose indexes are to be modified */
	ibool		stats_were_modified; /*!< will be set to TRUE if at
				least one index stats were modified */
} index_fetch_t;

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
dict_stats_fetch_index_stats_step() @{
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
			for (index = dict_table_get_first_index(table);
			     index != NULL;
			     index = dict_table_get_next_index(index)) {

				// FIXME: Can we use a better cast operator
				if (strncasecmp(index->name,
						(const char*) data,
						len) == 0) {
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

	if (strncasecmp("size", stat_name, stat_name_len) == 0) {
		index->stat_index_size = (ulint) stat_value;
		arg->stats_were_modified = TRUE;
	} else if (strncasecmp("n_leaf_pages", stat_name, stat_name_len)
		   == 0) {
		index->stat_n_leaf_pages = (ulint) stat_value;
		arg->stats_were_modified = TRUE;
	} else if (strncasecmp(PFX, stat_name,
			       ut_min(strlen(PFX), stat_name_len)) == 0) {

		const char*	num_ptr;
		unsigned long	n_pfx;

		/* point num_ptr into "1" from "n_diff_pfx12..." */
		num_ptr = stat_name + strlen(PFX);

		/* stat_name should have exactly 2 chars appended to PFX
		and they should be digits */
		if (stat_name_len != strlen(PFX) + 2
		    || num_ptr[0] < '0' || num_ptr[0] > '9'
		    || num_ptr[1] < '0' || num_ptr[1] > '9') {

			ut_print_timestamp(stderr);
			fprintf(stderr,
				" InnoDB: Ignoring strange row from "
				"%s WHERE "
				"database_name = '%.*s' AND "
				"table_name = '%s' AND "
				"index_name = '%s' AND "
				"stat_name = '%.*s'; because stat_name "
				"is malformed\n",
				INDEX_STATS_NAME_PRINT,
				(int) dict_get_db_name_len(table->name),
				table->name,
				dict_remove_db_name(table->name),
				index->name,
				(int) stat_name_len,
				stat_name);
			return(TRUE);
		}
		/* else */

		/* extract 12 from "n_diff_pfx12..." into n_pfx
		note that stat_name does not have a terminating '\0' */
		n_pfx = (num_ptr[0] - '0') * 10 + (num_ptr[1] - '0');

		if (n_pfx == 0 || n_pfx > dict_index_get_n_unique(index)) {

			ut_print_timestamp(stderr);
			fprintf(stderr,
				" InnoDB: Ignoring strange row from "
				"%s WHERE "
				"database_name = '%.*s' AND "
				"table_name = '%s' AND "
				"index_name = '%s' AND "
				"stat_name = '%.*s'; because stat_name is "
				"out of range, the index has %lu unique "
				"columns\n",
				INDEX_STATS_NAME_PRINT,
				(int) dict_get_db_name_len(table->name),
				table->name,
				dict_remove_db_name(table->name),
				index->name,
				(int) stat_name_len,
				stat_name,
				dict_index_get_n_unique(index));
			return(TRUE);
		}
		/* else */

		index->stat_n_diff_key_vals[n_pfx] = stat_value;

		if (sample_size != UINT64_UNDEFINED) {
			index->stat_n_sample_sizes[n_pfx] = sample_size;
		} else {
			/* hmm, strange... the user must have UPDATEd the
			table manually and SET sample_size = NULL */
			index->stat_n_sample_sizes[n_pfx] = 0;
		}

		arg->stats_were_modified = TRUE;
	} else {
		/* silently ignore rows with unknown stat_name, the
		user may have developed her own stats */
	}

	/* XXX this is not used but returning non-NULL is necessary */
	return(TRUE);
}
/* @} */

/*********************************************************************//**
Read table's statistics from the persistent statistics storage.
dict_stats_fetch_from_ps() @{
@return DB_SUCCESS or error code */
static
enum db_err
dict_stats_fetch_from_ps(
/*=====================*/
	dict_table_t*	table,		/*!< in/out: table */
	ibool		caller_has_dict_sys_mutex)/*!< in: TRUE if the caller
					owns dict_sys->mutex */
{
	index_fetch_t	index_fetch_arg;
	trx_t*		trx;
	pars_info_t*	pinfo;
	enum db_err	ret;

	ut_ad(mutex_own(&dict_sys->mutex) == caller_has_dict_sys_mutex);

	trx = trx_allocate_for_background();

	/* Use 'read-uncommitted' so that the SELECTs we execute
	do not get blocked in case some user has locked the rows we
	are SELECTing */

	trx->isolation_level = TRX_ISO_READ_UNCOMMITTED;

	trx_start_if_not_started(trx);

	pinfo = pars_info_create();

	pars_info_add_literal(pinfo, "database_name", table->name,
			      dict_get_db_name_len(table->name),
			      DATA_VARCHAR, 0);

	pars_info_add_str_literal(pinfo, "table_name",
				  dict_remove_db_name(table->name));

	pars_info_bind_function(pinfo,
			       "fetch_table_stats_step",
			       dict_stats_fetch_table_stats_step,
			       table);

	index_fetch_arg.table = table;
	index_fetch_arg.stats_were_modified = FALSE;
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
			   !caller_has_dict_sys_mutex, trx);

	/* pinfo is freed by que_eval_sql() */

	/* XXX If mysql.innodb_index_stats contained less rows than the number
	of indexes in the table, then some of the indexes of the table
	were left uninitialized. Currently this is ignored and those
	indexes are left with uninitialized stats until ANALYZE TABLE is
	run. This condition happens when the user creates a new index
	on a table. We could return DB_STATS_DO_NOT_EXIST from here,
	forcing the usage of transient stats until mysql.innodb_index_stats
	is complete. */

	trx_commit_for_mysql(trx);

	trx_free_for_background(trx);

	if (!index_fetch_arg.stats_were_modified) {
		return(DB_STATS_DO_NOT_EXIST);
	}

	return(ret);
}
/* @} */

/*********************************************************************//**
Calculates new estimates for table and index statistics. The statistics
are used in query optimization.
dict_stats_update() @{
@return DB_* error code or DB_SUCCESS */
UNIV_INTERN
enum db_err
dict_stats_update(
/*==============*/
	dict_table_t*		table,	/*!< in/out: table */
	dict_stats_upd_option_t	stats_upd_option,
					/*!< in: whether to (re)calc
					the stats or to fetch them from
					the persistent statistics
					storage */
	ibool			caller_has_dict_sys_mutex)
					/*!< in: TRUE if the caller
					owns dict_sys->mutex */
{
	enum db_err	ret = DB_ERROR;

	/* check whether caller_has_dict_sys_mutex is set correctly;
	note that mutex_own() is not implemented in non-debug code so
	we cannot avoid having this extra param to the current function */
	ut_ad(caller_has_dict_sys_mutex
	      ? mutex_own(&dict_sys->mutex)
	      : !mutex_own(&dict_sys->mutex));

	if (table->ibd_file_missing) {
		ut_print_timestamp(stderr);
		fprintf(stderr,
			"  InnoDB: cannot calculate statistics for table %s\n"
			"InnoDB: because the .ibd file is missing.  For help,"
			" please refer to\n"
			"InnoDB: " REFMAN "innodb-troubleshooting.html\n",
			table->name);

		return(DB_TABLESPACE_DELETED);
	}

	/* If we have set a high innodb_force_recovery level, do not calculate
	statistics, as a badly corrupted index can cause a crash in it. */

	if (srv_force_recovery >= SRV_FORCE_NO_IBUF_MERGE) {

		return(DB_SUCCESS);
	}

	switch (stats_upd_option) {
	case DICT_STATS_RECALC_PERSISTENT:
	case DICT_STATS_RECALC_PERSISTENT_SILENT:
		/* Persistent recalculation requested, called from
		ANALYZE TABLE or from TRUNCATE TABLE */

		/* FTS auxiliary tables do not need persistent stats */
		if ((ut_strcount(table->name, "FTS") > 0
		&& (ut_strcount(table->name, "CONFIG") > 0
		    || ut_strcount(table->name, "INDEX") > 0
		    || ut_strcount(table->name, "DELETED") > 0
		    || ut_strcount(table->name, "DOC_ID") > 0
		    || ut_strcount(table->name, "ADDED") > 0))) {
			goto transient;
		}

		/* check if the persistent statistics storage exists
		before calling the potentially slow function
		dict_stats_update_persistent(); that is a
		prerequisite for dict_stats_save() succeeding */
		if (dict_stats_persistent_storage_check(
				caller_has_dict_sys_mutex)) {

			dict_table_stats_lock(table, RW_X_LATCH);

			ret = dict_stats_update_persistent(table);

			/* XXX Currently dict_stats_save() would read the
			stats from the table without dict_table_stats_lock()
			which means it could save inconsistent data on the
			disk. This is because we must call
			dict_table_stats_lock() after locking dict_sys->mutex.
			A solution is to copy here the stats to a temporary
			buffer while holding the _stats_lock(), release it,
			and pass that buffer to dict_stats_save(). */

			dict_table_stats_unlock(table, RW_X_LATCH);

			if (ret == DB_SUCCESS) {
				ret = dict_stats_save(
					table,
					caller_has_dict_sys_mutex);
			}

			return(ret);
		}
		/* else */

		/* Fall back to transient stats since the persistent
		storage is not present or is corrupted */

		if (stats_upd_option == DICT_STATS_RECALC_PERSISTENT) {

			ut_print_timestamp(stderr);
			/* XXX add link to the doc about storage
			creation */
			fprintf(stderr,
				" InnoDB: Recalculation of persistent "
				"statistics requested but the required "
				"persistent statistics storage is not "
				"present or is corrupted. "
				"Using quick transient stats "
				"instead.\n");
		}

		goto transient;

	case DICT_STATS_RECALC_TRANSIENT:

		goto transient;

	case DICT_STATS_FETCH:
	case DICT_STATS_FETCH_ONLY_IF_NOT_IN_MEMORY:
		/* fetch requested, either fetch from persistent statistics
		storage or use the old method */

		dict_table_stats_lock(table, RW_X_LATCH);

		if (stats_upd_option == DICT_STATS_FETCH_ONLY_IF_NOT_IN_MEMORY
		    && table->stat_initialized) {

			dict_table_stats_unlock(table, RW_X_LATCH);
			return(DB_SUCCESS);
		}
		/* else */

		/* Must unlock because otherwise there is a lock order
		violation with dict_sys->mutex below. Declare stats to be
		initialized before unlocking. */
		table->stat_initialized = TRUE;
		dict_table_stats_unlock(table, RW_X_LATCH);

		if (strchr(table->name, '/') == NULL
		    || strcmp(table->name, INDEX_STATS_NAME) == 0
		    || strcmp(table->name, TABLE_STATS_NAME) == 0
		    || (ut_strcount(table->name, "FTS") > 0
		        && (ut_strcount(table->name, "CONFIG") > 0
			    || ut_strcount(table->name, "INDEX") > 0
			    || ut_strcount(table->name, "DELETED") > 0
			    || ut_strcount(table->name, "DOC_ID") > 0
			    || ut_strcount(table->name, "ADDED") > 0))) {
			/* Use the quick transient stats method for
			InnoDB internal tables, because we know the
			persistent stats storage does not contain data
			for them */

			goto transient;
		}
		/* else */

		if (dict_stats_persistent_storage_check(
			caller_has_dict_sys_mutex)) {

			ret = dict_stats_fetch_from_ps(table,
				caller_has_dict_sys_mutex);

			if (ret == DB_STATS_DO_NOT_EXIST
			    || (ret != DB_SUCCESS && stats_upd_option
				== DICT_STATS_FETCH_ONLY_IF_NOT_IN_MEMORY)) {
				/* Stats for this particular table do not
				exist or we have been called from open table
				which needs to initialize the stats,
				calculate the quick transient statistics */
				goto transient;
			}
			/* else */

			return(ret);
		} else {
			/* persistent statistics storage does not exist,
			calculate the transient stats */
			goto transient;
		}

		break;

	/* no "default:" in order to produce a compilation warning
	about unhandled enumeration value */
	}

transient:

	dict_table_stats_lock(table, RW_X_LATCH);

	dict_stats_update_transient(table);

	dict_table_stats_unlock(table, RW_X_LATCH);

	return(DB_SUCCESS);
}
/* @} */

/*********************************************************************//**
Close the stats tables. Should always be called after successful
dict_stats_open(). It will free the dict_stats handle.
dict_stats_close() @{ */
UNIV_INLINE
void
dict_stats_close(
/*=============*/
	dict_stats_t*	dict_stats)	/*!< in/own: Handle to open
					statistics tables */
{
	if (dict_stats->table_stats != NULL) {
		dict_table_close(dict_stats->table_stats, FALSE);
		dict_stats->table_stats = NULL;
	}

	if (dict_stats->index_stats != NULL) {
		dict_table_close(dict_stats->index_stats, FALSE);
		dict_stats->index_stats = NULL;
	}

	mem_free(dict_stats);
}
/* @} */

/*********************************************************************//**
Open stats tables to prevent these tables from being DROPped.
Also check whether they have the correct structure. The caller
must call dict_stats_close() when he has finished DMLing the tables.
dict_stats_open() @{
@return pointer to open tables or NULL on failure */
UNIV_INLINE
dict_stats_t*
dict_stats_open(void)
/*=================*/
{
	dict_stats_t*	dict_stats;

	dict_stats = static_cast<dict_stats_t*>(
		mem_zalloc(sizeof(*dict_stats)));

	dict_stats->table_stats = dict_table_open_on_name_no_stats(
		TABLE_STATS_NAME, FALSE, DICT_ERR_IGNORE_NONE);

	dict_stats->index_stats = dict_table_open_on_name_no_stats(
		INDEX_STATS_NAME, FALSE, DICT_ERR_IGNORE_NONE);

	/* Check if the tables have the correct structure, if yes then
	after this function we can safely DELETE from them without worrying
	that they may get DROPped or DDLed because the open will have
	increased the reference count. */

	if (dict_stats->table_stats == NULL
	    || dict_stats->index_stats == NULL
	    || !dict_stats_persistent_storage_check(FALSE)) {

		/* There was an error, close the tables and free the handle. */
		dict_stats_close(dict_stats);
		dict_stats = NULL;
	}

	return(dict_stats);
}
/* @} */

/*********************************************************************//**
Removes the information for a particular index's stats from the persistent
storage if it exists and if there is data stored for this index.
The transaction is not committed, it must not be committed in this
function because this is the user trx that is running DROP INDEX.
The transaction will be committed at the very end when dropping an
index.
A note from Marko why we cannot edit user and sys_* tables in one trx:
marko: The problem is that ibuf merges should be disabled while we are
rolling back dict transactions.
marko: If ibuf merges are not disabled, we need to scan the *.ibd files.
But we shouldn't open *.ibd files before we have rolled back dict
transactions and opened the SYS_* records for the *.ibd files.
dict_stats_delete_index_stats() @{
@return DB_SUCCESS or error code */
UNIV_INTERN
enum db_err
dict_stats_delete_index_stats(
/*==========================*/
	dict_index_t*	index,	/*!< in: index */
	trx_t*		trx,	/*!< in: transaction to use */
	char*		errstr, /*!< out: error message if != DB_SUCCESS
				is returned */
	ulint		errstr_sz)/*!< in: size of the errstr buffer */
{
	char		database_name[MAX_DATABASE_NAME_LEN + 1];
	const char*	table_name;
	pars_info_t*	pinfo;
	enum db_err	ret;
	dict_stats_t*	dict_stats;
	void*		mysql_thd = trx->mysql_thd;

	/* skip indexes whose table names do not contain a database name
	e.g. if we are dropping an index from SYS_TABLES */
	if (strchr(index->table_name, '/') == NULL) {

		return(DB_SUCCESS);
	}

	/* Increment table reference count to prevent the tables from
	being DROPped just before que_eval_sql(). */
	dict_stats = dict_stats_open();

	if (dict_stats == NULL) {
		/* stats tables do not exist or have unexpected structure */
		return(DB_SUCCESS);
	}

	/* the stats tables cannot be DROPped now */

	ut_snprintf(database_name, sizeof(database_name), "%.*s",
		    (int) dict_get_db_name_len(index->table_name),
		    index->table_name);

	table_name = dict_remove_db_name(index->table_name);

	pinfo = pars_info_create();

	pars_info_add_str_literal(pinfo, "database_name", database_name);

	pars_info_add_str_literal(pinfo, "table_name", table_name);

	pars_info_add_str_literal(pinfo, "index_name", index->name);

	/* Force lock wait timeout to be instantaneous because the incoming
	transaction was created via MySQL. */

	mysql_thd = trx->mysql_thd;
	trx->mysql_thd = NULL;

	ret = que_eval_sql(pinfo,
			   "PROCEDURE DROP_INDEX_STATS () IS\n"
			   "BEGIN\n"
			   "DELETE FROM \"" INDEX_STATS_NAME "\" WHERE\n"
			   "database_name = :database_name AND\n"
			   "table_name = :table_name AND\n"
			   "index_name = :index_name;\n"
			   "END;\n",
			   TRUE,
			   trx);

	trx->mysql_thd = mysql_thd;

	/* pinfo is freed by que_eval_sql() */

	/* do not to commit here, see the function's comment */

	if (ret != DB_SUCCESS) {

		ut_snprintf(errstr, errstr_sz,
			    "Unable to delete statistics for index %s "
			    "from %s%s. They can be deleted later using "
			    "DELETE FROM %s WHERE "
			    "database_name = '%s' AND "
			    "table_name = '%s' AND "
			    "index_name = '%s';",
			    index->name,
			    INDEX_STATS_NAME_PRINT,
			    (ret == DB_LOCK_WAIT_TIMEOUT
			     ? " because the rows are locked"
			     : ""),
			    INDEX_STATS_NAME_PRINT,
			    database_name,
			    table_name,
			    index->name);

		ut_print_timestamp(stderr);
		fprintf(stderr, " InnoDB: %s\n", errstr);
	}

	dict_stats_close(dict_stats);

	return(ret);
}
/* @} */

/*********************************************************************//**
Removes the statistics for a table and all of its indexes from the
persistent statistics storage if it exists and if there is data stored for
the table.  This function creates its own transaction and commits it.
dict_stats_delete_table_stats() @{
@return DB_SUCCESS or error code */
UNIV_INTERN
enum db_err
dict_stats_delete_table_stats(
/*==========================*/
	const char*	table_name,	/*!< in: table name */
	char*		errstr,		/*!< out: error message
					if != DB_SUCCESS is returned */
	ulint		errstr_sz)	/*!< in: size of errstr buffer */
{
	char		database_name[MAX_DATABASE_NAME_LEN + 1];
	const char*	table_name_strip; /* without leading db name */
	trx_t*		trx;
	pars_info_t*	pinfo;
	enum db_err	ret = DB_ERROR;
	dict_stats_t*	dict_stats;

	/* skip tables that do not contain a database name
	e.g. if we are dropping SYS_TABLES */
	if (strchr(table_name, '/') == NULL) {

		return(DB_SUCCESS);
	}

	/* skip innodb_table_stats and innodb_index_stats themselves */
	if (strcmp(table_name, TABLE_STATS_NAME) == 0
	    || strcmp(table_name, INDEX_STATS_NAME) == 0) {

		return(DB_SUCCESS);
	}

	/* Create a new private trx */

	trx = trx_allocate_for_background();

	/* Use 'read-uncommitted' so that the SELECTs we execute
	do not get blocked in case some user has locked the rows we
	are SELECTing */

	trx->isolation_level = TRX_ISO_READ_UNCOMMITTED;

	trx_start_if_not_started(trx);

	/* Increment table reference count to prevent the tables from
	being DROPped just before que_eval_sql(). */
	dict_stats = dict_stats_open();

	if (dict_stats == NULL) {
		/* stats tables do not exist or have unexpected structure */
		ret = DB_SUCCESS;
		goto commit_and_return;
	}

	ut_snprintf(database_name, sizeof(database_name), "%.*s",
		    (int) dict_get_db_name_len(table_name),
		    table_name);

	table_name_strip = dict_remove_db_name(table_name);

	pinfo = pars_info_create();

	pars_info_add_str_literal(pinfo, "database_name", database_name);

	pars_info_add_str_literal(pinfo, "table_name", table_name_strip);

	ret = que_eval_sql(pinfo,
			   "PROCEDURE DROP_TABLE_STATS () IS\n"
			   "BEGIN\n"

			   "DELETE FROM \"" INDEX_STATS_NAME "\" WHERE\n"
			   "database_name = :database_name AND\n"
			   "table_name = :table_name;\n"

			   "DELETE FROM \"" TABLE_STATS_NAME "\" WHERE\n"
			   "database_name = :database_name AND\n"
			   "table_name = :table_name;\n"

			   "END;\n",
			   TRUE,
			   trx);

	/* pinfo is freed by que_eval_sql() */

	if (ret != DB_SUCCESS) {

		ut_snprintf(errstr, errstr_sz,
			    "Unable to delete statistics for table %s.%s "
			    "from %s or %s%s. "
			    "They can be deleted later using "

			    "DELETE FROM %s WHERE "
			    "database_name = '%s' AND "
			    "table_name = '%s'; "

			    "DELETE FROM %s WHERE "
			    "database_name = '%s' AND "
			    "table_name = '%s';",

			    database_name, table_name_strip,
			    TABLE_STATS_NAME_PRINT, INDEX_STATS_NAME_PRINT,

			    (ret == DB_LOCK_WAIT_TIMEOUT
			     ? " because the rows are locked"
			     : ""),

			    INDEX_STATS_NAME_PRINT,
			    database_name, table_name_strip,

			    TABLE_STATS_NAME_PRINT,
			    database_name, table_name_strip);

		ut_print_timestamp(stderr);
		fprintf(stderr, " InnoDB: %s\n", errstr);
	}

	dict_stats_close(dict_stats);

commit_and_return:

	trx_commit_for_mysql(trx);

	trx_free_for_background(trx);

	return(ret);
}
/* @} */

/* tests @{ */
#ifdef UNIV_COMPILE_TEST_FUNCS

/* The following unit tests test some of the functions in this file
individually, such testing cannot be performed by the mysql-test framework
via SQL. */

/* test_dict_table_schema_check() @{ */
void
test_dict_table_schema_check()
{
	/*
	CREATE TABLE tcheck (
		c01 VARCHAR(123),
		c02 INT,
		c03 INT NOT NULL,
		c04 INT UNSIGNED,
		c05 BIGINT,
		c06 BIGINT UNSIGNED NOT NULL,
		c07 TIMESTAMP
	) ENGINE=INNODB;
	*/
	/* definition for the table 'test/tcheck' */
	dict_col_meta_t	columns[] = {
		{"c01", DATA_VARCHAR, 0, 123},
		{"c02", DATA_INT, 0, 4},
		{"c03", DATA_INT, DATA_NOT_NULL, 4},
		{"c04", DATA_INT, DATA_UNSIGNED, 4},
		{"c05", DATA_INT, 0, 8},
		{"c06", DATA_INT, DATA_NOT_NULL | DATA_UNSIGNED, 8},
		{"c07", DATA_INT, 0, 4},
		{"c_extra", DATA_INT, 0, 4}
	};
	dict_table_schema_t	schema = {
		"test/tcheck",
		0 /* will be set individually for each test below */,
		columns
	};
	char	errstr[512];

	ut_snprintf(errstr, sizeof(errstr), "Table not found");

	/* prevent any data dictionary modifications while we are checking
	the tables' structure */

	mutex_enter(&(dict_sys->mutex));

	/* check that a valid table is reported as valid */
	schema.n_cols = 7;
	if (dict_table_schema_check(&schema, errstr, sizeof(errstr))
	    == DB_SUCCESS) {
		printf("OK: test.tcheck ok\n");
	} else {
		printf("ERROR: %s\n", errstr);
		printf("ERROR: test.tcheck not present or corrupted\n");
		goto test_dict_table_schema_check_end;
	}

	/* check columns with wrong length */
	schema.columns[1].len = 8;
	if (dict_table_schema_check(&schema, errstr, sizeof(errstr))
	    != DB_SUCCESS) {
		printf("OK: test.tcheck.c02 has different length and is "
		       "reported as corrupted\n");
	} else {
		printf("OK: test.tcheck.c02 has different length but is "
		       "reported as ok\n");
		goto test_dict_table_schema_check_end;
	}
	schema.columns[1].len = 4;

	/* request that c02 is NOT NULL while actually it does not have
	this flag set */
	schema.columns[1].prtype_mask |= DATA_NOT_NULL;
	if (dict_table_schema_check(&schema, errstr, sizeof(errstr))
	    != DB_SUCCESS) {
		printf("OK: test.tcheck.c02 does not have NOT NULL while "
		       "it should and is reported as corrupted\n");
	} else {
		printf("ERROR: test.tcheck.c02 does not have NOT NULL while "
		       "it should and is not reported as corrupted\n");
		goto test_dict_table_schema_check_end;
	}
	schema.columns[1].prtype_mask &= ~DATA_NOT_NULL;

	/* check a table that contains some extra columns */
	schema.n_cols = 6;
	if (dict_table_schema_check(&schema, errstr, sizeof(errstr))
	    == DB_SUCCESS) {
		printf("ERROR: test.tcheck has more columns but is not "
		       "reported as corrupted\n");
		goto test_dict_table_schema_check_end;
	} else {
		printf("OK: test.tcheck has more columns and is "
		       "reported as corrupted\n");
	}

	/* check a table that has some columns missing */
	schema.n_cols = 8;
	if (dict_table_schema_check(&schema, errstr, sizeof(errstr))
	    != DB_SUCCESS) {
		printf("OK: test.tcheck has missing columns and is "
		       "reported as corrupted\n");
	} else {
		printf("ERROR: test.tcheck has missing columns but is "
		       "reported as ok\n");
		goto test_dict_table_schema_check_end;
	}

	/* check non-existent table */
	schema.table_name = "test/tcheck_nonexistent";
	if (dict_table_schema_check(&schema, errstr, sizeof(errstr))
	    != DB_SUCCESS) {
		printf("OK: test.tcheck_nonexistent is not present\n");
	} else {
		printf("ERROR: test.tcheck_nonexistent is present!?\n");
		goto test_dict_table_schema_check_end;
	}

test_dict_table_schema_check_end:

	mutex_exit(&(dict_sys->mutex));
}
/* @} */

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
	ib_uint64_t	index1_stat_n_diff_key_vals[2];
	ib_uint64_t	index1_stat_n_sample_sizes[2];
	dict_index_t	index2;
	dict_field_t	index2_fields[4];
	ib_uint64_t	index2_stat_n_diff_key_vals[5];
	ib_uint64_t	index2_stat_n_sample_sizes[5];
	enum db_err	ret;

	/* craft a dummy dict_table_t */
	table.name = (char*) (TEST_DATABASE_NAME "/" TEST_TABLE_NAME);
	table.stat_n_rows = TEST_N_ROWS;
	table.stat_clustered_index_size = TEST_CLUSTERED_INDEX_SIZE;
	table.stat_sum_of_other_index_sizes = TEST_SUM_OF_OTHER_INDEX_SIZES;
	UT_LIST_INIT(table.indexes);
	UT_LIST_ADD_LAST(indexes, table.indexes, &index1);
	UT_LIST_ADD_LAST(indexes, table.indexes, &index2);
#ifdef UNIV_DEBUG
	table.magic_n = DICT_TABLE_MAGIC_N;
#endif /* UNIV_DEBUG */

	index1.name = TEST_IDX1_NAME;
	index1.table = &table;
#ifdef UNIV_DEBUG
	index1.magic_n = DICT_INDEX_MAGIC_N;
#endif /* UNIV_DEBUG */
	index1.to_be_dropped = 0;
	index1.cached = 1;
	index1.n_uniq = 1;
	index1.fields = index1_fields;
	index1.stat_n_diff_key_vals = index1_stat_n_diff_key_vals;
	index1.stat_n_sample_sizes = index1_stat_n_sample_sizes;
	index1.stat_index_size = TEST_IDX1_INDEX_SIZE;
	index1.stat_n_leaf_pages = TEST_IDX1_N_LEAF_PAGES;
	index1_fields[0].name = TEST_IDX1_COL1_NAME;
	index1_stat_n_diff_key_vals[0] = 1; /* dummy */
	index1_stat_n_diff_key_vals[1] = TEST_IDX1_N_DIFF1;
	index1_stat_n_sample_sizes[0] = 0; /* dummy */
	index1_stat_n_sample_sizes[1] = TEST_IDX1_N_DIFF1_SAMPLE_SIZE;

	index2.name = TEST_IDX2_NAME;
	index2.table = &table;
#ifdef UNIV_DEBUG
	index2.magic_n = DICT_INDEX_MAGIC_N;
#endif /* UNIV_DEBUG */
	index2.to_be_dropped = 0;
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
	index2_stat_n_diff_key_vals[0] = 1; /* dummy */
	index2_stat_n_diff_key_vals[1] = TEST_IDX2_N_DIFF1;
	index2_stat_n_diff_key_vals[2] = TEST_IDX2_N_DIFF2;
	index2_stat_n_diff_key_vals[3] = TEST_IDX2_N_DIFF3;
	index2_stat_n_diff_key_vals[4] = TEST_IDX2_N_DIFF4;
	index2_stat_n_sample_sizes[0] = 0; /* dummy */
	index2_stat_n_sample_sizes[1] = TEST_IDX2_N_DIFF1_SAMPLE_SIZE;
	index2_stat_n_sample_sizes[2] = TEST_IDX2_N_DIFF2_SAMPLE_SIZE;
	index2_stat_n_sample_sizes[3] = TEST_IDX2_N_DIFF3_SAMPLE_SIZE;
	index2_stat_n_sample_sizes[4] = TEST_IDX2_N_DIFF4_SAMPLE_SIZE;

	ret = dict_stats_save(&table, FALSE);

	ut_a(ret == DB_SUCCESS);

	printf("\nOK: stats saved successfully, now go ahead and read "
	       "what's inside %s and %s:\n\n",
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
	ib_uint64_t	index1_stat_n_diff_key_vals[2];
	ib_uint64_t	index1_stat_n_sample_sizes[2];
	dict_index_t	index2;
	ib_uint64_t	index2_stat_n_diff_key_vals[5];
	ib_uint64_t	index2_stat_n_sample_sizes[5];
	enum db_err	ret;

	/* craft a dummy dict_table_t */
	table.name = (char*) (TEST_DATABASE_NAME "/" TEST_TABLE_NAME);
	UT_LIST_INIT(table.indexes);
	UT_LIST_ADD_LAST(indexes, table.indexes, &index1);
	UT_LIST_ADD_LAST(indexes, table.indexes, &index2);
#ifdef UNIV_DEBUG
	table.magic_n = DICT_TABLE_MAGIC_N;
#endif /* UNIV_DEBUG */

	index1.name = TEST_IDX1_NAME;
#ifdef UNIV_DEBUG
	index1.magic_n = DICT_INDEX_MAGIC_N;
#endif /* UNIV_DEBUG */
	index1.cached = 1;
	index1.n_uniq = 1;
	index1.stat_n_diff_key_vals = index1_stat_n_diff_key_vals;
	index1.stat_n_sample_sizes = index1_stat_n_sample_sizes;

	index2.name = TEST_IDX2_NAME;
#ifdef UNIV_DEBUG
	index2.magic_n = DICT_INDEX_MAGIC_N;
#endif /* UNIV_DEBUG */
	index2.cached = 1;
	index2.n_uniq = 4;
	index2.stat_n_diff_key_vals = index2_stat_n_diff_key_vals;
	index2.stat_n_sample_sizes = index2_stat_n_sample_sizes;

	ret = dict_stats_fetch_from_ps(&table, FALSE);

	ut_a(ret == DB_SUCCESS);

	ut_a(table.stat_n_rows == TEST_N_ROWS);
	ut_a(table.stat_clustered_index_size == TEST_CLUSTERED_INDEX_SIZE);
	ut_a(table.stat_sum_of_other_index_sizes
	     == TEST_SUM_OF_OTHER_INDEX_SIZES);

	ut_a(index1.stat_index_size == TEST_IDX1_INDEX_SIZE);
	ut_a(index1.stat_n_leaf_pages == TEST_IDX1_N_LEAF_PAGES);
	ut_a(index1_stat_n_diff_key_vals[1] == TEST_IDX1_N_DIFF1);
	ut_a(index1_stat_n_sample_sizes[1] == TEST_IDX1_N_DIFF1_SAMPLE_SIZE);

	ut_a(index2.stat_index_size == TEST_IDX2_INDEX_SIZE);
	ut_a(index2.stat_n_leaf_pages == TEST_IDX2_N_LEAF_PAGES);
	ut_a(index2_stat_n_diff_key_vals[1] == TEST_IDX2_N_DIFF1);
	ut_a(index2_stat_n_sample_sizes[1] == TEST_IDX2_N_DIFF1_SAMPLE_SIZE);
	ut_a(index2_stat_n_diff_key_vals[2] == TEST_IDX2_N_DIFF2);
	ut_a(index2_stat_n_sample_sizes[2] == TEST_IDX2_N_DIFF2_SAMPLE_SIZE);
	ut_a(index2_stat_n_diff_key_vals[3] == TEST_IDX2_N_DIFF3);
	ut_a(index2_stat_n_sample_sizes[3] == TEST_IDX2_N_DIFF3_SAMPLE_SIZE);
	ut_a(index2_stat_n_diff_key_vals[4] == TEST_IDX2_N_DIFF4);
	ut_a(index2_stat_n_sample_sizes[4] == TEST_IDX2_N_DIFF4_SAMPLE_SIZE);

	printf("OK: fetch successful\n");
}
/* @} */

/* test_dict_stats_all() @{ */
void
test_dict_stats_all()
{
	test_dict_table_schema_check();

	test_dict_stats_save();

	test_dict_stats_fetch_from_ps();
}
/* @} */

#endif /* UNIV_COMPILE_TEST_FUNCS */
/* @} */

#endif /* UNIV_HOTBACKUP */

/* vim: set foldmethod=marker foldmarker=@{,@}: */
