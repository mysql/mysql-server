/******************************************************
Update of a row

(c) 1996 Innobase Oy

Created 12/27/1996 Heikki Tuuri
*******************************************************/

#ifndef row0upd_h
#define row0upd_h

#include "univ.i"
#include "data0data.h"
#include "btr0types.h"
#include "btr0pcur.h"
#include "dict0types.h"
#include "trx0types.h"
#include "que0types.h"
#include "row0types.h"
#include "pars0types.h"
	
/*************************************************************************
Creates an update vector object. */
UNIV_INLINE
upd_t*
upd_create(
/*=======*/
				/* out, own: update vector object */
	ulint		n,	/* in: number of fields */
	mem_heap_t*	heap);	/* in: heap from which memory allocated */
/*************************************************************************
Returns the number of fields in the update vector == number of columns
to be updated by an update vector. */
UNIV_INLINE
ulint
upd_get_n_fields(
/*=============*/
			/* out: number of fields */
	upd_t*	update);	/* in: update vector */
/*************************************************************************
Returns the nth field of an update vector. */
UNIV_INLINE
upd_field_t*
upd_get_nth_field(
/*==============*/
			/* out: update vector field */
	upd_t*	update,	/* in: update vector */
	ulint	n);	/* in: field position in update vector */
/*************************************************************************
Sets the clustered index field number to be updated by an update vector
field. */
UNIV_INLINE
void
upd_field_set_field_no(
/*===================*/
	upd_field_t*	upd_field,	/* in: update vector field */
	ulint		field_no,	/* in: field number in a clustered
					index */
	dict_index_t*	index);		/* in: clustered index */
/*************************************************************************
Writes into the redo log the values of trx id and roll ptr and enough info
to determine their positions within a clustered index record. */

byte*
row_upd_write_sys_vals_to_log(
/*==========================*/
				/* out: new pointer to mlog */
	dict_index_t*	index,	/* in: clustered index */
	trx_t*		trx,	/* in: transaction */
	dulint		roll_ptr,/* in: roll ptr of the undo log record */
	byte*		log_ptr,/* pointer to a buffer of size > 20 opened
				in mlog */
	mtr_t*		mtr);	/* in: mtr */
/*************************************************************************
Updates the trx id and roll ptr field in a clustered index record when
a row is updated or marked deleted. */
UNIV_INLINE
void
row_upd_rec_sys_fields(
/*===================*/
	rec_t*		rec,	/* in: record */
	dict_index_t*	index,	/* in: clustered index */
	trx_t*		trx,	/* in: transaction */
	dulint		roll_ptr);/* in: roll ptr of the undo log record */
/*************************************************************************
Sets the trx id or roll ptr field of a clustered index entry. */

void
row_upd_index_entry_sys_field(
/*==========================*/
	dtuple_t*	entry,	/* in: index entry, where the memory buffers
				for sys fields are already allocated:
				the function just copies the new values to
				them */
	dict_index_t*	index,	/* in: clustered index */
	ulint		type,	/* in: DATA_TRX_ID or DATA_ROLL_PTR */
	dulint		val);	/* in: value to write */
/*************************************************************************
Creates an update node for a query graph. */

upd_node_t*
upd_node_create(
/*============*/
				/* out, own: update node */
	mem_heap_t*	heap);	/* in: mem heap where created */
/***************************************************************
Writes to the redo log the new values of the fields occurring in the index. */

void
row_upd_index_write_log(
/*====================*/
	upd_t*	update,	/* in: update vector */
	byte*	log_ptr,/* in: pointer to mlog buffer: must contain at least
			MLOG_BUF_MARGIN bytes of free space; the buffer is
			closed within this function */
	mtr_t*	mtr);	/* in: mtr into whose log to write */
/***************************************************************
Returns TRUE if row update changes size of some field in index. */

ibool
row_upd_changes_field_size(
/*=======================*/
				/* out: TRUE if the update changes the size of
				some field in index */		
	rec_t*		rec,	/* in: record in clustered index */
	dict_index_t*	index,	/* in: clustered index */
	upd_t*		update);/* in: update vector */
/***************************************************************
Replaces the new column values stored in the update vector to the record
given. No field size changes are allowed. This function is used only for
a clustered index */

void
row_upd_rec_in_place(
/*=================*/
	rec_t*	rec,	/* in/out: record where replaced */
	upd_t*	update);/* in: update vector */
/*******************************************************************
Builds an update vector from those fields, excluding the roll ptr and
trx id fields, which in an index entry differ from a record that has
the equal ordering fields. */

upd_t*
row_upd_build_difference(
/*=====================*/
				/* out, own: update vector of differing
				fields, excluding roll ptr and trx id */
	dict_index_t*	index,	/* in: clustered index */
	dtuple_t*	entry,	/* in: entry to insert */
	ulint*		ext_vec,/* in: array containing field numbers of
				externally stored fields in entry, or NULL */
	ulint		n_ext_vec,/* in: number of fields in ext_vec */
	rec_t*		rec,	/* in: clustered index record */
	mem_heap_t*	heap);	/* in: memory heap from which allocated */
/***************************************************************
Replaces the new column values stored in the update vector to the index entry
given. */

void
row_upd_index_replace_new_col_vals(
/*===============================*/
	dtuple_t*	entry,	/* in/out: index entry where replaced */
	dict_index_t*	index,	/* in: index; NOTE that may also be a
				non-clustered index */
	upd_t*		update);	/* in: update vector */
/***************************************************************
Replaces the new column values stored in the update vector to the
clustered index entry given. */

void
row_upd_clust_index_replace_new_col_vals(
/*=====================================*/
	dtuple_t*	entry,	/* in/out: index entry where replaced */
	upd_t*		update);	/* in: update vector */
/***************************************************************
Checks if an update vector changes an ordering field of an index record.
This function is fast if the update vector is short or the number of ordering
fields in the index is small. Otherwise, this can be quadratic. */

ibool
row_upd_changes_ord_field(
/*======================*/
				/* out: TRUE if update vector changes
				an ordering field in the index record */
	dtuple_t*	row,	/* in: old value of row, or NULL if the
				row and the data values in update are not
				known when this function is called, e.g., at
				compile time */
	dict_index_t*	index,	/* in: index of the record */
	upd_t*		update);/* in: update vector for the row */
/***************************************************************
Checks if an update vector changes an ordering field of an index record.
This function is fast if the update vector is short or the number of ordering
fields in the index is small. Otherwise, this can be quadratic. */

ibool
row_upd_changes_some_index_ord_field(
/*=================================*/
				/* out: TRUE if update vector may change
				an ordering field in an index record */
	dict_table_t*	table,	/* in: table */
	upd_t*		update);/* in: update vector for the row */
/***************************************************************
Updates a row in a table. This is a high-level function used
in SQL execution graphs. */

que_thr_t*
row_upd_step(
/*=========*/
				/* out: query thread to run next or NULL */
	que_thr_t*	thr);	/* in: query thread */
/*************************************************************************
Performs an in-place update for the current clustered index record in
select. */

void
row_upd_in_place_in_select(
/*=======================*/
	sel_node_t*	sel_node,	/* in: select node */
	que_thr_t*	thr,		/* in: query thread */
	mtr_t*		mtr);		/* in: mtr */
/*************************************************************************
Parses the log data of system field values. */

byte*
row_upd_parse_sys_vals(
/*===================*/
			/* out: log data end or NULL */
	byte*	ptr,	/* in: buffer */
	byte*	end_ptr,/* in: buffer end */
	ulint*	pos,	/* out: TRX_ID position in record */
	dulint*	trx_id,	/* out: trx id */
	dulint*	roll_ptr);/* out: roll ptr */
/*************************************************************************
Updates the trx id and roll ptr field in a clustered index record in database
recovery. */

void
row_upd_rec_sys_fields_in_recovery(
/*===============================*/
	rec_t*	rec,	/* in: record */
	ulint	pos,	/* in: TRX_ID position in rec */
	dulint	trx_id,	/* in: transaction id */
	dulint	roll_ptr);/* in: roll ptr of the undo log record */
/*************************************************************************
Parses the log data written by row_upd_index_write_log. */

byte*
row_upd_index_parse(
/*================*/
				/* out: log data end or NULL */
	byte*		ptr,	/* in: buffer */
	byte*		end_ptr,/* in: buffer end */
	mem_heap_t*	heap,	/* in: memory heap where update vector is
				built */
	upd_t**		update_out);/* out: update vector */


/* Update vector field */
struct upd_field_struct{
	ulint		field_no;	/* field number in the clustered
					index */
	que_node_t*	exp;		/* expression for calculating a new
					value: it refers to column values and
					constants in the symbol table of the
					query graph */
	dfield_t	new_val;	/* new value for the column */
	ibool		extern_storage;	/* this is set to TRUE if dfield
					actually contains a reference to
					an externally stored field */
};

/* Update vector structure */
struct upd_struct{
	ulint		info_bits;	/* new value of info bits to record;
					default is 0 */
	ulint		n_fields;	/* number of update fields */
	upd_field_t*	fields;		/* array of update fields */
};

/* Update node structure which also implements the delete operation
of a row */

struct upd_node_struct{
	que_common_t	common;	/* node type: QUE_NODE_UPDATE */
	ibool		is_delete;/* TRUE if delete, FALSE if update */
	ibool		searched_update;
				/* TRUE if searched update, FALSE if
				positioned */
	ibool		select_will_do_update;
				/* TRUE if a searched update where ordering
				fields will not be updated, and the size of
				the fields will not change: in this case the
				select node will take care of the update */
	ibool		in_mysql_interface;
				/* TRUE if the update node was created
				for the MySQL interface */
	sel_node_t*	select;	/* query graph subtree implementing a base
				table cursor: the rows returned will be
				updated */
	btr_pcur_t*	pcur;	/* persistent cursor placed on the clustered
				index record which should be updated or
				deleted; the cursor is stored in the graph
				of 'select' field above, except in the case
				of the MySQL interface */
	dict_table_t*	table;	/* table where updated */
	upd_t*		update;	/* update vector for the row */
	sym_node_list_t	columns;/* symbol table nodes for the columns
				to retrieve from the table */
	ibool		has_clust_rec_x_lock;
				/* TRUE if the select which retrieves the
				records to update already sets an x-lock on
				the clustered record; note that it must always
				set at least an s-lock */
	ulint		cmpl_info;/* information extracted during query
				compilation; speeds up execution:
				UPD_NODE_NO_ORD_CHANGE and
				UPD_NODE_NO_SIZE_CHANGE, ORed */
	/*----------------------*/
	/* Local storage for this graph node */
	ulint		state;	/* node execution state */
	dict_index_t*	index;	/* NULL, or the next index whose record should
				be updated */
	dtuple_t*	row;	/* NULL, or a copy (also fields copied to
				heap) of the row to update; this must be reset
				to NULL after a successful update */
	ulint*		ext_vec;/* array describing which fields are stored
				externally in the clustered index record of
				row */
	ulint		n_ext_vec;/* number of fields in ext_vec */
	mem_heap_t*	heap;	/* memory heap used as auxiliary storage for
				row; this must be emptied after a successful
				update if node->row != NULL */
	/*----------------------*/
	sym_node_t*	table_sym;/* table node in symbol table */
	que_node_t*	col_assign_list;
				/* column assignment list */
	ulint		magic_n;
};

#define	UPD_NODE_MAGIC_N	1579975

/* Node execution states */
#define UPD_NODE_SET_IX_LOCK	   1	/* execution came to the node from
					a node above and if the field
					has_clust_rec_x_lock is FALSE, we
					should set an intention x-lock on
					the table */
#define UPD_NODE_UPDATE_CLUSTERED  2	/* clustered index record should be
					updated */
#define UPD_NODE_INSERT_CLUSTERED  3	/* clustered index record should be
					inserted, old record is already delete
					marked */
#define UPD_NODE_UPDATE_ALL_SEC	   4	/* an ordering field of the clustered
					index record was changed, or this is
					a delete operation: should update
					all the secondary index records */
#define	UPD_NODE_UPDATE_SOME_SEC   5 	/* secondary index entries should be
					looked at and updated if an ordering
					field changed */

/* Compilation info flags: these must fit within 3 bits; see trx0rec.h */
#define UPD_NODE_NO_ORD_CHANGE	1	/* no secondary index record will be
					changed in the update and no ordering
					field of the clustered index */
#define UPD_NODE_NO_SIZE_CHANGE	2	/* no record field size will be
					changed in the update */

#ifndef UNIV_NONINL
#include "row0upd.ic"
#endif

#endif 
