/******************************************************
General row routines

(c) 1996 Innobase Oy

Created 4/20/1996 Heikki Tuuri
*******************************************************/

#include "row0row.h"

#ifdef UNIV_NONINL
#include "row0row.ic"
#endif

#include "dict0dict.h"
#include "btr0btr.h"
#include "mach0data.h"
#include "trx0rseg.h"
#include "trx0trx.h"
#include "trx0roll.h"
#include "trx0undo.h"
#include "trx0purge.h"
#include "trx0rec.h"
#include "que0que.h"
#include "row0row.h"
#include "row0upd.h"
#include "rem0cmp.h"
#include "read0read.h"

/*************************************************************************
Reads the trx id or roll ptr field from a clustered index record: this function
is slower than the specialized inline functions. */

dulint
row_get_rec_sys_field(
/*==================*/
				/* out: value of the field */
	ulint		type,	/* in: DATA_TRX_ID or DATA_ROLL_PTR */
	rec_t*		rec,	/* in: record */
	dict_index_t*	index,	/* in: clustered index */
	const ulint*	offsets)/* in: rec_get_offsets(rec, index) */
{
	ulint		pos;
	byte*		field;
	ulint		len;

	ut_ad(index->type & DICT_CLUSTERED);

	pos = dict_index_get_sys_col_pos(index, type);

	field = rec_get_nth_field(rec, offsets, pos, &len);

	if (type == DATA_TRX_ID) {

		return(trx_read_trx_id(field));
	} else {
		ut_ad(type == DATA_ROLL_PTR);

		return(trx_read_roll_ptr(field));
	}
}

/*************************************************************************
Sets the trx id or roll ptr field in a clustered index record: this function
is slower than the specialized inline functions. */

void
row_set_rec_sys_field(
/*==================*/
				/* out: value of the field */
	ulint		type,	/* in: DATA_TRX_ID or DATA_ROLL_PTR */
	rec_t*		rec,	/* in: record */
	dict_index_t*	index,	/* in: clustered index */
	const ulint*	offsets,/* in: rec_get_offsets(rec, index) */
	dulint		val)	/* in: value to set */
{
	ulint	pos;
	byte*	field;
	ulint	len;

	ut_ad(index->type & DICT_CLUSTERED);
	ut_ad(rec_offs_validate(rec, index, offsets));

	pos = dict_index_get_sys_col_pos(index, type);

	field = rec_get_nth_field(rec, offsets, pos, &len);

	if (type == DATA_TRX_ID) {

		trx_write_trx_id(field, val);
	} else {
		ut_ad(type == DATA_ROLL_PTR);

		trx_write_roll_ptr(field, val);
	}
}

/*********************************************************************
When an insert to a table is performed, this function builds the entry which
has to be inserted to an index on the table. */

dtuple_t*
row_build_index_entry(
/*==================*/
				/* out: index entry which should be inserted */
	dtuple_t*	row,	/* in: row which should be inserted to the
				table */
	dict_index_t*	index,	/* in: index on the table */
	mem_heap_t*	heap)	/* in: memory heap from which the memory for
				the index entry is allocated */
{
	dtuple_t*	entry;
	ulint		entry_len;
	dict_field_t*	ind_field;
	dfield_t*	dfield;
	dfield_t*	dfield2;
	dict_col_t*	col;
	ulint		i;
	ulint		storage_len;
	dtype_t*	cur_type;

	ut_ad(row && index && heap);
	ut_ad(dtuple_check_typed(row));

	entry_len = dict_index_get_n_fields(index);
	entry = dtuple_create(heap, entry_len);

	if (index->type & DICT_UNIVERSAL) {
		dtuple_set_n_fields_cmp(entry, entry_len);
	} else {
		dtuple_set_n_fields_cmp
			(entry, dict_index_get_n_unique_in_tree(index));
	}

	for (i = 0; i < entry_len; i++) {
		ind_field = dict_index_get_nth_field(index, i);
		col = ind_field->col;

		dfield = dtuple_get_nth_field(entry, i);

		dfield2 = dtuple_get_nth_field(row, dict_col_get_no(col));

		dfield_copy(dfield, dfield2);

		/* If a column prefix index, take only the prefix */
		if (ind_field->prefix_len > 0
		    && dfield_get_len(dfield2) != UNIV_SQL_NULL) {

			cur_type = dict_col_get_type
				(dict_field_get_col(ind_field));

			storage_len = dtype_get_at_most_n_mbchars
				(cur_type, ind_field->prefix_len,
				 dfield_get_len(dfield2), dfield2->data);

			dfield_set_len(dfield, storage_len);
		}
	}

	ut_ad(dtuple_check_typed(entry));

	return(entry);
}

/***********************************************************************
An inverse function to dict_row_build_index_entry. Builds a row from a
record in a clustered index. */

dtuple_t*
row_build(
/*======*/
				/* out, own: row built; see the NOTE below! */
	ulint		type,	/* in: ROW_COPY_POINTERS or ROW_COPY_DATA;
				the latter copies also the data fields to
				heap while the first only places pointers to
				data fields on the index page, and thus is
				more efficient */
	dict_index_t*	index,	/* in: clustered index */
	rec_t*		rec,	/* in: record in the clustered index;
				NOTE: in the case ROW_COPY_POINTERS
				the data fields in the row will point
				directly into this record, therefore,
				the buffer page of this record must be
				at least s-latched and the latch held
				as long as the row dtuple is used! */
	const ulint*	offsets,/* in: rec_get_offsets(rec, index)
				or NULL, in which case this function
				will invoke rec_get_offsets() */
	mem_heap_t*	heap)	/* in: memory heap from which the memory
				needed is allocated */
{
	dtuple_t*	row;
	dict_table_t*	table;
	dict_field_t*	ind_field;
	dict_col_t*	col;
	dfield_t*	dfield;
	ulint		n_fields;
	byte*		field;
	ulint		len;
	ulint		row_len;
	byte*		buf;
	ulint		i;
	mem_heap_t*	tmp_heap	= NULL;
	ulint		offsets_[REC_OFFS_NORMAL_SIZE];
	*offsets_ = (sizeof offsets_) / sizeof *offsets_;

	ut_ad(index && rec && heap);
	ut_ad(index->type & DICT_CLUSTERED);

	if (!offsets) {
		offsets = rec_get_offsets(rec, index, offsets_,
					  ULINT_UNDEFINED, &tmp_heap);
	} else {
		ut_ad(rec_offs_validate(rec, index, offsets));
	}

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

	dtuple_set_info_bits(row, rec_get_info_bits
			     (rec, dict_table_is_comp(table)));

	n_fields = rec_offs_n_fields(offsets);

	dict_table_copy_types(row, table);

	for (i = 0; i < n_fields; i++) {
		ind_field = dict_index_get_nth_field(index, i);

		if (ind_field->prefix_len == 0) {

			col = dict_field_get_col(ind_field);
			dfield = dtuple_get_nth_field(row,
						      dict_col_get_no(col));
			field = rec_get_nth_field(rec, offsets, i, &len);

			dfield_set_data(dfield, field, len);
		}
	}

	ut_ad(dtuple_check_typed(row));

	if (tmp_heap) {
		mem_heap_free(tmp_heap);
	}

	return(row);
}

/***********************************************************************
Converts an index record to a typed data tuple. NOTE that externally
stored (often big) fields are NOT copied to heap. */

dtuple_t*
row_rec_to_index_entry(
/*===================*/
				/* out, own: index entry built; see the
				NOTE below! */
	ulint		type,	/* in: ROW_COPY_DATA, or ROW_COPY_POINTERS:
				the former copies also the data fields to
				heap as the latter only places pointers to
				data fields on the index page */
	dict_index_t*	index,	/* in: index */
	rec_t*		rec,	/* in: record in the index;
				NOTE: in the case ROW_COPY_POINTERS
				the data fields in the row will point
				directly into this record, therefore,
				the buffer page of this record must be
				at least s-latched and the latch held
				as long as the dtuple is used! */
	mem_heap_t*	heap)	/* in: memory heap from which the memory
				needed is allocated */
{
	dtuple_t*	entry;
	dfield_t*	dfield;
	ulint		i;
	byte*		field;
	ulint		len;
	ulint		rec_len;
	byte*		buf;
	mem_heap_t*	tmp_heap	= NULL;
	ulint		offsets_[REC_OFFS_NORMAL_SIZE];
	ulint*		offsets		= offsets_;
	*offsets_ = (sizeof offsets_) / sizeof *offsets_;

	ut_ad(rec && heap && index);

	offsets = rec_get_offsets(rec, index, offsets,
				  ULINT_UNDEFINED, &tmp_heap);

	if (type == ROW_COPY_DATA) {
		/* Take a copy of rec to heap */
		buf = mem_heap_alloc(heap, rec_offs_size(offsets));
		rec = rec_copy(buf, rec, offsets);
		/* Avoid a debug assertion in rec_offs_validate(). */
		rec_offs_make_valid(rec, index, offsets);
	}

	rec_len = rec_offs_n_fields(offsets);

	entry = dtuple_create(heap, rec_len);

	dtuple_set_n_fields_cmp(entry,
				dict_index_get_n_unique_in_tree(index));
	ut_ad(rec_len == dict_index_get_n_fields(index));

	dict_index_copy_types(entry, index, rec_len);

	dtuple_set_info_bits(entry,
			     rec_get_info_bits(rec, rec_offs_comp(offsets)));

	for (i = 0; i < rec_len; i++) {

		dfield = dtuple_get_nth_field(entry, i);
		field = rec_get_nth_field(rec, offsets, i, &len);

		dfield_set_data(dfield, field, len);
	}

	ut_ad(dtuple_check_typed(entry));
	if (tmp_heap) {
		mem_heap_free(tmp_heap);
	}

	return(entry);
}

/***********************************************************************
Builds from a secondary index record a row reference with which we can
search the clustered index record. */

dtuple_t*
row_build_row_ref(
/*==============*/
				/* out, own: row reference built; see the
				NOTE below! */
	ulint		type,	/* in: ROW_COPY_DATA, or ROW_COPY_POINTERS:
				the former copies also the data fields to
				heap, whereas the latter only places pointers
				to data fields on the index page */
	dict_index_t*	index,	/* in: index */
	rec_t*		rec,	/* in: record in the index;
				NOTE: in the case ROW_COPY_POINTERS
				the data fields in the row will point
				directly into this record, therefore,
				the buffer page of this record must be
				at least s-latched and the latch held
				as long as the row reference is used! */
	mem_heap_t*	heap)	/* in: memory heap from which the memory
				needed is allocated */
{
	dict_table_t*	table;
	dict_index_t*	clust_index;
	dfield_t*	dfield;
	dtuple_t*	ref;
	byte*		field;
	ulint		len;
	ulint		ref_len;
	ulint		pos;
	byte*		buf;
	ulint		clust_col_prefix_len;
	ulint		i;
	mem_heap_t*	tmp_heap	= NULL;
	ulint		offsets_[REC_OFFS_NORMAL_SIZE];
	ulint*		offsets		= offsets_;
	*offsets_ = (sizeof offsets_) / sizeof *offsets_;

	ut_ad(index && rec && heap);

	offsets = rec_get_offsets(rec, index, offsets,
				  ULINT_UNDEFINED, &tmp_heap);

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

		clust_col_prefix_len = dict_index_get_nth_field
			(clust_index, i)->prefix_len;

		if (clust_col_prefix_len > 0) {
			if (len != UNIV_SQL_NULL) {

				dfield_set_len(dfield,
					       dtype_get_at_most_n_mbchars
					       (dfield_get_type(dfield),
						clust_col_prefix_len, len,
						(char*) field));
			}
		}
	}

	ut_ad(dtuple_check_typed(ref));
	if (tmp_heap) {
		mem_heap_free(tmp_heap);
	}

	return(ref);
}

/***********************************************************************
Builds from a secondary index record a row reference with which we can
search the clustered index record. */

void
row_build_row_ref_in_tuple(
/*=======================*/
	dtuple_t*	ref,	/* in/out: row reference built; see the
				NOTE below! */
	dict_index_t*	index,	/* in: index */
	rec_t*		rec,	/* in: record in the index;
				NOTE: the data fields in ref will point
				directly into this record, therefore,
				the buffer page of this record must be
				at least s-latched and the latch held
				as long as the row reference is used! */
	trx_t*		trx)	/* in: transaction */
{
	dict_index_t*	clust_index;
	dfield_t*	dfield;
	byte*		field;
	ulint		len;
	ulint		ref_len;
	ulint		pos;
	ulint		clust_col_prefix_len;
	ulint		i;
	mem_heap_t*	heap		= NULL;
	ulint		offsets_[REC_OFFS_NORMAL_SIZE];
	ulint*		offsets		= offsets_;
	*offsets_ = (sizeof offsets_) / sizeof *offsets_;

	ut_a(ref && index && rec);

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

	if (!clust_index) {
		fputs("InnoDB: clust index for table ", stderr);
		goto notfound;
	}

	offsets = rec_get_offsets(rec, index, offsets, ULINT_UNDEFINED, &heap);

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

		clust_col_prefix_len = dict_index_get_nth_field
			(clust_index, i)->prefix_len;

		if (clust_col_prefix_len > 0) {
			if (len != UNIV_SQL_NULL) {

				dfield_set_len(dfield,
					       dtype_get_at_most_n_mbchars
					       (dfield_get_type(dfield),
						clust_col_prefix_len, len,
						(char*) field));
			}
		}
	}

	ut_ad(dtuple_check_typed(ref));
	if (UNIV_LIKELY_NULL(heap)) {
		mem_heap_free(heap);
	}
}

/***********************************************************************
From a row build a row reference with which we can search the clustered
index record. */

void
row_build_row_ref_from_row(
/*=======================*/
	dtuple_t*	ref,	/* in/out: row reference built; see the
				NOTE below! ref must have the right number
				of fields! */
	dict_table_t*	table,	/* in: table */
	dtuple_t*	row)	/* in: row
				NOTE: the data fields in ref will point
				directly into data of this row */
{
	dict_index_t*	clust_index;
	dict_field_t*	field;
	dfield_t*	dfield;
	dfield_t*	dfield2;
	dict_col_t*	col;
	ulint		ref_len;
	ulint		i;
	dtype_t*	cur_type;

	ut_ad(ref && table && row);

	clust_index = dict_table_get_first_index(table);

	ref_len = dict_index_get_n_unique(clust_index);

	ut_ad(ref_len == dtuple_get_n_fields(ref));

	for (i = 0; i < ref_len; i++) {
		dfield = dtuple_get_nth_field(ref, i);

		field = dict_index_get_nth_field(clust_index, i);

		col = dict_field_get_col(field);

		dfield2 = dtuple_get_nth_field(row, dict_col_get_no(col));

		dfield_copy(dfield, dfield2);

		if (field->prefix_len > 0
		    && dfield->len != UNIV_SQL_NULL) {

			cur_type = dict_col_get_type
				(dict_field_get_col(field));

			dfield->len = dtype_get_at_most_n_mbchars
				(cur_type, field->prefix_len,
				 dfield->len, dfield->data);
		}
	}

	ut_ad(dtuple_check_typed(ref));
}

/*******************************************************************
Searches the clustered index record for a row, if we have the row reference. */

ibool
row_search_on_row_ref(
/*==================*/
				/* out: TRUE if found */
	btr_pcur_t*	pcur,	/* in/out: persistent cursor, which must
				be closed by the caller */
	ulint		mode,	/* in: BTR_MODIFY_LEAF, ... */
	dict_table_t*	table,	/* in: table */
	dtuple_t*	ref,	/* in: row reference */
	mtr_t*		mtr)	/* in: mtr */
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

/*************************************************************************
Fetches the clustered index record for a secondary index record. The latches
on the secondary index record are preserved. */

rec_t*
row_get_clust_rec(
/*==============*/
				/* out: record or NULL, if no record found */
	ulint		mode,	/* in: BTR_MODIFY_LEAF, ... */
	rec_t*		rec,	/* in: record in a secondary index */
	dict_index_t*	index,	/* in: secondary index */
	dict_index_t**	clust_index,/* out: clustered index */
	mtr_t*		mtr)	/* in: mtr */
{
	mem_heap_t*	heap;
	dtuple_t*	ref;
	dict_table_t*	table;
	btr_pcur_t	pcur;
	ibool		found;
	rec_t*		clust_rec;

	ut_ad((index->type & DICT_CLUSTERED) == 0);

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

/*******************************************************************
Searches an index record. */

ibool
row_search_index_entry(
/*===================*/
				/* out: TRUE if found */
	dict_index_t*	index,	/* in: index */
	dtuple_t*	entry,	/* in: index entry */
	ulint		mode,	/* in: BTR_MODIFY_LEAF, ... */
	btr_pcur_t*	pcur,	/* in/out: persistent cursor, which must
				be closed by the caller */
	mtr_t*		mtr)	/* in: mtr */
{
	ulint	n_fields;
	ulint	low_match;
	rec_t*	rec;

	ut_ad(dtuple_check_typed(entry));

	btr_pcur_open(index, entry, PAGE_CUR_LE, mode, pcur, mtr);
	low_match = btr_pcur_get_low_match(pcur);

	rec = btr_pcur_get_rec(pcur);

	n_fields = dtuple_get_n_fields(entry);

	if (page_rec_is_infimum(rec)) {

		return(FALSE);
	}

	if (low_match != n_fields) {
		/* Not found */

		return(FALSE);
	}

	return(TRUE);
}
