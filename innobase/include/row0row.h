/******************************************************
General row routines

(c) 1996 Innobase Oy

Created 4/20/1996 Heikki Tuuri
*******************************************************/

#ifndef row0row_h
#define row0row_h

#include "univ.i"
#include "data0data.h"
#include "dict0types.h"
#include "trx0types.h"
#include "que0types.h"
#include "mtr0mtr.h"	
#include "rem0types.h"
#include "read0types.h"
#include "btr0types.h"

/*************************************************************************
Reads the trx id field from a clustered index record. */
UNIV_INLINE
dulint
row_get_rec_trx_id(
/*===============*/
				/* out: value of the field */
	rec_t*		rec,	/* in: record */
	dict_index_t*	index);	/* in: clustered index */
/*************************************************************************
Reads the roll pointer field from a clustered index record. */
UNIV_INLINE
dulint
row_get_rec_roll_ptr(
/*=================*/
				/* out: value of the field */
	rec_t*		rec,	/* in: record */
	dict_index_t*	index);	/* in: clustered index */
/*************************************************************************
Writes the trx id field to a clustered index record. */
UNIV_INLINE
void
row_set_rec_trx_id(
/*===============*/
	rec_t*		rec,	/* in: record */
	dict_index_t*	index,	/* in: clustered index */
	dulint		trx_id);	/* in: value of the field */
/*************************************************************************
Sets the roll pointer field in a clustered index record. */
UNIV_INLINE
void
row_set_rec_roll_ptr(
/*=================*/
	rec_t*		rec,	/* in: record */
	dict_index_t*	index,	/* in: clustered index */
	dulint		roll_ptr);/* in: value of the field */
/*********************************************************************
When an insert to a table is performed, this function builds the entry which
has to be inserted to an index on the table. */

dtuple_t*
row_build_index_entry(
/*==================*/
				/* out: index entry which should be inserted */
	dtuple_t*	row, 	/* in: row which should be inserted to the
				table */
	dict_index_t*	index, 	/* in: index on the table */
	mem_heap_t*	heap);	/* in: memory heap from which the memory for
				the index entry is allocated */
/*********************************************************************
Builds an index entry from a row. */

void
row_build_index_entry_to_tuple(
/*===========================*/
	dtuple_t*	entry,	/* in/out: index entry; the dtuple must have
				enough fields for the index! */
	dtuple_t*	row, 	/* in: row */
	dict_index_t*	index); /* in: index on the table */
/***********************************************************************
An inverse function to dict_row_build_index_entry. Builds a row from a
record in a clustered index. */

dtuple_t*
row_build(
/*======*/
				/* out, own: row built; see the NOTE below! */
	ulint		type,	/* in: ROW_COPY_DATA, or ROW_COPY_POINTERS:
				the former copies also the data fields to
				heap as the latter only places pointers to
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
	mem_heap_t*	heap);	/* in: memory heap from which the memory
				needed is allocated */
/***********************************************************************
An inverse function to dict_row_build_index_entry. Builds a row from a
record in a clustered index. */

void
row_build_to_tuple(
/*===============*/
	dtuple_t*	row,	/* in/out: row built; see the NOTE below! */
	dict_index_t*	index,	/* in: clustered index */
	rec_t*		rec);	/* in: record in the clustered index;
				NOTE: the data fields in the row will point
				directly into this record, therefore,
				the buffer page of this record must be
				at least s-latched and the latch held
				as long as the row dtuple is used! */
/***********************************************************************
Converts an index record to a typed data tuple. */

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
	mem_heap_t*	heap);	/* in: memory heap from which the memory
				needed is allocated */
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
	mem_heap_t*	heap);	/* in: memory heap from which the memory
				needed is allocated */
/***********************************************************************
Builds from a secondary index record a row reference with which we can
search the clustered index record. */

void
row_build_row_ref_in_tuple(
/*=======================*/
	dtuple_t*	ref,	/* in/out: row reference built; see the
				NOTE below! */
	dict_index_t*	index,	/* in: index */
	rec_t*		rec);	/* in: record in the index;
				NOTE: the data fields in ref will point
				directly into this record, therefore,
				the buffer page of this record must be
				at least s-latched and the latch held
				as long as the row reference is used! */
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
	dtuple_t*	row);	/* in: row
				NOTE: the data fields in ref will point
				directly into data of this row */
/***********************************************************************
Builds from a secondary index record a row reference with which we can
search the clustered index record. */
UNIV_INLINE
void
row_build_row_ref_fast(
/*===================*/
	dtuple_t*	ref,	/* in: typed data tuple where the reference
				is built */
	ulint*		map,	/* in: array of field numbers in rec telling
				how ref should be built from the fields of
				rec */
	rec_t*		rec);	/* in: record in the index; must be preserved
				while ref is used, as we do not copy field
				values to heap */
/*******************************************************************
Searches the clustered index record for a row, if we have the row
reference. */

ibool
row_search_on_row_ref(
/*==================*/
				/* out: TRUE if found */
	btr_pcur_t*	pcur,	/* in/out: persistent cursor, which must
				be closed by the caller */
	ulint		mode,	/* in: BTR_MODIFY_LEAF, ... */
	dict_table_t*	table,	/* in: table */
	dtuple_t*	ref,	/* in: row reference */
	mtr_t*		mtr);	/* in: mtr */
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
	mtr_t*		mtr);	/* in: mtr */
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
	mtr_t*		mtr);	/* in: mtr */

	
#define ROW_COPY_DATA		1
#define ROW_COPY_POINTERS	2

/* The allowed latching order of index records is the following:
(1) a secondary index record ->
(2) the clustered index record ->
(3) rollback segment data for the clustered index record.

No new latches may be obtained while the kernel mutex is reserved.
However, the kernel mutex can be reserved while latches are owned. */

#ifndef UNIV_NONINL
#include "row0row.ic"
#endif

#endif 
