/*****************************************************************************

Copyright (c) 1996, 2016, Oracle and/or its affiliates. All Rights Reserved.

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
@file include/row0upd.h
Update of a row

Created 12/27/1996 Heikki Tuuri
*******************************************************/

#ifndef row0upd_h
#define row0upd_h

#include "univ.i"
#include "data0data.h"
#include "row0types.h"
#include "btr0types.h"
#include "dict0types.h"
#include "trx0types.h"
#include <stack>

#ifndef UNIV_HOTBACKUP
# include "btr0pcur.h"
# include "que0types.h"
# include "pars0types.h"
#endif /* !UNIV_HOTBACKUP */

/** The std::deque to store cascade update nodes, that uses mem_heap_t
as allocator. */
typedef std::deque<upd_node_t*, mem_heap_allocator<upd_node_t*> >
	deque_mem_heap_t;

/** Double-ended queue of update nodes to be processed for cascade
operations */
typedef deque_mem_heap_t upd_cascade_t;

/*********************************************************************//**
Creates an update vector object.
@return own: update vector object */
UNIV_INLINE
upd_t*
upd_create(
/*=======*/
	ulint		n,	/*!< in: number of fields */
	mem_heap_t*	heap);	/*!< in: heap from which memory allocated */
/*********************************************************************//**
Returns the number of fields in the update vector == number of columns
to be updated by an update vector.
@return number of fields */
UNIV_INLINE
ulint
upd_get_n_fields(
/*=============*/
	const upd_t*	update);	/*!< in: update vector */
#ifdef UNIV_DEBUG
/*********************************************************************//**
Returns the nth field of an update vector.
@return update vector field */
UNIV_INLINE
upd_field_t*
upd_get_nth_field(
/*==============*/
	const upd_t*	update,	/*!< in: update vector */
	ulint		n);	/*!< in: field position in update vector */
#else
# define upd_get_nth_field(update, n) ((update)->fields + (n))
#endif
#ifndef UNIV_HOTBACKUP
/*********************************************************************//**
Sets an index field number to be updated by an update vector field. */
UNIV_INLINE
void
upd_field_set_field_no(
/*===================*/
	upd_field_t*	upd_field,	/*!< in: update vector field */
	ulint		field_no,	/*!< in: field number in a clustered
					index */
	dict_index_t*	index,		/*!< in: index */
	trx_t*		trx);		/*!< in: transaction */

/** set field number to a update vector field, marks this field is updated
@param[in,out]	upd_field	update vector field
@param[in]	field_no	virtual column sequence num
@param[in]	index		index */
UNIV_INLINE
void
upd_field_set_v_field_no(
	upd_field_t*	upd_field,
	ulint		field_no,
	dict_index_t*	index);
/*********************************************************************//**
Returns a field of an update vector by field_no.
@return update vector field, or NULL */
UNIV_INLINE
const upd_field_t*
upd_get_field_by_field_no(
/*======================*/
	const upd_t*	update,	/*!< in: update vector */
	ulint		no,	/*!< in: field_no */
	bool		is_virtual) /*!< in: if it is a virtual column */
	MY_ATTRIBUTE((warn_unused_result));
/*********************************************************************//**
Writes into the redo log the values of trx id and roll ptr and enough info
to determine their positions within a clustered index record.
@return new pointer to mlog */
byte*
row_upd_write_sys_vals_to_log(
/*==========================*/
	dict_index_t*	index,	/*!< in: clustered index */
	trx_id_t	trx_id,	/*!< in: transaction id */
	roll_ptr_t	roll_ptr,/*!< in: roll ptr of the undo log record */
	byte*		log_ptr,/*!< pointer to a buffer of size > 20 opened
				in mlog */
	mtr_t*		mtr);	/*!< in: mtr */
/*********************************************************************//**
Updates the trx id and roll ptr field in a clustered index record when
a row is updated or marked deleted. */
UNIV_INLINE
void
row_upd_rec_sys_fields(
/*===================*/
	rec_t*		rec,	/*!< in/out: record */
	page_zip_des_t*	page_zip,/*!< in/out: compressed page whose
				uncompressed part will be updated, or NULL */
	dict_index_t*	index,	/*!< in: clustered index */
	const ulint*	offsets,/*!< in: rec_get_offsets(rec, index) */
	const trx_t*	trx,	/*!< in: transaction */
	roll_ptr_t	roll_ptr);/*!< in: roll ptr of the undo log record,
				  can be 0 during IMPORT */
/*********************************************************************//**
Sets the trx id or roll ptr field of a clustered index entry. */
void
row_upd_index_entry_sys_field(
/*==========================*/
	dtuple_t*	entry,	/*!< in/out: index entry, where the memory
				buffers for sys fields are already allocated:
				the function just copies the new values to
				them */
	dict_index_t*	index,	/*!< in: clustered index */
	ulint		type,	/*!< in: DATA_TRX_ID or DATA_ROLL_PTR */
	ib_uint64_t	val);	/*!< in: value to write */
/*********************************************************************//**
Creates an update node for a query graph.
@return own: update node */
upd_node_t*
upd_node_create(
/*============*/
	mem_heap_t*	heap);	/*!< in: mem heap where created */
/***********************************************************//**
Writes to the redo log the new values of the fields occurring in the index. */
void
row_upd_index_write_log(
/*====================*/
	const upd_t*	update,	/*!< in: update vector */
	byte*		log_ptr,/*!< in: pointer to mlog buffer: must
				contain at least MLOG_BUF_MARGIN bytes
				of free space; the buffer is closed
				within this function */
	mtr_t*		mtr);	/*!< in: mtr into whose log to write */
/***********************************************************//**
Returns TRUE if row update changes size of some field in index or if some
field to be updated is stored externally in rec or update.
@return TRUE if the update changes the size of some field in index or
the field is external in rec or update */
ibool
row_upd_changes_field_size_or_external(
/*===================================*/
	dict_index_t*	index,	/*!< in: index */
	const ulint*	offsets,/*!< in: rec_get_offsets(rec, index) */
	const upd_t*	update);/*!< in: update vector */
/***********************************************************//**
Returns true if row update contains disowned external fields.
@return true if the update contains disowned external fields. */
bool
row_upd_changes_disowned_external(
/*==============================*/
	const upd_t*	update)	/*!< in: update vector */
	MY_ATTRIBUTE((warn_unused_result));
#endif /* !UNIV_HOTBACKUP */
/***********************************************************//**
Replaces the new column values stored in the update vector to the
record given. No field size changes are allowed. This function is
usually invoked on a clustered index. The only use case for a
secondary index is row_ins_sec_index_entry_by_modify() or its
counterpart in ibuf_insert_to_index_page(). */
void
row_upd_rec_in_place(
/*=================*/
	rec_t*		rec,	/*!< in/out: record where replaced */
	dict_index_t*	index,	/*!< in: the index the record belongs to */
	const ulint*	offsets,/*!< in: array returned by rec_get_offsets() */
	const upd_t*	update,	/*!< in: update vector */
	page_zip_des_t*	page_zip);/*!< in: compressed page with enough space
				available, or NULL */
#ifndef UNIV_HOTBACKUP
/***************************************************************//**
Builds an update vector from those fields which in a secondary index entry
differ from a record that has the equal ordering fields. NOTE: we compare
the fields as binary strings!
@return own: update vector of differing fields */
upd_t*
row_upd_build_sec_rec_difference_binary(
/*====================================*/
	const rec_t*	rec,	/*!< in: secondary index record */
	dict_index_t*	index,	/*!< in: index */
	const ulint*	offsets,/*!< in: rec_get_offsets(rec, index) */
	const dtuple_t*	entry,	/*!< in: entry to insert */
	mem_heap_t*	heap)	/*!< in: memory heap from which allocated */
	MY_ATTRIBUTE((warn_unused_result));
/** Builds an update vector from those fields, excluding the roll ptr and
trx id fields, which in an index entry differ from a record that has
the equal ordering fields. NOTE: we compare the fields as binary strings!
@param[in]	index		clustered index
@param[in]	entry		clustered index entry to insert
@param[in]	rec		clustered index record
@param[in]	offsets		rec_get_offsets(rec,index), or NULL
@param[in]	no_sys		skip the system columns
				DB_TRX_ID and DB_ROLL_PTR
@param[in]	trx		transaction (for diagnostics),
				or NULL
@param[in]	heap		memory heap from which allocated
@param[in,out]	mysql_table	NULL, or mysql table object when
				user thread invokes dml
@return own: update vector of differing fields, excluding roll ptr and
trx id */
upd_t*
row_upd_build_difference_binary(
	dict_index_t*	index,
	const dtuple_t*	entry,
	const rec_t*	rec,
	const ulint*	offsets,
	bool		no_sys,
	trx_t*		trx,
	mem_heap_t*	heap,
	TABLE*		mysql_table)
	MY_ATTRIBUTE((warn_unused_result));
/***********************************************************//**
Replaces the new column values stored in the update vector to the index entry
given. */
void
row_upd_index_replace_new_col_vals_index_pos(
/*=========================================*/
	dtuple_t*	entry,	/*!< in/out: index entry where replaced;
				the clustered index record must be
				covered by a lock or a page latch to
				prevent deletion (rollback or purge) */
	dict_index_t*	index,	/*!< in: index; NOTE that this may also be a
				non-clustered index */
	const upd_t*	update,	/*!< in: an update vector built for the index so
				that the field number in an upd_field is the
				index position */
	ibool		order_only,
				/*!< in: if TRUE, limit the replacement to
				ordering fields of index; note that this
				does not work for non-clustered indexes. */
	mem_heap_t*	heap);	/*!< in: memory heap for allocating and
				copying the new values */
/***********************************************************//**
Replaces the new column values stored in the update vector to the index entry
given. */
void
row_upd_index_replace_new_col_vals(
/*===============================*/
	dtuple_t*	entry,	/*!< in/out: index entry where replaced;
				the clustered index record must be
				covered by a lock or a page latch to
				prevent deletion (rollback or purge) */
	dict_index_t*	index,	/*!< in: index; NOTE that this may also be a
				non-clustered index */
	const upd_t*	update,	/*!< in: an update vector built for the
				CLUSTERED index so that the field number in
				an upd_field is the clustered index position */
	mem_heap_t*	heap);	/*!< in: memory heap for allocating and
				copying the new values */
/***********************************************************//**
Replaces the new column values stored in the update vector. */
void
row_upd_replace(
/*============*/
	dtuple_t*		row,	/*!< in/out: row where replaced,
					indexed by col_no;
					the clustered index record must be
					covered by a lock or a page latch to
					prevent deletion (rollback or purge) */
	row_ext_t**		ext,	/*!< out, own: NULL, or externally
					stored column prefixes */
	const dict_index_t*	index,	/*!< in: clustered index */
	const upd_t*		update,	/*!< in: an update vector built for the
					clustered index */
	mem_heap_t*		heap);	/*!< in: memory heap */
/** Replaces the virtual column values stored in a dtuple with that of
a update vector.
@param[in,out]	row	dtuple whose column to be updated
@param[in]	table	table
@param[in]	update	an update vector built for the clustered index
@param[in]	upd_new	update to new or old value
@param[in,out]	undo_row undo row (if needs to be updated)
@param[in]	ptr	remaining part in update undo log */
void
row_upd_replace_vcol(
	dtuple_t*		row,
	const dict_table_t*	table,
	const upd_t*		update,
	bool			upd_new,
	dtuple_t*		undo_row,
	const byte*		ptr);

/***********************************************************//**
Checks if an update vector changes an ordering field of an index record.

This function is fast if the update vector is short or the number of ordering
fields in the index is small. Otherwise, this can be quadratic.
NOTE: we compare the fields as binary strings!
@return TRUE if update vector changes an ordering field in the index record */
ibool
row_upd_changes_ord_field_binary_func(
/*==================================*/
	dict_index_t*	index,	/*!< in: index of the record */
	const upd_t*	update,	/*!< in: update vector for the row; NOTE: the
				field numbers in this MUST be clustered index
				positions! */
#ifdef UNIV_DEBUG
	const que_thr_t*thr,	/*!< in: query thread */
#endif /* UNIV_DEBUG */
	const dtuple_t*	row,	/*!< in: old value of row, or NULL if the
				row and the data values in update are not
				known when this function is called, e.g., at
				compile time */
	const row_ext_t*ext,	/*!< NULL, or prefixes of the externally
				stored columns in the old row */
	ulint		flag)	/*!< in: ROW_BUILD_NORMAL,
				ROW_BUILD_FOR_PURGE or ROW_BUILD_FOR_UNDO */
	MY_ATTRIBUTE((warn_unused_result));
#ifdef UNIV_DEBUG
# define row_upd_changes_ord_field_binary(index,update,thr,row,ext)	\
	row_upd_changes_ord_field_binary_func(index,update,thr,row,ext,0)
#else /* UNIV_DEBUG */
# define row_upd_changes_ord_field_binary(index,update,thr,row,ext)	\
	row_upd_changes_ord_field_binary_func(index,update,row,ext,0)
#endif /* UNIV_DEBUG */
/***********************************************************//**
Checks if an FTS indexed column is affected by an UPDATE.
@return offset within fts_t::indexes if FTS indexed column updated else
ULINT_UNDEFINED */
ulint
row_upd_changes_fts_column(
/*=======================*/
	dict_table_t*	table,		/*!< in: table */
	upd_field_t*	upd_field);	/*!< in: field to check */
/***********************************************************//**
Checks if an FTS Doc ID column is affected by an UPDATE.
@return whether Doc ID column is affected */
bool
row_upd_changes_doc_id(
/*===================*/
	dict_table_t*	table,		/*!< in: table */
	upd_field_t*	upd_field)	/*!< in: field to check */
	MY_ATTRIBUTE((warn_unused_result));
/***********************************************************//**
Checks if an update vector changes an ordering field of an index record.
This function is fast if the update vector is short or the number of ordering
fields in the index is small. Otherwise, this can be quadratic.
NOTE: we compare the fields as binary strings!
@return TRUE if update vector may change an ordering field in an index
record */
ibool
row_upd_changes_some_index_ord_field_binary(
/*========================================*/
	const dict_table_t*	table,	/*!< in: table */
	const upd_t*		update);/*!< in: update vector for the row */
/** Stores to the heap the row on which the node->pcur is positioned.
@param[in]	node		row update node
@param[in]	thd		mysql thread handle
@param[in,out]	mysql_table	NULL, or mysql table object when
				user thread invokes dml */
void
row_upd_store_row(
	upd_node_t*	node,
	THD*		thd,
	TABLE*		mysql_table);
/***********************************************************//**
Updates a row in a table. This is a high-level function used
in SQL execution graphs.
@return query thread to run next or NULL */
que_thr_t*
row_upd_step(
/*=========*/
	que_thr_t*	thr);	/*!< in: query thread */
#endif /* !UNIV_HOTBACKUP */
/*********************************************************************//**
Parses the log data of system field values.
@return log data end or NULL */
byte*
row_upd_parse_sys_vals(
/*===================*/
	const byte*	ptr,	/*!< in: buffer */
	const byte*	end_ptr,/*!< in: buffer end */
	ulint*		pos,	/*!< out: TRX_ID position in record */
	trx_id_t*	trx_id,	/*!< out: trx id */
	roll_ptr_t*	roll_ptr);/*!< out: roll ptr */
/*********************************************************************//**
Updates the trx id and roll ptr field in a clustered index record in database
recovery. */
void
row_upd_rec_sys_fields_in_recovery(
/*===============================*/
	rec_t*		rec,	/*!< in/out: record */
	page_zip_des_t*	page_zip,/*!< in/out: compressed page, or NULL */
	const ulint*	offsets,/*!< in: array returned by rec_get_offsets() */
	ulint		pos,	/*!< in: TRX_ID position in rec */
	trx_id_t	trx_id,	/*!< in: transaction id */
	roll_ptr_t	roll_ptr);/*!< in: roll ptr of the undo log record */
/*********************************************************************//**
Parses the log data written by row_upd_index_write_log.
@return log data end or NULL */
byte*
row_upd_index_parse(
/*================*/
	const byte*	ptr,	/*!< in: buffer */
	const byte*	end_ptr,/*!< in: buffer end */
	mem_heap_t*	heap,	/*!< in: memory heap where update vector is
				built */
	upd_t**		update_out);/*!< out: update vector */


/* Update vector field */
struct upd_field_t{
	unsigned	field_no:16;	/*!< field number in an index, usually
					the clustered index, but in updating
					a secondary index record in btr0cur.cc
					this is the position in the secondary
					index. If this field is a virtual
					column, then field_no represents
					the nth virtual	column in the table */
#ifndef UNIV_HOTBACKUP
	unsigned	orig_len:16;	/*!< original length of the locally
					stored part of an externally stored
					column, or 0 */
	que_node_t*	exp;		/*!< expression for calculating a new
					value: it refers to column values and
					constants in the symbol table of the
					query graph */
#endif /* !UNIV_HOTBACKUP */
	dfield_t	new_val;	/*!< new value for the column */
	dfield_t*	old_v_val;	/*!< old value for the virtual column */
};


/* check whether an update field is on virtual column */
#define upd_fld_is_virtual_col(upd_fld)			\
	(((upd_fld)->new_val.type.prtype & DATA_VIRTUAL) == DATA_VIRTUAL)

/* set DATA_VIRTUAL bit on update field to show it is a virtual column */
#define upd_fld_set_virtual_col(upd_fld)			\
	((upd_fld)->new_val.type.prtype |= DATA_VIRTUAL)

/* Update vector structure */
struct upd_t{
	mem_heap_t*	heap;		/*!< heap from which memory allocated */
	ulint		info_bits;	/*!< new value of info bits to record;
					default is 0 */
	dtuple_t*	old_vrow;	/*!< pointer to old row, used for
					virtual column update now */
	ulint		n_fields;	/*!< number of update fields */
	upd_field_t*	fields;		/*!< array of update fields */

	/** Append an update field to the end of array
	@param[in]	field	an update field */
	void append(const upd_field_t& field)
	{
		fields[n_fields++] = field;
	}

	/** Determine if the given field_no is modified.
	@return true if modified, false otherwise.  */
	bool is_modified(const ulint field_no) const
	{
		for (ulint i = 0; i < n_fields; ++i) {
			if (field_no == fields[i].field_no) {
				return(true);
			}
		}
		return(false);
	}

#ifdef UNIV_DEBUG
        bool validate() const
        {
                for (ulint i = 0; i < n_fields; ++i) {
                        dfield_t* field = &fields[i].new_val;
                        if (dfield_is_ext(field)) {
				ut_ad(dfield_get_len(field)
				      >= BTR_EXTERN_FIELD_REF_SIZE);
                        }
                }
                return(true);
        }
#endif // UNIV_DEBUG

};

#ifndef UNIV_HOTBACKUP
/* Update node structure which also implements the delete operation
of a row */

struct upd_node_t{
	que_common_t	common;	/*!< node type: QUE_NODE_UPDATE */
	ibool		is_delete;/* TRUE if delete, FALSE if update */
	ibool		searched_update;
				/* TRUE if searched update, FALSE if
				positioned */
	ibool		in_mysql_interface;
				/* TRUE if the update node was created
				for the MySQL interface */
	dict_foreign_t*	foreign;/* NULL or pointer to a foreign key
				constraint if this update node is used in
				doing an ON DELETE or ON UPDATE operation */

	bool		cascade_top;
				/*!< true if top level in cascade */

	upd_cascade_t*	cascade_upd_nodes;
				/*!< Queue of update nodes to handle the
				cascade of update and delete operations in an
				iterative manner.  Their parent/child
				relations are properly maintained. All update
				nodes point to this same queue.  All these
				nodes are allocated in heap pointed to by
				upd_node_t::cascade_heap. */

	upd_cascade_t*	new_upd_nodes;
				/*!< Intermediate list of update nodes in a
				cascading update/delete operation.  After
				processing one update node, this will be
				concatenated to cascade_upd_nodes.  This extra
				list is needed so that retry because of
				DB_LOCK_WAIT works corrrectly. */

	upd_cascade_t*	processed_cascades;
				/*!< List of processed update nodes in a
				cascading update/delete operation.  All the
				cascade nodes are stored here, so that memory
				can be freed. */

	mem_heap_t*	cascade_heap;
				/*!< NULL or a mem heap where cascade_upd_nodes
				are created.  This heap is owned by the node
				that has cascade_top=true. */

	sel_node_t*	select;	/*!< query graph subtree implementing a base
				table cursor: the rows returned will be
				updated */
	btr_pcur_t*	pcur;	/*!< persistent cursor placed on the clustered
				index record which should be updated or
				deleted; the cursor is stored in the graph
				of 'select' field above, except in the case
				of the MySQL interface */
	dict_table_t*	table;	/*!< table where updated */
	upd_t*		update;	/*!< update vector for the row */
	ulint		update_n_fields;
				/* when this struct is used to implement
				a cascade operation for foreign keys, we store
				here the size of the buffer allocated for use
				as the update vector */
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
	ulint		state;	/*!< node execution state */
	dict_index_t*	index;	/*!< NULL, or the next index whose record should
				be updated */
	dtuple_t*	row;	/*!< NULL, or a copy (also fields copied to
				heap) of the row to update; this must be reset
				to NULL after a successful update */
	row_ext_t*	ext;	/*!< NULL, or prefixes of the externally
				stored columns in the old row */
	dtuple_t*	upd_row;/* NULL, or a copy of the updated row */
	row_ext_t*	upd_ext;/* NULL, or prefixes of the externally
				stored columns in upd_row */
	mem_heap_t*	heap;	/*!< memory heap used as auxiliary storage;
				this must be emptied after a successful
				update */
	/*----------------------*/
	sym_node_t*	table_sym;/* table node in symbol table */
	que_node_t*	col_assign_list;
				/* column assignment list */

	doc_id_t	fts_doc_id;
				/* The FTS doc id of the row that is now
				pointed to by the pcur. */

	doc_id_t	fts_next_doc_id;
				/* The new fts doc id that will be used
				in update operation */

	ulint		magic_n;

#ifndef DBUG_OFF
	/** Print information about this object into the trace log file. */
	void dbug_trace();

	/** Ensure that the member cascade_upd_nodes has only one update node
	for each of the tables.  This is useful for testing purposes. */
	void check_cascade_only_once();
#endif /* !DBUG_OFF */
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
#define UPD_NODE_UPDATE_ALL_SEC	   5	/* an ordering field of the clustered
					index record was changed, or this is
					a delete operation: should update
					all the secondary index records */
#define UPD_NODE_UPDATE_SOME_SEC   6	/* secondary index entries should be
					looked at and updated if an ordering
					field changed */

/* Compilation info flags: these must fit within 3 bits; see trx0rec.h */
#define UPD_NODE_NO_ORD_CHANGE	1	/* no secondary index record will be
					changed in the update and no ordering
					field of the clustered index */
#define UPD_NODE_NO_SIZE_CHANGE	2	/* no record field size will be
					changed in the update */

#endif /* !UNIV_HOTBACKUP */

#ifndef UNIV_NONINL
#include "row0upd.ic"
#endif

#endif
