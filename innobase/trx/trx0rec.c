/******************************************************
Transaction undo log record

(c) 1996 Innobase Oy

Created 3/26/1996 Heikki Tuuri
*******************************************************/

#include "trx0rec.h"

#ifdef UNIV_NONINL
#include "trx0rec.ic"
#endif

#include "fsp0fsp.h"
#include "mach0data.h"
#include "trx0rseg.h"
#include "trx0trx.h"
#include "trx0undo.h"
#include "dict0dict.h"
#include "ut0mem.h"
#include "row0upd.h"
#include "que0que.h"
#include "trx0purge.h"
#include "row0row.h"

/*=========== UNDO LOG RECORD CREATION AND DECODING ====================*/

/**************************************************************************
Writes the mtr log entry of the inserted undo log record on the undo log
page. */
UNIV_INLINE
void
trx_undof_page_add_undo_rec_log(
/*============================*/
	page_t* undo_page,	/* in: undo log page */
	ulint	old_free,	/* in: start offset of the inserted entry */
	ulint	new_free,	/* in: end offset of the entry */
	mtr_t*	mtr)		/* in: mtr */
{
	byte*	log_ptr;
	ulint	len;

#ifdef notdefined
	ulint	i;
	byte*	prev_rec_ptr;
	byte*	ptr;
	ulint	min_len;

	ut_ad(new_free >= old_free + 4);

	i = 0;
	ptr = undo_page + old_free + 2;
	
	if (old_free > mach_read_from_2(undo_page + TRX_UNDO_PAGE_HDR
					+ TRX_UNDO_PAGE_START)) {
		prev_rec_ptr = undo_page + mach_read_from_2(ptr - 4) + 2;

		min_len = ut_min(new_free - old_free - 4,
				 (undo_page + old_free - 2) - prev_rec_ptr); 
		for (;;) {
			if (i >= min_len) {

				break;
			} else if ((*ptr == *prev_rec_ptr)
				   || ((*ptr == *prev_rec_ptr + 1)
				       && (ptr + 1 == suffix))) {
				i++;
				ptr++;
				prev_rec_ptr++;
			} else {
				break;
			}
		}
	}
	
	mlog_write_initial_log_record(undo_page, MLOG_UNDO_INSERT, mtr);

	mlog_catenate_ulint(mtr, old_free, MLOG_2BYTES);

	mlog_catenate_ulint_compressed(mtr, i);

	mlog_catenate_string(mtr, ptr, new_free - old_free - 2 - i);
#endif
	log_ptr = mlog_open(mtr, 30 + MLOG_BUF_MARGIN);

	if (log_ptr == NULL) {

		return;
	}

	log_ptr = mlog_write_initial_log_record_fast(undo_page,
					MLOG_UNDO_INSERT, log_ptr, mtr);
	len = new_free - old_free - 4;

	mach_write_to_2(log_ptr, len);
	log_ptr += 2;

	if (len < 256) {
		ut_memcpy(log_ptr, undo_page + old_free + 2, len);
		log_ptr += len;
	}

	mlog_close(mtr, log_ptr);

	if (len >= MLOG_BUF_MARGIN) {
		mlog_catenate_string(mtr, undo_page + old_free + 2, len);
	}
}	

/***************************************************************
Parses a redo log record of adding an undo log record. */

byte*
trx_undo_parse_add_undo_rec(
/*========================*/
			/* out: end of log record or NULL */
	byte*	ptr,	/* in: buffer */
	byte*	end_ptr,/* in: buffer end */
	page_t*	page)	/* in: page or NULL */
{
	ulint	len;
	byte*	rec;
	ulint	first_free;

	if (end_ptr < ptr + 2) {

		return(NULL);
	}

	len = mach_read_from_2(ptr);
	ptr += 2;

	if (end_ptr < ptr + len) {

		return(NULL);
	}
	
	if (page == NULL) {

		return(ptr + len);
	}
	
	first_free = mach_read_from_2(page + TRX_UNDO_PAGE_HDR
							+ TRX_UNDO_PAGE_FREE);
	rec = page + first_free;
	
	mach_write_to_2(rec, first_free + 4 + len);
	mach_write_to_2(rec + 2 + len, first_free);

	mach_write_to_2(page + TRX_UNDO_PAGE_HDR + TRX_UNDO_PAGE_FREE,
							first_free + 4 + len);
	ut_memcpy(rec + 2, ptr, len);

	return(ptr + len);
}
	
/**************************************************************************
Calculates the free space left for extending an undo log record. */
UNIV_INLINE
ulint
trx_undo_left(
/*==========*/
			/* out: bytes left */
	page_t* page,	/* in: undo log page */
	byte*	ptr)	/* in: pointer to page */
{
	/* The '- 10' is a safety margin, in case we have some small
	calculation error below */

	return(UNIV_PAGE_SIZE - (ptr - page) - 10 - FIL_PAGE_DATA_END);
}

/**************************************************************************
Reports in the undo log of an insert of a clustered index record. */
static
ulint
trx_undo_page_report_insert(
/*========================*/
					/* out: offset of the inserted entry
					on the page if succeed, 0 if fail */
	page_t* 	undo_page,	/* in: undo log page */
	trx_t*		trx,		/* in: transaction */
	dict_index_t*	index,		/* in: clustered index */
	dtuple_t*	clust_entry,	/* in: index entry which will be
					inserted to the clustered index */
	mtr_t*		mtr)		/* in: mtr */
{
	ulint		first_free;
	byte*		ptr;
	ulint		len;
	dfield_t*	field;
	ulint		flen;
	ulint		i;
	
	ut_ad(mach_read_from_2(undo_page + TRX_UNDO_PAGE_HDR
				+ TRX_UNDO_PAGE_TYPE) == TRX_UNDO_INSERT);

	first_free = mach_read_from_2(undo_page + TRX_UNDO_PAGE_HDR
							+ TRX_UNDO_PAGE_FREE);
	ptr = undo_page + first_free;
	
	ut_ad(first_free <= UNIV_PAGE_SIZE);

	if (trx_undo_left(undo_page, ptr) < 30) {

		/* NOTE: the value 30 must be big enough such that the general
		fields written below fit on the undo log page */

		return(0);
	}

	/* Reserve 2 bytes for the pointer to the next undo log record */
	ptr += 2;
		
	/* Store first some general parameters to the undo log */ 
	mach_write_to_1(ptr, TRX_UNDO_INSERT_REC);
	ptr++;

	len = mach_dulint_write_much_compressed(ptr, trx->undo_no);
	ptr += len;

	len = mach_dulint_write_much_compressed(ptr, (index->table)->id);
	ptr += len;
	/*----------------------------------------*/
	/* Store then the fields required to uniquely determine the record
	to be inserted in the clustered index */

	for (i = 0; i < dict_index_get_n_unique(index); i++) {

		field = dtuple_get_nth_field(clust_entry, i);

		flen = dfield_get_len(field);

		if (trx_undo_left(undo_page, ptr) < 5) {

			return(0);
		}

		len = mach_write_compressed(ptr, flen); 
		ptr += len;

		if (flen != UNIV_SQL_NULL) {
			if (trx_undo_left(undo_page, ptr) < flen) {

				return(0);
			}

			ut_memcpy(ptr, dfield_get_data(field), flen);
			ptr += flen;
		}
	}

	if (trx_undo_left(undo_page, ptr) < 2) {

		return(0);
	}

	/*----------------------------------------*/
	/* Write pointers to the previous and the next undo log records */

	if (trx_undo_left(undo_page, ptr) < 2) {

		return(0);
	}

	mach_write_to_2(ptr, first_free);
	ptr += 2;

	mach_write_to_2(undo_page + first_free, ptr - undo_page);

	mach_write_to_2(undo_page + TRX_UNDO_PAGE_HDR + TRX_UNDO_PAGE_FREE,
							ptr - undo_page);

	/* Write the log entry to the REDO log of this change in the UNDO log */

	trx_undof_page_add_undo_rec_log(undo_page, first_free,
							ptr - undo_page, mtr);
	return(first_free);	
}

/**************************************************************************
Reads from an undo log record the general parameters. */

byte*
trx_undo_rec_get_pars(
/*==================*/
					/* out: remaining part of undo log
					record after reading these values */
	trx_undo_rec_t*	undo_rec,	/* in: undo log record */
	ulint*		type,		/* out: undo record type:
					TRX_UNDO_INSERT_REC, ... */
	ulint*		cmpl_info,	/* out: compiler info, relevant only
					for update type records */
	dulint*		undo_no,	/* out: undo log record number */
	dulint*		table_id)	/* out: table id */
{
	byte*		ptr;
	ulint		len;
	ulint		type_cmpl;

	ptr = undo_rec + 2;

	type_cmpl = mach_read_from_1(ptr);
	ptr++;
	
	*type = type_cmpl & (TRX_UNDO_CMPL_INFO_MULT - 1);
	*cmpl_info = type_cmpl / TRX_UNDO_CMPL_INFO_MULT;

	*undo_no = mach_dulint_read_much_compressed(ptr); 		
	len = mach_dulint_get_much_compressed_size(*undo_no);
	ptr += len;

	*table_id = mach_dulint_read_much_compressed(ptr); 		
	len = mach_dulint_get_much_compressed_size(*table_id);
	ptr += len;

	return(ptr);
}

/**************************************************************************
Reads from an undo log record a stored column value. */
UNIV_INLINE
byte*
trx_undo_rec_get_col_val(
/*=====================*/
			/* out: remaining part of undo log record after
			reading these values */
	byte*	ptr,	/* in: pointer to remaining part of undo log record */
	byte**	field,	/* out: pointer to stored field */
	ulint*	len)	/* out: length of the field, or UNIV_SQL_NULL */
{
	*len = mach_read_compressed(ptr); 
	ptr += mach_get_compressed_size(*len);

	*field = ptr;
	
	if (*len != UNIV_SQL_NULL) {
		ptr += *len;
	}

	return(ptr);
}

/***********************************************************************
Builds a row reference from an undo log record. */

byte*
trx_undo_rec_get_row_ref(
/*=====================*/
				/* out: pointer to remaining part of undo
				record */
	byte*		ptr,	/* in: remaining part of a copy of an undo log
				record, at the start of the row reference;
				NOTE that this copy of the undo log record must
				be preserved as long as the row reference is
				used, as we do NOT copy the data in the
				record! */
	dict_index_t*	index,	/* in: clustered index */
	dtuple_t**	ref,	/* out, own: row reference */
	mem_heap_t*	heap)	/* in: memory heap from which the memory
				needed is allocated */
{
	ulint		i;
	dfield_t*	dfield;
	byte*		field;
	ulint		len;
	ulint		ref_len;
	
	ut_ad(index && ptr && ref && heap);
	
	ref_len = dict_index_get_n_unique(index);

	*ref = dtuple_create(heap, ref_len);

	dict_index_copy_types(*ref, index, ref_len);

	for (i = 0; i < ref_len; i++) {
		dfield = dtuple_get_nth_field(*ref, i);

		ptr = trx_undo_rec_get_col_val(ptr, &field, &len);

		dfield_set_data(dfield, field, len);
	}

	return(ptr);
}	

/***********************************************************************
Skips a row reference from an undo log record. */

byte*
trx_undo_rec_skip_row_ref(
/*======================*/
				/* out: pointer to remaining part of undo
				record */
	byte*		ptr,	/* in: remaining part in update undo log
				record, at the start of the row reference */
	dict_index_t*	index)	/* in: clustered index */
{
	ulint		i;
	byte*		field;
	ulint		len;
	ulint		ref_len;
	
	ut_ad(index && ptr);
	
	ref_len = dict_index_get_n_unique(index);

	for (i = 0; i < ref_len; i++) {
		ptr = trx_undo_rec_get_col_val(ptr, &field, &len);
	}

	return(ptr);
}	

/**************************************************************************
Reports in the undo log of an update or delete marking of a clustered index
record. */
static
ulint
trx_undo_page_report_modify(
/*========================*/
					/* out: byte offset of the inserted
					undo log entry on the page if succeed,
					0 if fail */
	page_t* 	undo_page,	/* in: undo log page */
	trx_t*		trx,		/* in: transaction */
	dict_index_t*	index,		/* in: clustered index where update or
					delete marking is done */
	rec_t*		rec,		/* in: clustered index record which
					has NOT yet been modified */
	upd_t*		update,		/* in: update vector which tells the
					columns to be updated; in the case of
					a delete, this should be set to NULL */
	ulint		cmpl_info,	/* in: compiler info on secondary
					index updates */
	mtr_t*		mtr)		/* in: mtr */
{
	dict_table_t*	table;
	upd_field_t*	upd_field;
	dict_col_t*	col;
	ulint		first_free;
	byte*		ptr;
	ulint		len;
	byte* 		field;
	ulint		flen;
	ulint		pos;
	dulint		roll_ptr;
	dulint		trx_id;
	ulint		bits;
	ulint		col_no;
	byte*		old_ptr;
	ulint		type_cmpl;
	ulint		i;
	
	ut_ad(index->type & DICT_CLUSTERED);
	ut_ad(mach_read_from_2(undo_page + TRX_UNDO_PAGE_HDR
				+ TRX_UNDO_PAGE_TYPE) == TRX_UNDO_UPDATE);
	table = index->table;
	
	first_free = mach_read_from_2(undo_page + TRX_UNDO_PAGE_HDR
						+ TRX_UNDO_PAGE_FREE);
	ptr = undo_page + first_free;
						
	ut_ad(first_free <= UNIV_PAGE_SIZE);

	if (trx_undo_left(undo_page, ptr) < 50) {

		/* NOTE: the value 50 must be big enough so that the general
		fields written below fit on the undo log page */

		return(0);
	}

	/* Reserve 2 bytes for the pointer to the next undo log record */
	ptr += 2;

	/* Store first some general parameters to the undo log */ 		
	if (update) {
		if (rec_get_deleted_flag(rec)) {
			type_cmpl = TRX_UNDO_UPD_DEL_REC;
		} else {
			type_cmpl = TRX_UNDO_UPD_EXIST_REC;
		}
	} else {
		type_cmpl = TRX_UNDO_DEL_MARK_REC;
	}

	type_cmpl = type_cmpl | (cmpl_info * TRX_UNDO_CMPL_INFO_MULT);

	mach_write_to_1(ptr, type_cmpl);
	
	ptr++;
	len = mach_dulint_write_much_compressed(ptr, trx->undo_no);
	ptr += len;

	len = mach_dulint_write_much_compressed(ptr, table->id);
	ptr += len;

	/*----------------------------------------*/
	/* Store the state of the info bits */

	bits = rec_get_info_bits(rec);
	mach_write_to_1(ptr, bits);
	ptr += 1;

	/* Store the values of the system columns */
	trx_id = dict_index_rec_get_sys_col(index, DATA_TRX_ID, rec);

	roll_ptr = dict_index_rec_get_sys_col(index, DATA_ROLL_PTR, rec);	

	len = mach_dulint_write_compressed(ptr, trx_id);
	ptr += len;

	len = mach_dulint_write_compressed(ptr, roll_ptr);
	ptr += len;

	/*----------------------------------------*/
	/* Store then the fields required to uniquely determine the
	record which will be modified in the clustered index */

	for (i = 0; i < dict_index_get_n_unique(index); i++) {

		field = rec_get_nth_field(rec, i, &flen);

		if (trx_undo_left(undo_page, ptr) < 4) {

			return(0);
		}

		len = mach_write_compressed(ptr, flen); 
		ptr += len;

		if (flen != UNIV_SQL_NULL) {
			if (trx_undo_left(undo_page, ptr) < flen) {

				return(0);
			}

			ut_memcpy(ptr, field, flen);
			ptr += flen;
		}
	}

	/*----------------------------------------*/
	/* Save to the undo log the old values of the columns to be updated. */

	if (update) {
	    if (trx_undo_left(undo_page, ptr) < 5) {

		return(0);
	    }

	    len = mach_write_compressed(ptr, upd_get_n_fields(update));
	    ptr += len;

	    for (i = 0; i < upd_get_n_fields(update); i++) {

		upd_field = upd_get_nth_field(update, i);
		pos = upd_field->field_no;

		/* Write field number to undo log */
		if (trx_undo_left(undo_page, ptr) < 5) {

			return(0);
		}

		len = mach_write_compressed(ptr, pos);
		ptr += len;

		/* Save the old value of field */
		field = rec_get_nth_field(rec, pos, &flen);

		if (trx_undo_left(undo_page, ptr) < 5) {

			return(0);
		}

		len = mach_write_compressed(ptr, flen);
		ptr += len;

		if (flen != UNIV_SQL_NULL) {
			if (trx_undo_left(undo_page, ptr) < flen) {

				return(0);
			}

			ut_memcpy(ptr, field, flen);
			ptr += flen;
		}
	    }
	}		

	/*----------------------------------------*/
	/* In the case of a delete marking, and also in the case of an update
	where any ordering field of any index changes, store the values of all
	columns which occur as ordering fields in any index. This info is used
	in the purge of old versions where we use it to build and search the
	delete marked index records, to look if we can remove them from the
	index tree. */

	if (!update || !(cmpl_info & UPD_NODE_NO_ORD_CHANGE)) {	    

	    (trx->update_undo)->del_marks = TRUE;

	    if (trx_undo_left(undo_page, ptr) < 5) {

		return(0);
	    }
	    
	    old_ptr = ptr;

	    /* Reserve 2 bytes to write the number of bytes the stored fields
	    take in this undo record */

	    ptr += 2;

	    for (col_no = 0; col_no < dict_table_get_n_cols(table); col_no++) {

	    	col = dict_table_get_nth_col(table, col_no);

	    	if (col->ord_part > 0) {
	    		
			pos = dict_index_get_nth_col_pos(index, col_no);

			/* Write field number to undo log */
			if (trx_undo_left(undo_page, ptr) < 5) {
	
				return(0);
			}

			len = mach_write_compressed(ptr, pos);
			ptr += len;
	
			/* Save the old value of field */
			field = rec_get_nth_field(rec, pos, &flen);
	
			if (trx_undo_left(undo_page, ptr) < 5) {
	
				return(0);
			}

			len = mach_write_compressed(ptr, flen);
			ptr += len;

			if (flen != UNIV_SQL_NULL) {
				if (trx_undo_left(undo_page, ptr) < flen) {
	
					return(0);
				}

				ut_memcpy(ptr, field, flen);
				ptr += flen;
			}
		}
	    }

	    mach_write_to_2(old_ptr, ptr - old_ptr);	    
	}		

	/*----------------------------------------*/
	/* Write pointers to the previous and the next undo log records */
	if (trx_undo_left(undo_page, ptr) < 2) {

		return(0);
	}
	
	mach_write_to_2(ptr, first_free);
	ptr += 2;
	mach_write_to_2(undo_page + first_free, ptr - undo_page);

	mach_write_to_2(undo_page + TRX_UNDO_PAGE_HDR + TRX_UNDO_PAGE_FREE,
							ptr - undo_page);

	/* Write to the REDO log about this change in the UNDO log */

	trx_undof_page_add_undo_rec_log(undo_page, first_free,
							ptr - undo_page, mtr);
	return(first_free);	
}

/**************************************************************************
Reads from an undo log update record the system field values of the old
version. */

byte*
trx_undo_update_rec_get_sys_cols(
/*=============================*/
				/* out: remaining part of undo log
				record after reading these values */
	byte*	ptr,		/* in: remaining part of undo log
				record after reading general
				parameters */
	dulint*	trx_id,		/* out: trx id */
	dulint*	roll_ptr,	/* out: roll ptr */
	ulint*	info_bits)	/* out: info bits state */
{
	ulint	len;

	/* Read the state of the info bits */
	*info_bits = mach_read_from_1(ptr);
	ptr += 1;

	/* Read the values of the system columns */

	*trx_id = mach_dulint_read_compressed(ptr); 		
	len = mach_dulint_get_compressed_size(*trx_id);
	ptr += len;

	*roll_ptr = mach_dulint_read_compressed(ptr); 		
	len = mach_dulint_get_compressed_size(*roll_ptr);
	ptr += len;

	return(ptr);
}

/**************************************************************************
Reads from an update undo log record the number of updated fields. */
UNIV_INLINE
byte*
trx_undo_update_rec_get_n_upd_fields(
/*=================================*/
			/* out: remaining part of undo log record after
			reading this value */
	byte*	ptr,	/* in: pointer to remaining part of undo log record */
	ulint*	n)	/* out: number of fields */
{
	*n = mach_read_compressed(ptr); 
	ptr += mach_get_compressed_size(*n);

	return(ptr);
}

/**************************************************************************
Reads from an update undo log record a stored field number. */
UNIV_INLINE
byte*
trx_undo_update_rec_get_field_no(
/*=============================*/
			/* out: remaining part of undo log record after
			reading this value */
	byte*	ptr,	/* in: pointer to remaining part of undo log record */
	ulint*	field_no)/* out: field number */
{
	*field_no = mach_read_compressed(ptr); 
	ptr += mach_get_compressed_size(*field_no);

	return(ptr);
}

/***********************************************************************
Builds an update vector based on a remaining part of an undo log record. */

byte*
trx_undo_update_rec_get_update(
/*===========================*/
				/* out: remaining part of the record */
	byte*		ptr,	/* in: remaining part in update undo log
				record, after reading the row reference
				NOTE that this copy of the undo log record must
				be preserved as long as the update vector is
				used, as we do NOT copy the data in the
				record! */
	dict_index_t*	index,	/* in: clustered index */
	ulint		type,	/* in: TRX_UNDO_UPD_EXIST_REC,
				TRX_UNDO_UPD_DEL_REC, or
				TRX_UNDO_DEL_MARK_REC; in the last case,
				only trx id and roll ptr fields are added to
				the update vector */
	dulint		trx_id,	/* in: transaction id from this undorecord */
	dulint		roll_ptr,/* in: roll pointer from this undo record */
	ulint		info_bits,/* in: info bits from this undo record */
	mem_heap_t*	heap,	/* in: memory heap from which the memory
				needed is allocated */
	upd_t**		upd)	/* out, own: update vector */
{
	upd_field_t*	upd_field;
	upd_t*		update;
	ulint		n_fields;
	byte*		buf;
	byte*		field;
	ulint		len;
	ulint		field_no;
	ulint		i;
	
	if (type != TRX_UNDO_DEL_MARK_REC) {
		ptr = trx_undo_update_rec_get_n_upd_fields(ptr, &n_fields);
	} else {
		n_fields = 0;
	}

	update = upd_create(n_fields + 2, heap);

	update->info_bits = info_bits;

	/* Store first trx id and roll ptr to update vector */

	upd_field = upd_get_nth_field(update, n_fields);
	buf = mem_heap_alloc(heap, DATA_TRX_ID_LEN);
	trx_write_trx_id(buf, trx_id);

	upd_field_set_field_no(upd_field,
			dict_index_get_sys_col_pos(index, DATA_TRX_ID),
									index);
	dfield_set_data(&(upd_field->new_val), buf, DATA_TRX_ID_LEN);

	upd_field = upd_get_nth_field(update, n_fields + 1);
	buf = mem_heap_alloc(heap, DATA_ROLL_PTR_LEN);
	trx_write_roll_ptr(buf, roll_ptr);

	upd_field_set_field_no(upd_field,
			dict_index_get_sys_col_pos(index, DATA_ROLL_PTR),
									index);
	dfield_set_data(&(upd_field->new_val), buf, DATA_ROLL_PTR_LEN);
	
	/* Store then the updated ordinary columns to update vector */

	for (i = 0; i < n_fields; i++) {

		ptr = trx_undo_update_rec_get_field_no(ptr, &field_no);
		ptr = trx_undo_rec_get_col_val(ptr, &field, &len);

		upd_field = upd_get_nth_field(update, i);

		upd_field_set_field_no(upd_field, field_no, index);

		dfield_set_data(&(upd_field->new_val), field, len);
	}

	*upd = update;

	return(ptr);
}
	
/***********************************************************************
Builds a partial row from an update undo log record. It contains the
columns which occur as ordering in any index of the table. */

byte*
trx_undo_rec_get_partial_row(
/*=========================*/
				/* out: pointer to remaining part of undo
				record */
	byte*		ptr,	/* in: remaining part in update undo log
				record of a suitable type, at the start of
				the stored index columns;
				NOTE that this copy of the undo log record must
				be preserved as long as the partial row is
				used, as we do NOT copy the data in the
				record! */
	dict_index_t*	index,	/* in: clustered index */
	dtuple_t**	row,	/* out, own: partial row */
	mem_heap_t*	heap)	/* in: memory heap from which the memory
				needed is allocated */
{
	dfield_t*	dfield;
	byte*		field;
	ulint		len;
	ulint		field_no;
	ulint		col_no;
	ulint		row_len;
	ulint		total_len;
	byte*		start_ptr;
	ulint		i;
	
	ut_ad(index && ptr && row && heap);
	
	row_len = dict_table_get_n_cols(index->table);

	*row = dtuple_create(heap, row_len);

	dict_table_copy_types(*row, index->table);

	start_ptr = ptr;

	total_len = mach_read_from_2(ptr);
	ptr += 2;
	
	for (i = 0;; i++) {

		if (ptr == start_ptr + total_len) {

			break;
		}
	
		ptr = trx_undo_update_rec_get_field_no(ptr, &field_no);

		col_no = dict_index_get_nth_col_no(index, field_no);
		
		ptr = trx_undo_rec_get_col_val(ptr, &field, &len);

		dfield = dtuple_get_nth_field(*row, col_no);

		dfield_set_data(dfield, field, len);
	}

	return(ptr);
}	

/***************************************************************************
Erases the unused undo log page end. */
static
void
trx_undo_erase_page_end(
/*====================*/
	page_t*	undo_page,	/* in: undo page whose end to erase */
	mtr_t*	mtr)		/* in: mtr */
{
	ulint	first_free;
	ulint	i;
	
	first_free = mach_read_from_2(undo_page + TRX_UNDO_PAGE_HDR
							+ TRX_UNDO_PAGE_FREE);
	for (i = first_free; i < UNIV_PAGE_SIZE - FIL_PAGE_DATA_END; i++) {
		undo_page[i] = 0xFF;
	}

	mlog_write_initial_log_record(undo_page, MLOG_UNDO_ERASE_END, mtr);
}
	
/***************************************************************
Parses a redo log record of erasing of an undo page end. */

byte*
trx_undo_parse_erase_page_end(
/*==========================*/
			/* out: end of log record or NULL */
	byte*	ptr,	/* in: buffer */
	byte*	end_ptr,/* in: buffer end */
	page_t*	page,	/* in: page or NULL */
	mtr_t*	mtr)	/* in: mtr or NULL */
{
	ut_ad(ptr && end_ptr);

	if (page == NULL) {

		return(ptr);
	}

	trx_undo_erase_page_end(page, mtr);

	return(ptr);
}

/***************************************************************************
Writes information to an undo log about an insert, update, or a delete marking
of a clustered index record. This information is used in a rollback of the
transaction and in consistent reads that must look to the history of this
transaction. */

ulint
trx_undo_report_row_operation(
/*==========================*/
					/* out: DB_SUCCESS or error code */
	ulint		flags,		/* in: if BTR_NO_UNDO_LOG_FLAG bit is
					set, does nothing */
	ulint		op_type,	/* in: TRX_UNDO_INSERT_OP or
					TRX_UNDO_MODIFY_OP */
	que_thr_t*	thr,		/* in: query thread */
	dict_index_t*	index,		/* in: clustered index */
	dtuple_t*	clust_entry,	/* in: in the case of an insert,
					index entry to insert into the
					clustered index, otherwise NULL */
	upd_t*		update,		/* in: in the case of an update,
					the update vector, otherwise NULL */
	ulint		cmpl_info,	/* in: compiler info on secondary
					index updates */
	rec_t*		rec,		/* in: case of an update or delete
					marking, the record in the clustered
					index, otherwise NULL */
	dulint*		roll_ptr)	/* out: rollback pointer to the
					inserted undo log record,
					ut_dulint_zero if BTR_NO_UNDO_LOG
					flag was specified */
{
	trx_t*		trx;
	trx_undo_t*	undo;
	page_t*		undo_page;
	ulint		offset;
	mtr_t		mtr;
	ulint		page_no;
	ibool		is_insert;
	trx_rseg_t*	rseg;
	
	if (flags & BTR_NO_UNDO_LOG_FLAG) {

		*roll_ptr = ut_dulint_zero;

		return(DB_SUCCESS);
	}
		
	ut_ad(thr);
	ut_ad(index->type & DICT_CLUSTERED);
	ut_ad((op_type != TRX_UNDO_INSERT_OP)
	      || (clust_entry && !update && !rec));
	
	trx = thr_get_trx(thr);
	rseg = trx->rseg;
	
	mutex_enter(&(trx->undo_mutex));

	/* If the undo log is not assigned yet, assign one */

	if (op_type == TRX_UNDO_INSERT_OP) {

		if (trx->insert_undo == NULL) {

			trx_undo_assign_undo(trx, TRX_UNDO_INSERT);
		}

		undo = trx->insert_undo;
		is_insert = TRUE;
	} else {
		ut_ad(op_type == TRX_UNDO_MODIFY_OP);

		if (trx->update_undo == NULL) {

			trx_undo_assign_undo(trx, TRX_UNDO_UPDATE);

		}

		undo = trx->update_undo;
		is_insert = FALSE;
	}

	if (undo == NULL) {
		/* Did not succeed: out of space */
		mutex_exit(&(trx->undo_mutex));

		return(DB_OUT_OF_FILE_SPACE);
	}

	page_no = undo->last_page_no;
	
	mtr_start(&mtr);

	for (;;) {
		undo_page = buf_page_get_gen(undo->space, page_no,
						RW_X_LATCH, undo->guess_page,
						BUF_GET,
						#ifdef UNIV_SYNC_DEBUG
						__FILE__, __LINE__,
						#endif
						&mtr);

		buf_page_dbg_add_level(undo_page, SYNC_TRX_UNDO_PAGE);

		if (op_type == TRX_UNDO_INSERT_OP) {
			offset = trx_undo_page_report_insert(undo_page, trx,
							index, clust_entry,
							&mtr);
		} else {
			offset = trx_undo_page_report_modify(undo_page, trx,
							index, rec, update,
							cmpl_info, &mtr);
		}

		if (offset == 0) {
			/* The record did not fit on the page. We erase the
			end segment of the undo log page and write a log
			record of it: this is to ensure that in the debug
			version the replicate page constructed using the log
			records stays identical to the original page */

			trx_undo_erase_page_end(undo_page, &mtr);
		}
		
		mtr_commit(&mtr);

		if (offset != 0) {
			/* Success */

			break;
		}

		ut_ad(page_no == undo->last_page_no);
		
		/* We have to extend the undo log by one page */

		mtr_start(&mtr);

		/* When we add a page to an undo log, this is analogous to
		a pessimistic insert in a B-tree, and we must reserve the
		counterpart of the tree latch, which is the rseg mutex. */

		mutex_enter(&(rseg->mutex));
		
		page_no = trx_undo_add_page(trx, undo, &mtr);

		mutex_exit(&(rseg->mutex));
		
		if (page_no == FIL_NULL) {
			/* Did not succeed: out of space */

			mutex_exit(&(trx->undo_mutex));
			mtr_commit(&mtr);

			return(DB_OUT_OF_FILE_SPACE);
		}
	}

	undo->empty = FALSE;
	undo->top_page_no = page_no;
	undo->top_offset  = offset;
	undo->top_undo_no = trx->undo_no;
	undo->guess_page = undo_page;

	UT_DULINT_INC(trx->undo_no);
	
	mutex_exit(&(trx->undo_mutex));

	*roll_ptr = trx_undo_build_roll_ptr(is_insert, rseg->id, page_no,
								offset);
	return(DB_SUCCESS);
}

/*============== BUILDING PREVIOUS VERSION OF A RECORD ===============*/

/**********************************************************************
Copies an undo record to heap. This function can be called if we know that
the undo log record exists. */

trx_undo_rec_t*
trx_undo_get_undo_rec_low(
/*======================*/
					/* out, own: copy of the record */
	dulint		roll_ptr,	/* in: roll pointer to record */
	mem_heap_t*	heap)		/* in: memory heap where copied */
{
	ulint		rseg_id;
	ulint		page_no;
	ulint		offset;
	page_t*		undo_page;
	trx_rseg_t*	rseg;
	ibool		is_insert;
	mtr_t		mtr;
	trx_undo_rec_t*	undo_rec;
	
	trx_undo_decode_roll_ptr(roll_ptr, &is_insert, &rseg_id, &page_no,
								&offset);
	rseg = trx_rseg_get_on_id(rseg_id);

	mtr_start(&mtr);
	
	undo_page = trx_undo_page_get_s_latched(rseg->space, page_no, &mtr);
	
	undo_rec = trx_undo_rec_copy(undo_page + offset, heap);

	mtr_commit(&mtr);

	return(undo_rec);
}

/**********************************************************************
Copies an undo record to heap. */

ulint
trx_undo_get_undo_rec(
/*==================*/
					/* out: DB_SUCCESS, or
					DB_MISSING_HISTORY if the undo log
					has been truncated and we cannot
					fetch the old version; NOTE: the
					caller must have latches on the
					clustered index page and purge_view */
	dulint		roll_ptr,	/* in: roll pointer to record */
	dulint		trx_id,		/* in: id of the trx that generated
					the roll pointer: it points to an
					undo log of this transaction */
	trx_undo_rec_t** undo_rec,	/* out, own: copy of the record */
	mem_heap_t*	heap)		/* in: memory heap where copied */
{
	ut_ad(rw_lock_own(&(purge_sys->latch), RW_LOCK_SHARED));

	if (!trx_purge_update_undo_must_exist(trx_id)) {

	    	/* It may be that the necessary undo log has already been
		deleted */

		return(DB_MISSING_HISTORY);
	}

	*undo_rec = trx_undo_get_undo_rec_low(roll_ptr, heap);
	
	return(DB_SUCCESS);
}

/***********************************************************************
Build a previous version of a clustered index record. This function checks
that the caller has a latch on the index page of the clustered index record
and an s-latch on the purge_view. This guarantees that the stack of versions
is locked. */

ulint
trx_undo_prev_version_build(
/*========================*/
				/* out: DB_SUCCESS, or DB_MISSING_HISTORY if
				the previous version is not >= purge_view,
				which means that it may have been removed */
	rec_t*		index_rec,/* in: clustered index record in the
				index tree */
	mtr_t*		index_mtr,/* in: mtr which contains the latch to
				index_rec page and purge_view */
	rec_t*		rec,	/* in: version of a clustered index record */
	dict_index_t*	index,	/* in: clustered index */
	mem_heap_t*	heap,	/* in: memory heap from which the memory
				needed is allocated */
	rec_t**		old_vers)/* out, own: previous version, or NULL if
				rec is the first inserted version, or if
				history data has been deleted */
{
	trx_undo_rec_t*	undo_rec;
	dtuple_t*	entry;
	dulint		rec_trx_id;
	ulint		type;
	dulint		undo_no;
	dulint		table_id;
	dulint		trx_id;
	dulint		roll_ptr;
	upd_t*		update;
	byte*		ptr;
	ulint		info_bits;
	ulint		cmpl_info;
	byte*		buf;
	ulint		err;

	ut_ad(rw_lock_own(&(purge_sys->latch), RW_LOCK_SHARED));
	ut_ad(mtr_memo_contains(index_mtr, buf_block_align(index_rec), 
						MTR_MEMO_PAGE_S_FIX) ||
	      mtr_memo_contains(index_mtr, buf_block_align(index_rec), 
						MTR_MEMO_PAGE_X_FIX));

	roll_ptr = row_get_rec_roll_ptr(rec, index);

	if (trx_undo_roll_ptr_is_insert(roll_ptr)) {

		/* The record rec is the first inserted version */
		*old_vers = NULL;

		return(DB_SUCCESS);
	}

	rec_trx_id = row_get_rec_trx_id(rec, index);
	
	err = trx_undo_get_undo_rec(roll_ptr, rec_trx_id, &undo_rec, heap);

	if (err != DB_SUCCESS) {

		*old_vers = NULL;

		return(err);
	}

	ptr = trx_undo_rec_get_pars(undo_rec, &type, &cmpl_info, &undo_no,
								&table_id);
	ptr = trx_undo_update_rec_get_sys_cols(ptr, &trx_id, &roll_ptr,
								&info_bits);
	ptr = trx_undo_rec_skip_row_ref(ptr, index);

	trx_undo_update_rec_get_update(ptr, index, type, trx_id, roll_ptr,
						info_bits, heap, &update);

	if (row_upd_changes_field_size(rec, index, update)) {

		entry = row_rec_to_index_entry(ROW_COPY_DATA, index, rec, heap);

		row_upd_clust_index_replace_new_col_vals(entry, update);

		buf = mem_heap_alloc(heap, rec_get_converted_size(entry));

		*old_vers = rec_convert_dtuple_to_rec(buf, entry);
	} else {
		buf = mem_heap_alloc(heap, rec_get_size(rec));

		*old_vers = rec_copy(buf, rec);

		row_upd_rec_in_place(*old_vers, update);
	}

	return(DB_SUCCESS);
}
