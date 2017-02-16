/*****************************************************************************

Copyright (c) 1996, 2012, Oracle and/or its affiliates. All Rights Reserved.

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
@file row/row0row.c
General row routines

Created 4/20/1996 Heikki Tuuri
*******************************************************/

#include "row0row.h"

#ifdef UNIV_NONINL
#include "row0row.ic"
#endif

#include "data0type.h"
#include "dict0dict.h"
#include "btr0btr.h"
#include "ha_prototypes.h"
#include "mach0data.h"
#include "trx0rseg.h"
#include "trx0trx.h"
#include "trx0roll.h"
#include "trx0undo.h"
#include "trx0purge.h"
#include "trx0rec.h"
#include "que0que.h"
#include "row0ext.h"
#include "row0upd.h"
#include "rem0cmp.h"
#include "read0read.h"
#include "ut0mem.h"

/*****************************************************************//**
When an insert or purge to a table is performed, this function builds
the entry to be inserted into or purged from an index on the table.
@return index entry which should be inserted or purged, or NULL if the
externally stored columns in the clustered index record are
unavailable and ext != NULL */
UNIV_INTERN
dtuple_t*
row_build_index_entry(
/*==================*/
	const dtuple_t*	row,	/*!< in: row which should be
				inserted or purged */
	row_ext_t*	ext,	/*!< in: externally stored column prefixes,
				or NULL */
	dict_index_t*	index,	/*!< in: index on the table */
	mem_heap_t*	heap)	/*!< in: memory heap from which the memory for
				the index entry is allocated */
{
	dtuple_t*	entry;
	ulint		entry_len;
	ulint		i;

	ut_ad(row && index && heap);
	ut_ad(dtuple_check_typed(row));

	entry_len = dict_index_get_n_fields(index);
	entry = dtuple_create(heap, entry_len);

	if (UNIV_UNLIKELY(index->type & DICT_UNIVERSAL)) {
		dtuple_set_n_fields_cmp(entry, entry_len);
		/* There may only be externally stored columns
		in a clustered index B-tree of a user table. */
		ut_a(!ext);
	} else {
		dtuple_set_n_fields_cmp(
			entry, dict_index_get_n_unique_in_tree(index));
	}

	for (i = 0; i < entry_len; i++) {
		const dict_field_t*	ind_field
			= dict_index_get_nth_field(index, i);
		const dict_col_t*	col
			= ind_field->col;
		ulint			col_no
			= dict_col_get_no(col);
		dfield_t*		dfield
			= dtuple_get_nth_field(entry, i);
		const dfield_t*		dfield2
			= dtuple_get_nth_field(row, col_no);
		ulint			len
			= dfield_get_len(dfield2);

		dfield_copy(dfield, dfield2);

		if (dfield_is_null(dfield)) {
			continue;
		}

		if (ind_field->prefix_len == 0
		    && (!dfield_is_ext(dfield)
			|| dict_index_is_clust(index))) {
			/* The dfield_copy() above suffices for
			columns that are stored in-page, or for
			clustered index record columns that are not
			part of a column prefix in the PRIMARY KEY. */
			continue;
		}

		/* If the column is stored externally (off-page) in
		the clustered index, it must be an ordering field in
		the secondary index.  In the Antelope format, only
		prefix-indexed columns may be stored off-page in the
		clustered index record. In the Barracuda format, also
		fully indexed long CHAR or VARCHAR columns may be
		stored off-page. */
		ut_ad(col->ord_part);

		if (UNIV_LIKELY_NULL(ext)) {
			/* See if the column is stored externally. */
			const byte*	buf = row_ext_lookup(ext, col_no,
							     &len);
			if (UNIV_LIKELY_NULL(buf)) {
				if (UNIV_UNLIKELY(buf == field_ref_zero)) {
					return(NULL);
				}
				dfield_set_data(dfield, buf, len);
			}

			if (ind_field->prefix_len == 0) {
				/* In the Barracuda format
				(ROW_FORMAT=DYNAMIC or
				ROW_FORMAT=COMPRESSED), we can have a
				secondary index on an entire column
				that is stored off-page in the
				clustered index. As this is not a
				prefix index (prefix_len == 0),
				include the entire off-page column in
				the secondary index record. */
				continue;
			}
		} else if (dfield_is_ext(dfield)) {
			/* This table is either in Antelope format
			(ROW_FORMAT=REDUNDANT or ROW_FORMAT=COMPACT)
			or a purge record where the ordered part of
			the field is not external.
			In Antelope, the maximum column prefix
			index length is 767 bytes, and the clustered
			index record contains a 768-byte prefix of
			each off-page column. */
			ut_a(len >= BTR_EXTERN_FIELD_REF_SIZE);
			len -= BTR_EXTERN_FIELD_REF_SIZE;
			dfield_set_len(dfield, len);
		}

		/* If a column prefix index, take only the prefix. */
		if (ind_field->prefix_len) {
			len = dtype_get_at_most_n_mbchars(
				col->prtype, col->mbminmaxlen,
				ind_field->prefix_len, len,
				dfield_get_data(dfield));
			dfield_set_len(dfield, len);
		}
	}

	ut_ad(dtuple_check_typed(entry));

	return(entry);
}

/*******************************************************************//**
An inverse function to row_build_index_entry. Builds a row from a
record in a clustered index.
@return	own: row built; see the NOTE below! */
UNIV_INTERN
dtuple_t*
row_build(
/*======*/
	ulint			type,	/*!< in: ROW_COPY_POINTERS or
					ROW_COPY_DATA; the latter
					copies also the data fields to
					heap while the first only
					places pointers to data fields
					on the index page, and thus is
					more efficient */
	const dict_index_t*	index,	/*!< in: clustered index */
	const rec_t*		rec,	/*!< in: record in the clustered
					index; NOTE: in the case
					ROW_COPY_POINTERS the data
					fields in the row will point
					directly into this record,
					therefore, the buffer page of
					this record must be at least
					s-latched and the latch held
					as long as the row dtuple is used! */
	const ulint*		offsets,/*!< in: rec_get_offsets(rec,index)
					or NULL, in which case this function
					will invoke rec_get_offsets() */
	const dict_table_t*	col_table,
					/*!< in: table, to check which
					externally stored columns
					occur in the ordering columns
					of an index, or NULL if
					index->table should be
					consulted instead */
	row_ext_t**		ext,	/*!< out, own: cache of
					externally stored column
					prefixes, or NULL */
	mem_heap_t*		heap)	/*!< in: memory heap from which
					the memory needed is allocated */
{
	dtuple_t*		row;
	const dict_table_t*	table;
	ulint			n_fields;
	ulint			n_ext_cols;
	ulint*			ext_cols	= NULL; /* remove warning */
	ulint			len;
	ulint			row_len;
	byte*			buf;
	ulint			i;
	ulint			j;
	mem_heap_t*		tmp_heap	= NULL;
	ulint			offsets_[REC_OFFS_NORMAL_SIZE];
	rec_offs_init(offsets_);

	ut_ad(index && rec && heap);
	ut_ad(dict_index_is_clust(index));

	if (!offsets) {
		offsets = rec_get_offsets(rec, index, offsets_,
					  ULINT_UNDEFINED, &tmp_heap);
	} else {
		ut_ad(rec_offs_validate(rec, index, offsets));
	}

#ifdef UNIV_BLOB_NULL_DEBUG
	if (rec_offs_any_null_extern(rec, offsets)) {
		/* This condition can occur during crash recovery
		before trx_rollback_active() has completed execution,
		or when a concurrently executing
		row_ins_index_entry_low() has committed the B-tree
		mini-transaction but has not yet managed to restore
		the cursor position for writing the big_rec. */
		ut_a(trx_undo_roll_ptr_is_insert(
			     row_get_rec_roll_ptr(rec, index, offsets)));
	}
#endif /* UNIV_BLOB_NULL_DEBUG */

	if (type != ROW_COPY_POINTERS) {
		/* Take a copy of rec to heap */
		buf = mem_heap_alloc(heap, rec_offs_size(offsets));
		rec = rec_copy(buf, rec, offsets);
		/* Avoid a debug assertion in rec_offs_validate(). */
		rec_offs_make_valid(rec, index, (ulint*) offsets);
	}

	table = index->table;
	row_len = dict_table_get_n_cols(table);

	row = dtuple_create(heap, row_len);

	dict_table_copy_types(row, table);

	dtuple_set_info_bits(row, rec_get_info_bits(
				     rec, dict_table_is_comp(table)));

	n_fields = rec_offs_n_fields(offsets);
	n_ext_cols = rec_offs_n_extern(offsets);
	if (n_ext_cols) {
		ext_cols = mem_heap_alloc(heap, n_ext_cols * sizeof *ext_cols);
	}

	for (i = j = 0; i < n_fields; i++) {
		dict_field_t*		ind_field
			= dict_index_get_nth_field(index, i);
		const dict_col_t*	col
			= dict_field_get_col(ind_field);
		ulint			col_no
			= dict_col_get_no(col);
		dfield_t*		dfield
			= dtuple_get_nth_field(row, col_no);

		if (ind_field->prefix_len == 0) {

			const byte*	field = rec_get_nth_field(
				rec, offsets, i, &len);

			dfield_set_data(dfield, field, len);
		}

		if (rec_offs_nth_extern(offsets, i)) {
			dfield_set_ext(dfield);

			if (UNIV_LIKELY_NULL(col_table)) {
				ut_a(col_no
				     < dict_table_get_n_cols(col_table));
				col = dict_table_get_nth_col(
					col_table, col_no);
			}

			if (col->ord_part) {
				/* We will have to fetch prefixes of
				externally stored columns that are
				referenced by column prefixes. */
				ext_cols[j++] = col_no;
			}
		}
	}

	ut_ad(dtuple_check_typed(row));

	if (!ext) {
		/* REDUNDANT and COMPACT formats store a local
		768-byte prefix of each externally stored
		column. No cache is needed. */
		ut_ad(dict_table_get_format(index->table)
		      < DICT_TF_FORMAT_ZIP);
	} else if (j) {
		*ext = row_ext_create(j, ext_cols, index->table->flags, row,
				      heap);
	} else {
		*ext = NULL;
	}

	if (tmp_heap) {
		mem_heap_free(tmp_heap);
	}

	return(row);
}

/*******************************************************************//**
Converts an index record to a typed data tuple.
@return index entry built; does not set info_bits, and the data fields
in the entry will point directly to rec */
UNIV_INTERN
dtuple_t*
row_rec_to_index_entry_low(
/*=======================*/
	const rec_t*		rec,	/*!< in: record in the index */
	const dict_index_t*	index,	/*!< in: index */
	const ulint*		offsets,/*!< in: rec_get_offsets(rec, index) */
	ulint*			n_ext,	/*!< out: number of externally
					stored columns */
	mem_heap_t*		heap)	/*!< in: memory heap from which
					the memory needed is allocated */
{
	dtuple_t*	entry;
	dfield_t*	dfield;
	ulint		i;
	const byte*	field;
	ulint		len;
	ulint		rec_len;

	ut_ad(rec && heap && index);
	/* Because this function may be invoked by row0merge.c
	on a record whose header is in different format, the check
	rec_offs_validate(rec, index, offsets) must be avoided here. */
	ut_ad(n_ext);
	*n_ext = 0;

	rec_len = rec_offs_n_fields(offsets);

	if (srv_use_sys_stats_table
	    && index == UT_LIST_GET_FIRST(dict_sys->sys_stats->indexes)) {
		if (rec_len < dict_index_get_n_fields(index)) {
			/* the new record should be extended */
			rec_len = dict_index_get_n_fields(index);
		}
	}

	entry = dtuple_create(heap, rec_len);

	dtuple_set_n_fields_cmp(entry,
				dict_index_get_n_unique_in_tree(index));
	ut_ad(rec_len == dict_index_get_n_fields(index));

	dict_index_copy_types(entry, index, rec_len);

	for (i = 0; i < rec_len; i++) {

		dfield = dtuple_get_nth_field(entry, i);

		if (srv_use_sys_stats_table
		    && index == UT_LIST_GET_FIRST(dict_sys->sys_stats->indexes)
		    && i >= rec_offs_n_fields(offsets)) {
			dfield_set_null(dfield);
			continue;
		}

		field = rec_get_nth_field(rec, offsets, i, &len);

		dfield_set_data(dfield, field, len);

		if (rec_offs_nth_extern(offsets, i)) {
			dfield_set_ext(dfield);
			(*n_ext)++;
		}
	}

	ut_ad(dtuple_check_typed(entry));

	return(entry);
}

/*******************************************************************//**
Converts an index record to a typed data tuple. NOTE that externally
stored (often big) fields are NOT copied to heap.
@return	own: index entry built; see the NOTE below! */
UNIV_INTERN
dtuple_t*
row_rec_to_index_entry(
/*===================*/
	ulint			type,	/*!< in: ROW_COPY_DATA, or
					ROW_COPY_POINTERS: the former
					copies also the data fields to
					heap as the latter only places
					pointers to data fields on the
					index page */
	const rec_t*		rec,	/*!< in: record in the index;
					NOTE: in the case
					ROW_COPY_POINTERS the data
					fields in the row will point
					directly into this record,
					therefore, the buffer page of
					this record must be at least
					s-latched and the latch held
					as long as the dtuple is used! */
	const dict_index_t*	index,	/*!< in: index */
	ulint*			offsets,/*!< in/out: rec_get_offsets(rec) */
	ulint*			n_ext,	/*!< out: number of externally
					stored columns */
	mem_heap_t*		heap)	/*!< in: memory heap from which
					the memory needed is allocated */
{
	dtuple_t*	entry;
	byte*		buf;

	ut_ad(rec && heap && index);
	ut_ad(rec_offs_validate(rec, index, offsets));

	if (type == ROW_COPY_DATA) {
		/* Take a copy of rec to heap */
		buf = mem_heap_alloc(heap, rec_offs_size(offsets));
		rec = rec_copy(buf, rec, offsets);
		/* Avoid a debug assertion in rec_offs_validate(). */
		rec_offs_make_valid(rec, index, offsets);
#ifdef UNIV_BLOB_NULL_DEBUG
	} else {
		ut_a(!rec_offs_any_null_extern(rec, offsets));
#endif /* UNIV_BLOB_NULL_DEBUG */
	}

	entry = row_rec_to_index_entry_low(rec, index, offsets, n_ext, heap);

	dtuple_set_info_bits(entry,
			     rec_get_info_bits(rec, rec_offs_comp(offsets)));

	return(entry);
}

/*******************************************************************//**
Builds from a secondary index record a row reference with which we can
search the clustered index record.
@return	own: row reference built; see the NOTE below! */
UNIV_INTERN
dtuple_t*
row_build_row_ref(
/*==============*/
	ulint		type,	/*!< in: ROW_COPY_DATA, or ROW_COPY_POINTERS:
				the former copies also the data fields to
				heap, whereas the latter only places pointers
				to data fields on the index page */
	dict_index_t*	index,	/*!< in: secondary index */
	const rec_t*	rec,	/*!< in: record in the index;
				NOTE: in the case ROW_COPY_POINTERS
				the data fields in the row will point
				directly into this record, therefore,
				the buffer page of this record must be
				at least s-latched and the latch held
				as long as the row reference is used! */
	mem_heap_t*	heap)	/*!< in: memory heap from which the memory
				needed is allocated */
{
	dict_table_t*	table;
	dict_index_t*	clust_index;
	dfield_t*	dfield;
	dtuple_t*	ref;
	const byte*	field;
	ulint		len;
	ulint		ref_len;
	ulint		pos;
	byte*		buf;
	ulint		clust_col_prefix_len;
	ulint		i;
	mem_heap_t*	tmp_heap	= NULL;
	ulint		offsets_[REC_OFFS_NORMAL_SIZE];
	ulint*		offsets		= offsets_;
	rec_offs_init(offsets_);

	ut_ad(index && rec && heap);
	ut_ad(!dict_index_is_clust(index));

	offsets = rec_get_offsets(rec, index, offsets,
				  ULINT_UNDEFINED, &tmp_heap);
	/* Secondary indexes must not contain externally stored columns. */
	ut_ad(!rec_offs_any_extern(offsets));

	if (type == ROW_COPY_DATA) {
		/* Take a copy of rec to heap */

		buf = mem_heap_alloc(heap, rec_offs_size(offsets));

		rec = rec_copy(buf, rec, offsets);
		/* Avoid a debug assertion in rec_offs_validate(). */
		rec_offs_make_valid(rec, index, offsets);
	}

	table = index->table;

	clust_index = dict_table_get_first_index(table);

	ref_len = dict_index_get_n_unique(clust_index);

	ref = dtuple_create(heap, ref_len);

	dict_index_copy_types(ref, clust_index, ref_len);

	for (i = 0; i < ref_len; i++) {
		dfield = dtuple_get_nth_field(ref, i);

		pos = dict_index_get_nth_field_pos(index, clust_index, i);

		ut_a(pos != ULINT_UNDEFINED);

		field = rec_get_nth_field(rec, offsets, pos, &len);

		dfield_set_data(dfield, field, len);

		/* If the primary key contains a column prefix, then the
		secondary index may contain a longer prefix of the same
		column, or the full column, and we must adjust the length
		accordingly. */

		clust_col_prefix_len = dict_index_get_nth_field(
			clust_index, i)->prefix_len;

		if (clust_col_prefix_len > 0) {
			if (len != UNIV_SQL_NULL) {

				const dtype_t*	dtype
					= dfield_get_type(dfield);

				dfield_set_len(dfield,
					       dtype_get_at_most_n_mbchars(
						       dtype->prtype,
						       dtype->mbminmaxlen,
						       clust_col_prefix_len,
						       len, (char*) field));
			}
		}
	}

	ut_ad(dtuple_check_typed(ref));
	if (tmp_heap) {
		mem_heap_free(tmp_heap);
	}

	return(ref);
}

/*******************************************************************//**
Builds from a secondary index record a row reference with which we can
search the clustered index record. */
UNIV_INTERN
void
row_build_row_ref_in_tuple(
/*=======================*/
	dtuple_t*		ref,	/*!< in/out: row reference built;
					see the NOTE below! */
	const rec_t*		rec,	/*!< in: record in the index;
					NOTE: the data fields in ref
					will point directly into this
					record, therefore, the buffer
					page of this record must be at
					least s-latched and the latch
					held as long as the row
					reference is used! */
	const dict_index_t*	index,	/*!< in: secondary index */
	ulint*			offsets,/*!< in: rec_get_offsets(rec, index)
					or NULL */
	trx_t*			trx)	/*!< in: transaction */
{
	const dict_index_t*	clust_index;
	dfield_t*		dfield;
	const byte*		field;
	ulint			len;
	ulint			ref_len;
	ulint			pos;
	ulint			clust_col_prefix_len;
	ulint			i;
	mem_heap_t*		heap		= NULL;
	ulint			offsets_[REC_OFFS_NORMAL_SIZE];
	rec_offs_init(offsets_);

	ut_a(ref);
	ut_a(index);
	ut_a(rec);
	ut_ad(!dict_index_is_clust(index));

	if (UNIV_UNLIKELY(!index->table)) {
		fputs("InnoDB: table ", stderr);
notfound:
		ut_print_name(stderr, trx, TRUE, index->table_name);
		fputs(" for index ", stderr);
		ut_print_name(stderr, trx, FALSE, index->name);
		fputs(" not found\n", stderr);
		ut_error;
	}

	clust_index = dict_table_get_first_index(index->table);

	if (UNIV_UNLIKELY(!clust_index)) {
		fputs("InnoDB: clust index for table ", stderr);
		goto notfound;
	}

	if (!offsets) {
		offsets = rec_get_offsets(rec, index, offsets_,
					  ULINT_UNDEFINED, &heap);
	} else {
		ut_ad(rec_offs_validate(rec, index, offsets));
	}

	/* Secondary indexes must not contain externally stored columns. */
	ut_ad(!rec_offs_any_extern(offsets));
	ref_len = dict_index_get_n_unique(clust_index);

	ut_ad(ref_len == dtuple_get_n_fields(ref));

	dict_index_copy_types(ref, clust_index, ref_len);

	for (i = 0; i < ref_len; i++) {
		dfield = dtuple_get_nth_field(ref, i);

		pos = dict_index_get_nth_field_pos(index, clust_index, i);

		ut_a(pos != ULINT_UNDEFINED);

		field = rec_get_nth_field(rec, offsets, pos, &len);

		dfield_set_data(dfield, field, len);

		/* If the primary key contains a column prefix, then the
		secondary index may contain a longer prefix of the same
		column, or the full column, and we must adjust the length
		accordingly. */

		clust_col_prefix_len = dict_index_get_nth_field(
			clust_index, i)->prefix_len;

		if (clust_col_prefix_len > 0) {
			if (len != UNIV_SQL_NULL) {

				const dtype_t*	dtype
					= dfield_get_type(dfield);

				dfield_set_len(dfield,
					       dtype_get_at_most_n_mbchars(
						       dtype->prtype,
						       dtype->mbminmaxlen,
						       clust_col_prefix_len,
						       len, (char*) field));
			}
		}
	}

	ut_ad(dtuple_check_typed(ref));
	if (UNIV_LIKELY_NULL(heap)) {
		mem_heap_free(heap);
	}
}

/***************************************************************//**
Searches the clustered index record for a row, if we have the row reference.
@return	TRUE if found */
UNIV_INTERN
ibool
row_search_on_row_ref(
/*==================*/
	btr_pcur_t*		pcur,	/*!< out: persistent cursor, which must
					be closed by the caller */
	ulint			mode,	/*!< in: BTR_MODIFY_LEAF, ... */
	const dict_table_t*	table,	/*!< in: table */
	const dtuple_t*		ref,	/*!< in: row reference */
	mtr_t*			mtr)	/*!< in/out: mtr */
{
	ulint		low_match;
	rec_t*		rec;
	dict_index_t*	index;

	ut_ad(dtuple_check_typed(ref));

	index = dict_table_get_first_index(table);

	ut_a(dtuple_get_n_fields(ref) == dict_index_get_n_unique(index));

	btr_pcur_open(index, ref, PAGE_CUR_LE, mode, pcur, mtr);

	low_match = btr_pcur_get_low_match(pcur);

	rec = btr_pcur_get_rec(pcur);

	if (page_rec_is_infimum(rec)) {

		return(FALSE);
	}

	if (low_match != dtuple_get_n_fields(ref)) {

		return(FALSE);
	}

	return(TRUE);
}

/*********************************************************************//**
Fetches the clustered index record for a secondary index record. The latches
on the secondary index record are preserved.
@return	record or NULL, if no record found */
UNIV_INTERN
rec_t*
row_get_clust_rec(
/*==============*/
	ulint		mode,	/*!< in: BTR_MODIFY_LEAF, ... */
	const rec_t*	rec,	/*!< in: record in a secondary index */
	dict_index_t*	index,	/*!< in: secondary index */
	dict_index_t**	clust_index,/*!< out: clustered index */
	mtr_t*		mtr)	/*!< in: mtr */
{
	mem_heap_t*	heap;
	dtuple_t*	ref;
	dict_table_t*	table;
	btr_pcur_t	pcur;
	ibool		found;
	rec_t*		clust_rec;

	ut_ad(!dict_index_is_clust(index));

	table = index->table;

	heap = mem_heap_create(256);

	ref = row_build_row_ref(ROW_COPY_POINTERS, index, rec, heap);

	found = row_search_on_row_ref(&pcur, mode, table, ref, mtr);

	clust_rec = found ? btr_pcur_get_rec(&pcur) : NULL;

	mem_heap_free(heap);

	btr_pcur_close(&pcur);

	*clust_index = dict_table_get_first_index(table);

	return(clust_rec);
}

/***************************************************************//**
Searches an index record.
@return	whether the record was found or buffered */
UNIV_INTERN
enum row_search_result
row_search_index_entry(
/*===================*/
	dict_index_t*	index,	/*!< in: index */
	const dtuple_t*	entry,	/*!< in: index entry */
	ulint		mode,	/*!< in: BTR_MODIFY_LEAF, ... */
	btr_pcur_t*	pcur,	/*!< in/out: persistent cursor, which must
				be closed by the caller */
	mtr_t*		mtr)	/*!< in: mtr */
{
	ulint	n_fields;
	ulint	low_match;
	rec_t*	rec;

	ut_ad(dtuple_check_typed(entry));

	btr_pcur_open(index, entry, PAGE_CUR_LE, mode, pcur, mtr);

	switch (btr_pcur_get_btr_cur(pcur)->flag) {
	case BTR_CUR_DELETE_REF:
		ut_a(mode & BTR_DELETE);
		return(ROW_NOT_DELETED_REF);

	case BTR_CUR_DEL_MARK_IBUF:
	case BTR_CUR_DELETE_IBUF:
	case BTR_CUR_INSERT_TO_IBUF:
		return(ROW_BUFFERED);

	case BTR_CUR_HASH:
	case BTR_CUR_HASH_FAIL:
	case BTR_CUR_BINARY:
		break;
	}

	low_match = btr_pcur_get_low_match(pcur);

	rec = btr_pcur_get_rec(pcur);

	n_fields = dtuple_get_n_fields(entry);

	if (page_rec_is_infimum(rec)) {

		return(ROW_NOT_FOUND);
	} else if (low_match != n_fields) {

		return(ROW_NOT_FOUND);
	}

	return(ROW_FOUND);
}

#include <my_sys.h>

/*******************************************************************//**
Formats the raw data in "data" (in InnoDB on-disk format) that is of
type DATA_INT using "prtype" and writes the result to "buf".
If the data is in unknown format, then nothing is written to "buf",
0 is returned and "format_in_hex" is set to TRUE, otherwise
"format_in_hex" is left untouched.
Not more than "buf_size" bytes are written to "buf".
The result is always '\0'-terminated (provided buf_size > 0) and the
number of bytes that were written to "buf" is returned (including the
terminating '\0').
@return	number of bytes that were written */
static
ulint
row_raw_format_int(
/*===============*/
	const char*	data,		/*!< in: raw data */
	ulint		data_len,	/*!< in: raw data length
					in bytes */
	ulint		prtype,		/*!< in: precise type */
	char*		buf,		/*!< out: output buffer */
	ulint		buf_size,	/*!< in: output buffer size
					in bytes */
	ibool*		format_in_hex)	/*!< out: should the data be
					formated in hex */
{
	ulint	ret;

	if (data_len <= sizeof(ullint)) {

		ullint		value;
		ibool		unsigned_type = prtype & DATA_UNSIGNED;

		value = mach_read_int_type((const byte*) data,
					   data_len, unsigned_type);

		if (unsigned_type) {

			ret = ut_snprintf(buf, buf_size, "%llu",
					  value) + 1;
		} else {

			ret = ut_snprintf(buf, buf_size, "%lld",
					  (long long) value) + 1;
		}

	} else {

		*format_in_hex = TRUE;
		ret = 0;
	}

	return(ut_min(ret, buf_size));
}

/*******************************************************************//**
Formats the raw data in "data" (in InnoDB on-disk format) that is of
type DATA_(CHAR|VARCHAR|MYSQL|VARMYSQL) using "prtype" and writes the
result to "buf".
If the data is in binary format, then nothing is written to "buf",
0 is returned and "format_in_hex" is set to TRUE, otherwise
"format_in_hex" is left untouched.
Not more than "buf_size" bytes are written to "buf".
The result is always '\0'-terminated (provided buf_size > 0) and the
number of bytes that were written to "buf" is returned (including the
terminating '\0').
@return	number of bytes that were written */
static
ulint
row_raw_format_str(
/*===============*/
	const char*	data,		/*!< in: raw data */
	ulint		data_len,	/*!< in: raw data length
					in bytes */
	ulint		prtype,		/*!< in: precise type */
	char*		buf,		/*!< out: output buffer */
	ulint		buf_size,	/*!< in: output buffer size
					in bytes */
	ibool*		format_in_hex)	/*!< out: should the data be
					formated in hex */
{
	ulint	charset_coll;

	if (buf_size == 0) {

		return(0);
	}

	/* we assume system_charset_info is UTF-8 */

	charset_coll = dtype_get_charset_coll(prtype);

	if (UNIV_LIKELY(dtype_is_utf8(prtype))) {

		return(ut_str_sql_format(data, data_len, buf, buf_size));
	}
	/* else */

	if (charset_coll == DATA_MYSQL_BINARY_CHARSET_COLL) {

		*format_in_hex = TRUE;
		return(0);
	}
	/* else */

	return(innobase_raw_format(data, data_len, charset_coll,
					  buf, buf_size));
}

/*******************************************************************//**
Formats the raw data in "data" (in InnoDB on-disk format) using
"dict_field" and writes the result to "buf".
Not more than "buf_size" bytes are written to "buf".
The result is always NUL-terminated (provided buf_size is positive) and the
number of bytes that were written to "buf" is returned (including the
terminating NUL).
@return	number of bytes that were written */
UNIV_INTERN
ulint
row_raw_format(
/*===========*/
	const char*		data,		/*!< in: raw data */
	ulint			data_len,	/*!< in: raw data length
						in bytes */
	const dict_field_t*	dict_field,	/*!< in: index field */
	char*			buf,		/*!< out: output buffer */
	ulint			buf_size)	/*!< in: output buffer size
						in bytes */
{
	ulint	mtype;
	ulint	prtype;
	ulint	ret;
	ibool	format_in_hex;

	if (buf_size == 0) {

		return(0);
	}

	if (data_len == UNIV_SQL_NULL) {

		ret = ut_snprintf((char*) buf, buf_size, "NULL") + 1;

		return(ut_min(ret, buf_size));
	}

	mtype = dict_field->col->mtype;
	prtype = dict_field->col->prtype;

	format_in_hex = FALSE;

	switch (mtype) {
	case DATA_INT:

		ret = row_raw_format_int(data, data_len, prtype,
					 buf, buf_size, &format_in_hex);
		if (format_in_hex) {

			goto format_in_hex;
		}
		break;
	case DATA_CHAR:
	case DATA_VARCHAR:
	case DATA_MYSQL:
	case DATA_VARMYSQL:

		ret = row_raw_format_str(data, data_len, prtype,
					 buf, buf_size, &format_in_hex);
		if (format_in_hex) {

			goto format_in_hex;
		}

		break;
	/* XXX support more data types */
	default:
	format_in_hex:

		if (UNIV_LIKELY(buf_size > 2)) {

			memcpy(buf, "0x", 2);
			buf += 2;
			buf_size -= 2;
			ret = 2 + ut_raw_to_hex(data, data_len,
						buf, buf_size);
		} else {

			buf[0] = '\0';
			ret = 1;
		}
	}

	return(ret);
}

#ifdef UNIV_COMPILE_TEST_FUNCS

#include "ut0dbg.h"

void
test_row_raw_format_int()
{
	ulint	ret;
	char	buf[128];
	ibool	format_in_hex;

#define CALL_AND_TEST(data, data_len, prtype, buf, buf_size,\
		      ret_expected, buf_expected, format_in_hex_expected)\
	do {\
		ibool	ok = TRUE;\
		ulint	i;\
		memset(buf, 'x', 10);\
		buf[10] = '\0';\
		format_in_hex = FALSE;\
		fprintf(stderr, "TESTING \"\\x");\
		for (i = 0; i < data_len; i++) {\
			fprintf(stderr, "%02hhX", data[i]);\
		}\
		fprintf(stderr, "\", %lu, %lu, %lu\n",\
                        (ulint) data_len, (ulint) prtype,\
			(ulint) buf_size);\
		ret = row_raw_format_int(data, data_len, prtype,\
					 buf, buf_size, &format_in_hex);\
		if (ret != ret_expected) {\
			fprintf(stderr, "expected ret %lu, got %lu\n",\
				(ulint) ret_expected, ret);\
			ok = FALSE;\
                }\
                if (strcmp((char*) buf, buf_expected) != 0) {\
                        fprintf(stderr, "expected buf \"%s\", got \"%s\"\n",\
                                buf_expected, buf);\
                        ok = FALSE;\
                }\
                if (format_in_hex != format_in_hex_expected) {\
                        fprintf(stderr, "expected format_in_hex %d, got %d\n",\
                                (int) format_in_hex_expected,\
				(int) format_in_hex);\
                        ok = FALSE;\
                }\
                if (ok) {\
                        fprintf(stderr, "OK: %lu, \"%s\" %d\n\n",\
                                (ulint) ret, buf, (int) format_in_hex);\
                } else {\
                        return;\
                }\
        } while (0)

#if 1
	/* min values for signed 1-8 byte integers */

	CALL_AND_TEST("\x00", 1, 0,
		      buf, sizeof(buf), 5, "-128", 0);

	CALL_AND_TEST("\x00\x00", 2, 0,
		      buf, sizeof(buf), 7, "-32768", 0);

	CALL_AND_TEST("\x00\x00\x00", 3, 0,
		      buf, sizeof(buf), 9, "-8388608", 0);

	CALL_AND_TEST("\x00\x00\x00\x00", 4, 0,
		      buf, sizeof(buf), 12, "-2147483648", 0);

	CALL_AND_TEST("\x00\x00\x00\x00\x00", 5, 0,
		      buf, sizeof(buf), 14, "-549755813888", 0);

	CALL_AND_TEST("\x00\x00\x00\x00\x00\x00", 6, 0,
		      buf, sizeof(buf), 17, "-140737488355328", 0);

	CALL_AND_TEST("\x00\x00\x00\x00\x00\x00\x00", 7, 0,
		      buf, sizeof(buf), 19, "-36028797018963968", 0);

	CALL_AND_TEST("\x00\x00\x00\x00\x00\x00\x00\x00", 8, 0,
		      buf, sizeof(buf), 21, "-9223372036854775808", 0);

	/* min values for unsigned 1-8 byte integers */

	CALL_AND_TEST("\x00", 1, DATA_UNSIGNED,
		      buf, sizeof(buf), 2, "0", 0);

	CALL_AND_TEST("\x00\x00", 2, DATA_UNSIGNED,
		      buf, sizeof(buf), 2, "0", 0);

	CALL_AND_TEST("\x00\x00\x00", 3, DATA_UNSIGNED,
		      buf, sizeof(buf), 2, "0", 0);

	CALL_AND_TEST("\x00\x00\x00\x00", 4, DATA_UNSIGNED,
		      buf, sizeof(buf), 2, "0", 0);

	CALL_AND_TEST("\x00\x00\x00\x00\x00", 5, DATA_UNSIGNED,
		      buf, sizeof(buf), 2, "0", 0);

	CALL_AND_TEST("\x00\x00\x00\x00\x00\x00", 6, DATA_UNSIGNED,
		      buf, sizeof(buf), 2, "0", 0);

	CALL_AND_TEST("\x00\x00\x00\x00\x00\x00\x00", 7, DATA_UNSIGNED,
		      buf, sizeof(buf), 2, "0", 0);

	CALL_AND_TEST("\x00\x00\x00\x00\x00\x00\x00\x00", 8, DATA_UNSIGNED,
		      buf, sizeof(buf), 2, "0", 0);

	/* max values for signed 1-8 byte integers */

	CALL_AND_TEST("\xFF", 1, 0,
		      buf, sizeof(buf), 4, "127", 0);

	CALL_AND_TEST("\xFF\xFF", 2, 0,
		      buf, sizeof(buf), 6, "32767", 0);

	CALL_AND_TEST("\xFF\xFF\xFF", 3, 0,
		      buf, sizeof(buf), 8, "8388607", 0);

	CALL_AND_TEST("\xFF\xFF\xFF\xFF", 4, 0,
		      buf, sizeof(buf), 11, "2147483647", 0);

	CALL_AND_TEST("\xFF\xFF\xFF\xFF\xFF", 5, 0,
		      buf, sizeof(buf), 13, "549755813887", 0);

	CALL_AND_TEST("\xFF\xFF\xFF\xFF\xFF\xFF", 6, 0,
		      buf, sizeof(buf), 16, "140737488355327", 0);

	CALL_AND_TEST("\xFF\xFF\xFF\xFF\xFF\xFF\xFF", 7, 0,
		      buf, sizeof(buf), 18, "36028797018963967", 0);

	CALL_AND_TEST("\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF", 8, 0,
		      buf, sizeof(buf), 20, "9223372036854775807", 0);

	/* max values for unsigned 1-8 byte integers */

	CALL_AND_TEST("\xFF", 1, DATA_UNSIGNED,
		      buf, sizeof(buf), 4, "255", 0);

	CALL_AND_TEST("\xFF\xFF", 2, DATA_UNSIGNED,
		      buf, sizeof(buf), 6, "65535", 0);

	CALL_AND_TEST("\xFF\xFF\xFF", 3, DATA_UNSIGNED,
		      buf, sizeof(buf), 9, "16777215", 0);

	CALL_AND_TEST("\xFF\xFF\xFF\xFF", 4, DATA_UNSIGNED,
		      buf, sizeof(buf), 11, "4294967295", 0);

	CALL_AND_TEST("\xFF\xFF\xFF\xFF\xFF", 5, DATA_UNSIGNED,
		      buf, sizeof(buf), 14, "1099511627775", 0);

	CALL_AND_TEST("\xFF\xFF\xFF\xFF\xFF\xFF", 6, DATA_UNSIGNED,
		      buf, sizeof(buf), 16, "281474976710655", 0);

	CALL_AND_TEST("\xFF\xFF\xFF\xFF\xFF\xFF\xFF", 7, DATA_UNSIGNED,
		      buf, sizeof(buf), 18, "72057594037927935", 0);

	CALL_AND_TEST("\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF", 8, DATA_UNSIGNED,
		      buf, sizeof(buf), 21, "18446744073709551615", 0);

	/* some random values */

	CALL_AND_TEST("\x52", 1, 0,
		      buf, sizeof(buf), 4, "-46", 0);

	CALL_AND_TEST("\x0E", 1, DATA_UNSIGNED,
		      buf, sizeof(buf), 3, "14", 0);

	CALL_AND_TEST("\x62\xCE", 2, 0,
		      buf, sizeof(buf), 6, "-7474", 0);

	CALL_AND_TEST("\x29\xD6", 2, DATA_UNSIGNED,
		      buf, sizeof(buf), 6, "10710", 0);

	CALL_AND_TEST("\x7F\xFF\x90", 3, 0,
		      buf, sizeof(buf), 5, "-112", 0);

	CALL_AND_TEST("\x00\xA1\x16", 3, DATA_UNSIGNED,
		      buf, sizeof(buf), 6, "41238", 0);

	CALL_AND_TEST("\x7F\xFF\xFF\xF7", 4, 0,
		      buf, sizeof(buf), 3, "-9", 0);

	CALL_AND_TEST("\x00\x00\x00\x5C", 4, DATA_UNSIGNED,
		      buf, sizeof(buf), 3, "92", 0);

	CALL_AND_TEST("\x7F\xFF\xFF\xFF\xFF\xFF\xDC\x63", 8, 0,
		      buf, sizeof(buf), 6, "-9117", 0);

	CALL_AND_TEST("\x00\x00\x00\x00\x00\x01\x64\x62", 8, DATA_UNSIGNED,
		      buf, sizeof(buf), 6, "91234", 0);
#endif

	/* speed test */

	speedo_t	speedo;
	ulint		i;

	speedo_reset(&speedo);

	for (i = 0; i < 1000000; i++) {
		row_raw_format_int("\x23", 1,
				   0, buf, sizeof(buf),
				   &format_in_hex);
		row_raw_format_int("\x23", 1,
				   DATA_UNSIGNED, buf, sizeof(buf),
				   &format_in_hex);

		row_raw_format_int("\x00\x00\x00\x00\x00\x01\x64\x62", 8,
				   0, buf, sizeof(buf),
				   &format_in_hex);
		row_raw_format_int("\x00\x00\x00\x00\x00\x01\x64\x62", 8,
				   DATA_UNSIGNED, buf, sizeof(buf),
				   &format_in_hex);
	}

	speedo_show(&speedo);
}

#endif /* UNIV_COMPILE_TEST_FUNCS */
