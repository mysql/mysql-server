/*****************************************************************************

Copyright (c) 2005, 2017, Oracle and/or its affiliates. All Rights Reserved.

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
@file row/row0merge.cc
New index creation routines using a merge sort

Created 12/4/2005 Jan Lindstrom
Completed by Sunny Bains and Marko Makela
*******************************************************/

#include <math.h>

#include "ha_prototypes.h"

#include "row0merge.h"
#include "row0ext.h"
#include "row0log.h"
#include "row0ins.h"
#include "row0sel.h"
#include "dict0crea.h"
#include "trx0purge.h"
#include "lock0lock.h"
#include "pars0pars.h"
#include "ut0sort.h"
#include "row0ftsort.h"
#include "row0import.h"
#include "handler0alter.h"
#include "btr0bulk.h"
#include "fsp0sysspace.h"
#include "ut0new.h"
#include "ut0stage.h"

/* Ignore posix_fadvise() on those platforms where it does not exist */
#if defined _WIN32
# define posix_fadvise(fd, offset, len, advice) /* nothing */
#endif /* _WIN32 */

/* Whether to disable file system cache */
char	srv_disable_sort_file_cache;

/** Class that caches index row tuples made from a single cluster
index page scan, and then insert into corresponding index tree */
class index_tuple_info_t {
public:
	/** constructor
	@param[in]	heap	memory heap
	@param[in]	index	index to be created */
	index_tuple_info_t(
		mem_heap_t*	heap,
		dict_index_t*	index) UNIV_NOTHROW
	{
		m_heap = heap;
		m_index = index;
		m_dtuple_vec = UT_NEW_NOKEY(idx_tuple_vec());
	}

	/** destructor */
	~index_tuple_info_t()
	{
		UT_DELETE(m_dtuple_vec);
	}

	/** Get the index object
	@return the index object */
	dict_index_t*   get_index() UNIV_NOTHROW
	{
		return(m_index);
	}

	/** Caches an index row into index tuple vector
	@param[in]	row	table row
	@param[in]	ext	externally stored column
	prefixes, or NULL */
	void add(
		const dtuple_t*		row,
		const row_ext_t*	ext) UNIV_NOTHROW
	{
		dtuple_t*	dtuple;

		dtuple = row_build_index_entry(row, ext, m_index, m_heap);

		ut_ad(dtuple);

		m_dtuple_vec->push_back(dtuple);
	}

	/** Insert spatial index rows cached in vector into spatial index
	@param[in]	trx_id		transaction id
	@param[in,out]	row_heap	memory heap
	@param[in]	pcur		cluster index scanning cursor
	@param[in,out]	scan_mtr	mini-transaction for pcur
	@param[out]	mtr_committed	whether scan_mtr got committed
	@return DB_SUCCESS if successful, else error number */
	dberr_t insert(
		trx_id_t		trx_id,
		mem_heap_t*		row_heap,
		btr_pcur_t*		pcur,
		mtr_t*			scan_mtr,
		bool*			mtr_committed)
	{
		big_rec_t*      big_rec;
		rec_t*          rec;
		btr_cur_t       ins_cur;
		mtr_t           mtr;
		rtr_info_t      rtr_info;
		ulint*		ins_offsets = NULL;
		dberr_t		error = DB_SUCCESS;
		dtuple_t*	dtuple;
		ulint		count = 0;
		const ulint	flag = BTR_NO_UNDO_LOG_FLAG
				       | BTR_NO_LOCKING_FLAG
				       | BTR_KEEP_SYS_FLAG | BTR_CREATE_FLAG;

		ut_ad(dict_index_is_spatial(m_index));

		DBUG_EXECUTE_IF("row_merge_instrument_log_check_flush",
			log_sys->check_flush_or_checkpoint = true;
		);

		for (idx_tuple_vec::iterator it = m_dtuple_vec->begin();
		     it != m_dtuple_vec->end();
		     ++it) {
			dtuple = *it;
			ut_ad(dtuple);

			if (log_sys->check_flush_or_checkpoint) {
				if (!(*mtr_committed)) {
					btr_pcur_move_to_prev_on_page(pcur);
					btr_pcur_store_position(pcur, scan_mtr);
					mtr_commit(scan_mtr);
					*mtr_committed = true;
				}

				log_free_check();
			}

			mtr.start();
			mtr.set_named_space(m_index->space);

			ins_cur.index = m_index;
			rtr_init_rtr_info(&rtr_info, false, &ins_cur, m_index,
					  false);
			rtr_info_update_btr(&ins_cur, &rtr_info);

			btr_cur_search_to_nth_level(m_index, 0, dtuple,
						    PAGE_CUR_RTREE_INSERT,
						    BTR_MODIFY_LEAF, &ins_cur,
						    0, __FILE__, __LINE__,
						    &mtr);

			/* It need to update MBR in parent entry,
			so change search mode to BTR_MODIFY_TREE */
			if (rtr_info.mbr_adj) {
				mtr_commit(&mtr);
				rtr_clean_rtr_info(&rtr_info, true);
				rtr_init_rtr_info(&rtr_info, false, &ins_cur,
						  m_index, false);
				rtr_info_update_btr(&ins_cur, &rtr_info);
				mtr_start(&mtr);
				mtr.set_named_space(m_index->space);
				btr_cur_search_to_nth_level(
					m_index, 0, dtuple,
					PAGE_CUR_RTREE_INSERT,
					BTR_MODIFY_TREE, &ins_cur, 0,
					__FILE__, __LINE__, &mtr);
			}

			error = btr_cur_optimistic_insert(
				flag, &ins_cur, &ins_offsets, &row_heap,
				dtuple, &rec, &big_rec, 0, NULL, &mtr);

			if (error == DB_FAIL) {
				ut_ad(!big_rec);
				mtr.commit();
				mtr.start();
				mtr.set_named_space(m_index->space);

				rtr_clean_rtr_info(&rtr_info, true);
				rtr_init_rtr_info(&rtr_info, false,
						  &ins_cur, m_index, false);

				rtr_info_update_btr(&ins_cur, &rtr_info);
				btr_cur_search_to_nth_level(
					m_index, 0, dtuple,
					PAGE_CUR_RTREE_INSERT,
					BTR_MODIFY_TREE,
					&ins_cur, 0,
					__FILE__, __LINE__, &mtr);


				error = btr_cur_pessimistic_insert(
						flag, &ins_cur, &ins_offsets,
						&row_heap, dtuple, &rec,
						&big_rec, 0, NULL, &mtr);
			}

			DBUG_EXECUTE_IF(
				"row_merge_ins_spatial_fail",
				error = DB_FAIL;
			);

			if (error == DB_SUCCESS) {
				if (rtr_info.mbr_adj) {
					error = rtr_ins_enlarge_mbr(
							&ins_cur, NULL, &mtr);
				}

				if (error == DB_SUCCESS) {
					page_update_max_trx_id(
						btr_cur_get_block(&ins_cur),
						btr_cur_get_page_zip(&ins_cur),
						trx_id, &mtr);
				}
			}

			mtr_commit(&mtr);

			rtr_clean_rtr_info(&rtr_info, true);
			count++;
		}

		m_dtuple_vec->clear();

		return(error);
	}

private:
	/** Cache index rows made from a cluster index scan. Usually
	for rows on single cluster index page */
	typedef std::vector<dtuple_t*, ut_allocator<dtuple_t*> >
		idx_tuple_vec;

	/** vector used to cache index rows made from cluster index scan */
	idx_tuple_vec*		m_dtuple_vec;

	/** the index being built */
	dict_index_t*		m_index;

	/** memory heap for creating index tuples */
	mem_heap_t*		m_heap;
};

/* Maximum pending doc memory limit in bytes for a fts tokenization thread */
#define FTS_PENDING_DOC_MEMORY_LIMIT	1000000

/** Insert sorted data tuples to the index.
@param[in]	trx_id		transaction identifier
@param[in]	index		index to be inserted
@param[in]	old_table	old table
@param[in]	fd		file descriptor
@param[in,out]	block		file buffer
@param[in]	row_buf		row_buf the sorted data tuples,
or NULL if fd, block will be used instead
@param[in,out]	btr_bulk	btr bulk instance
@param[in,out]	stage		performance schema accounting object, used by
ALTER TABLE. If not NULL stage->begin_phase_insert() will be called initially
and then stage->inc() will be called for each record that is processed.
@return DB_SUCCESS or error number */
static	MY_ATTRIBUTE((warn_unused_result))
dberr_t
row_merge_insert_index_tuples(
	trx_id_t		trx_id,
	dict_index_t*		index,
	const dict_table_t*	old_table,
	int			fd,
	row_merge_block_t*	block,
	const row_merge_buf_t*	row_buf,
	BtrBulk*		btr_bulk,
	ut_stage_alter_t*	stage = NULL);

/******************************************************//**
Encode an index record. */
static
void
row_merge_buf_encode(
/*=================*/
	byte**			b,		/*!< in/out: pointer to
						current end of output buffer */
	const dict_index_t*	index,		/*!< in: index */
	const mtuple_t*		entry,		/*!< in: index fields
						of the record to encode */
	ulint			n_fields)	/*!< in: number of fields
						in the entry */
{
	ulint	size;
	ulint	extra_size;

	size = rec_get_converted_size_temp(
		index, entry->fields, n_fields, NULL, &extra_size);
	ut_ad(size >= extra_size);

	/* Encode extra_size + 1 */
	if (extra_size + 1 < 0x80) {
		*(*b)++ = (byte) (extra_size + 1);
	} else {
		ut_ad((extra_size + 1) < 0x8000);
		*(*b)++ = (byte) (0x80 | ((extra_size + 1) >> 8));
		*(*b)++ = (byte) (extra_size + 1);
	}

	rec_convert_dtuple_to_temp(*b + extra_size, index,
				   entry->fields, n_fields, NULL);

	*b += size;
}

/******************************************************//**
Allocate a sort buffer.
@return own: sort buffer */
static MY_ATTRIBUTE((malloc))
row_merge_buf_t*
row_merge_buf_create_low(
/*=====================*/
	mem_heap_t*	heap,		/*!< in: heap where allocated */
	dict_index_t*	index,		/*!< in: secondary index */
	ulint		max_tuples,	/*!< in: maximum number of
					data tuples */
	ulint		buf_size)	/*!< in: size of the buffer,
					in bytes */
{
	row_merge_buf_t*	buf;

	ut_ad(max_tuples > 0);

	ut_ad(max_tuples <= srv_sort_buf_size);

	buf = static_cast<row_merge_buf_t*>(mem_heap_zalloc(heap, buf_size));
	buf->heap = heap;
	buf->index = index;
	buf->max_tuples = max_tuples;
	buf->tuples = static_cast<mtuple_t*>(
		ut_malloc_nokey(2 * max_tuples * sizeof *buf->tuples));
	buf->tmp_tuples = buf->tuples + max_tuples;

	return(buf);
}

/******************************************************//**
Allocate a sort buffer.
@return own: sort buffer */
row_merge_buf_t*
row_merge_buf_create(
/*=================*/
	dict_index_t*	index)	/*!< in: secondary index */
{
	row_merge_buf_t*	buf;
	ulint			max_tuples;
	ulint			buf_size;
	mem_heap_t*		heap;

	max_tuples = static_cast<ulint>(srv_sort_buf_size)
		/ ut_max(static_cast<ulint>(1),
			 dict_index_get_min_size(index));

	buf_size = (sizeof *buf);

	heap = mem_heap_create(buf_size);

	buf = row_merge_buf_create_low(heap, index, max_tuples, buf_size);

	return(buf);
}

/******************************************************//**
Empty a sort buffer.
@return sort buffer */
row_merge_buf_t*
row_merge_buf_empty(
/*================*/
	row_merge_buf_t*	buf)	/*!< in,own: sort buffer */
{
	ulint		buf_size	= sizeof *buf;
	ulint		max_tuples	= buf->max_tuples;
	mem_heap_t*	heap		= buf->heap;
	dict_index_t*	index		= buf->index;
	mtuple_t*	tuples		= buf->tuples;

	mem_heap_empty(heap);

	buf = static_cast<row_merge_buf_t*>(mem_heap_zalloc(heap, buf_size));
	buf->heap = heap;
	buf->index = index;
	buf->max_tuples = max_tuples;
	buf->tuples = tuples;
	buf->tmp_tuples = buf->tuples + max_tuples;

	return(buf);
}

/******************************************************//**
Deallocate a sort buffer. */
void
row_merge_buf_free(
/*===============*/
	row_merge_buf_t*	buf)	/*!< in,own: sort buffer to be freed */
{
	ut_free(buf->tuples);
	mem_heap_free(buf->heap);
}

/** Convert the field data from compact to redundant format.
@param[in]	row_field	field to copy from
@param[out]	field		field to copy to
@param[in]	len		length of the field data
@param[in]	zip_size	compressed BLOB page size,
				zero for uncompressed BLOBs
@param[in,out]	heap		memory heap where to allocate data when
				converting to ROW_FORMAT=REDUNDANT, or NULL
				when not to invoke
				row_merge_buf_redundant_convert(). */
static
void
row_merge_buf_redundant_convert(
	const dfield_t*		row_field,
	dfield_t*		field,
	ulint			len,
	const page_size_t&	page_size,
	mem_heap_t*		heap)
{
	ut_ad(DATA_MBMINLEN(field->type.mbminmaxlen) == 1);
	ut_ad(DATA_MBMAXLEN(field->type.mbminmaxlen) > 1);

	byte*		buf = (byte*) mem_heap_alloc(heap, len);
	ulint		field_len = row_field->len;
	ut_ad(field_len <= len);

	if (row_field->ext) {
		const byte*	field_data = static_cast<byte*>(
			dfield_get_data(row_field));
		ulint		ext_len;

		ut_a(field_len >= BTR_EXTERN_FIELD_REF_SIZE);
		ut_a(memcmp(field_data + field_len - BTR_EXTERN_FIELD_REF_SIZE,
			    field_ref_zero, BTR_EXTERN_FIELD_REF_SIZE));

		byte*	data = btr_copy_externally_stored_field(
			&ext_len, field_data, page_size, field_len, heap);

		ut_ad(ext_len < len);

		memcpy(buf, data, ext_len);
		field_len = ext_len;
	} else {
		memcpy(buf, row_field->data, field_len);
	}

	memset(buf + field_len, 0x20, len - field_len);

	dfield_set_data(field, buf, len);
}

/** Insert a data tuple into a sort buffer.
@param[in,out]	buf		sort buffer
@param[in]	fts_index	fts index to be created
@param[in]	old_table	original table
@param[in]	new_table	new table
@param[in,out]	psort_info	parallel sort info
@param[in]	row		table row
@param[in]	ext		cache of externally stored
				column prefixes, or NULL
@param[in,out]	doc_id		Doc ID if we are creating
				FTS index
@param[in,out]	conv_heap	memory heap where to allocate data when
				converting to ROW_FORMAT=REDUNDANT, or NULL
				when not to invoke
				row_merge_buf_redundant_convert()
@param[in,out]	err		set if error occurs
@param[in,out]	v_heap		heap memory to process data for virtual column
@param[in,out]	my_table	mysql table object
@param[in]	trx		transaction object
@return number of rows added, 0 if out of space */
static
ulint
row_merge_buf_add(
	row_merge_buf_t*	buf,
	dict_index_t*		fts_index,
	const dict_table_t*	old_table,
	const dict_table_t*	new_table,
	fts_psort_t*		psort_info,
	const dtuple_t*		row,
	const row_ext_t*	ext,
	doc_id_t*		doc_id,
	mem_heap_t*		conv_heap,
	dberr_t*		err,
	mem_heap_t**		v_heap,
	TABLE*			my_table,
	trx_t*			trx)
{
	ulint			i;
	const dict_index_t*	index;
	mtuple_t*		entry;
	dfield_t*		field;
	const dict_field_t*	ifield;
	ulint			n_fields;
	ulint			data_size;
	ulint			extra_size;
	ulint			bucket = 0;
	doc_id_t		write_doc_id;
	ulint			n_row_added = 0;
	DBUG_ENTER("row_merge_buf_add");

	if (buf->n_tuples >= buf->max_tuples) {
		DBUG_RETURN(0);
	}

	DBUG_EXECUTE_IF(
		"ib_row_merge_buf_add_two",
		if (buf->n_tuples >= 2) DBUG_RETURN(0););

	UNIV_PREFETCH_R(row->fields);

	/* If we are building FTS index, buf->index points to
	the 'fts_sort_idx', and real FTS index is stored in
	fts_index */
	index = (buf->index->type & DICT_FTS) ? fts_index : buf->index;

	/* create spatial index should not come here */
	ut_ad(!dict_index_is_spatial(index));

	n_fields = dict_index_get_n_fields(index);

	entry = &buf->tuples[buf->n_tuples];
	field = entry->fields = static_cast<dfield_t*>(
		mem_heap_alloc(buf->heap, n_fields * sizeof *entry->fields));

	data_size = 0;
	extra_size = UT_BITS_IN_BYTES(index->n_nullable);

	ifield = dict_index_get_nth_field(index, 0);

	for (i = 0; i < n_fields; i++, field++, ifield++) {
		ulint			len;
		const dict_col_t*	col;
		const dict_v_col_t*	v_col = NULL;
		ulint			col_no;
		ulint			fixed_len;
		const dfield_t*		row_field;

		col = ifield->col;
		if (dict_col_is_virtual(col)) {
			v_col = reinterpret_cast<const dict_v_col_t*>(col);
		}

		col_no = dict_col_get_no(col);

		/* Process the Doc ID column */
		if (*doc_id > 0
		    && col_no == index->table->fts->doc_col
		    && !dict_col_is_virtual(col)) {
			fts_write_doc_id((byte*) &write_doc_id, *doc_id);

			/* Note: field->data now points to a value on the
			stack: &write_doc_id after dfield_set_data(). Because
			there is only one doc_id per row, it shouldn't matter.
			We allocate a new buffer before we leave the function
			later below. */

			dfield_set_data(
				field, &write_doc_id, sizeof(write_doc_id));

			field->type.mtype = ifield->col->mtype;
			field->type.prtype = ifield->col->prtype;
			field->type.mbminmaxlen = DATA_MBMINMAXLEN(0, 0);
			field->type.len = ifield->col->len;
		} else {
			/* Use callback to get the virtual column value */
			if (dict_col_is_virtual(col)) {
				dict_index_t*	clust_index
					= dict_table_get_first_index(new_table);

				row_field = innobase_get_computed_value(
					row, v_col, clust_index,
					v_heap, NULL, ifield, trx->mysql_thd,
					my_table, old_table, NULL, NULL);

				if (row_field == NULL) {
					*err = DB_COMPUTE_VALUE_FAILED;
					DBUG_RETURN(0);
				}
				dfield_copy(field, row_field);
			} else {
				row_field = dtuple_get_nth_field(row, col_no);
				dfield_copy(field, row_field);
			}


			/* Tokenize and process data for FTS */
			if (index->type & DICT_FTS) {
				fts_doc_item_t*	doc_item;
				byte*		value;
				void*		ptr;
				const ulint	max_trial_count = 10000;
				ulint		trial_count = 0;

				/* fetch Doc ID if it already exists
				in the row, and not supplied by the
				caller. Even if the value column is
				NULL, we still need to get the Doc
				ID so to maintain the correct max
				Doc ID */
				if (*doc_id == 0) {
					const dfield_t*	doc_field;
					doc_field = dtuple_get_nth_field(
						row,
						index->table->fts->doc_col);
					*doc_id = (doc_id_t) mach_read_from_8(
						static_cast<byte*>(
						dfield_get_data(doc_field)));

					if (*doc_id == 0) {
						ib::warn() << "FTS Doc ID is"
							" zero. Record"
							" skipped";
						DBUG_RETURN(0);
					}
				}

				if (dfield_is_null(field)) {
					n_row_added = 1;
					continue;
				}

				ptr = ut_malloc_nokey(sizeof(*doc_item)
						      + field->len);

				doc_item = static_cast<fts_doc_item_t*>(ptr);
				value = static_cast<byte*>(ptr)
					+ sizeof(*doc_item);
				memcpy(value, field->data, field->len);
				field->data = value;

				doc_item->field = field;
				doc_item->doc_id = *doc_id;

				bucket = *doc_id % fts_sort_pll_degree;

				/* Add doc item to fts_doc_list */
				mutex_enter(&psort_info[bucket].mutex);

				if (psort_info[bucket].error == DB_SUCCESS) {
					UT_LIST_ADD_LAST(
						psort_info[bucket].fts_doc_list,
						doc_item);
					psort_info[bucket].memory_used +=
						sizeof(*doc_item) + field->len;
				} else {
					ut_free(doc_item);
				}

				mutex_exit(&psort_info[bucket].mutex);

				/* Sleep when memory used exceeds limit*/
				while (psort_info[bucket].memory_used
				       > FTS_PENDING_DOC_MEMORY_LIMIT
				       && trial_count++ < max_trial_count) {
					os_thread_sleep(1000);
				}

				n_row_added = 1;
				continue;
			}

			if (field->len != UNIV_SQL_NULL
			    && col->mtype == DATA_MYSQL
			    && col->len != field->len) {
				if (conv_heap != NULL) {
					row_merge_buf_redundant_convert(
						row_field, field, col->len,
						dict_table_page_size(old_table),
						conv_heap);
				} else {
					/* Field length mismatch should not
					happen when rebuilding redundant row
					format table. */
					ut_ad(dict_table_is_comp(index->table));
				}
			}
		}

		len = dfield_get_len(field);

		if (dfield_is_null(field)) {
			ut_ad(!(col->prtype & DATA_NOT_NULL));
			continue;
		} else if (!ext) {
		} else if (dict_index_is_clust(index)) {
			/* Flag externally stored fields. */
			const byte*	buf = row_ext_lookup(ext, col_no,
							     &len);
			if (UNIV_LIKELY_NULL(buf)) {
				ut_a(buf != field_ref_zero);
				if (i < dict_index_get_n_unique(index)) {
					dfield_set_data(field, buf, len);
				} else {
					dfield_set_ext(field);
					len = dfield_get_len(field);
				}
			}
		} else if (!dict_col_is_virtual(col)) {
			/* Only non-virtual column are stored externally */
			const byte*	buf = row_ext_lookup(ext, col_no,
							     &len);
			if (UNIV_LIKELY_NULL(buf)) {
				ut_a(buf != field_ref_zero);
				dfield_set_data(field, buf, len);
			}
		}

		/* If a column prefix index, take only the prefix */

		if (ifield->prefix_len) {
			len = dtype_get_at_most_n_mbchars(
				col->prtype,
				col->mbminmaxlen,
				ifield->prefix_len,
				len,
				static_cast<char*>(dfield_get_data(field)));
			dfield_set_len(field, len);
		}

		ut_ad(len <= col->len
		      || DATA_LARGE_MTYPE(col->mtype)
		      || (col->mtype == DATA_POINT
			  && len == DATA_MBR_LEN));

		fixed_len = ifield->fixed_len;
		if (fixed_len && !dict_table_is_comp(index->table)
		    && DATA_MBMINLEN(col->mbminmaxlen)
		    != DATA_MBMAXLEN(col->mbminmaxlen)) {
			/* CHAR in ROW_FORMAT=REDUNDANT is always
			fixed-length, but in the temporary file it is
			variable-length for variable-length character
			sets. */
			fixed_len = 0;
		}

		if (fixed_len) {
#ifdef UNIV_DEBUG
			ulint	mbminlen = DATA_MBMINLEN(col->mbminmaxlen);
			ulint	mbmaxlen = DATA_MBMAXLEN(col->mbminmaxlen);

			/* len should be between size calcualted base on
			mbmaxlen and mbminlen */
			ut_ad(len <= fixed_len);
			ut_ad(!mbmaxlen || len >= mbminlen
			      * (fixed_len / mbmaxlen));

			ut_ad(!dfield_is_ext(field));
#endif /* UNIV_DEBUG */
		} else if (dfield_is_ext(field)) {
			extra_size += 2;
		} else if (len < 128
			   || (!DATA_BIG_COL(col))) {
			extra_size++;
		} else {
			/* For variable-length columns, we look up the
			maximum length from the column itself.  If this
			is a prefix index column shorter than 256 bytes,
			this will waste one byte. */
			extra_size += 2;
		}
		data_size += len;
	}

	/* If this is FTS index, we already populated the sort buffer, return
	here */
	if (index->type & DICT_FTS) {
		DBUG_RETURN(n_row_added);
	}

#ifdef UNIV_DEBUG
	{
		ulint	size;
		ulint	extra;

		size = rec_get_converted_size_temp(
			index, entry->fields, n_fields, NULL, &extra);

		ut_ad(data_size + extra_size == size);
		ut_ad(extra_size == extra);
	}
#endif /* UNIV_DEBUG */

	/* Add to the total size of the record in row_merge_block_t
	the encoded length of extra_size and the extra bytes (extra_size).
	See row_merge_buf_write() for the variable-length encoding
	of extra_size. */
	data_size += (extra_size + 1) + ((extra_size + 1) >= 0x80);

	/* Record size can exceed page size while converting to
	redundant row format. But there is assert
	ut_ad(size < UNIV_PAGE_SIZE) in rec_offs_data_size().
	It may hit the assert before attempting to insert the row. */
	if (conv_heap != NULL && data_size > UNIV_PAGE_SIZE) {
		*err = DB_TOO_BIG_RECORD;
	}

	ut_ad(data_size < srv_sort_buf_size);

	/* Reserve one byte for the end marker of row_merge_block_t. */
	if (buf->total_size + data_size >= srv_sort_buf_size - 1) {
		DBUG_RETURN(0);
	}

	buf->total_size += data_size;
	buf->n_tuples++;
	n_row_added++;

	field = entry->fields;

	/* Copy the data fields. */

	do {
		dfield_dup(field++, buf->heap);
	} while (--n_fields);

	if (conv_heap != NULL) {
		mem_heap_empty(conv_heap);
	}

	DBUG_RETURN(n_row_added);
}

/*************************************************************//**
Report a duplicate key. */
void
row_merge_dup_report(
/*=================*/
	row_merge_dup_t*	dup,	/*!< in/out: for reporting duplicates */
	const dfield_t*		entry)	/*!< in: duplicate index entry */
{
	if (!dup->n_dup++) {
		/* Only report the first duplicate record,
		but count all duplicate records. */
		innobase_fields_to_mysql(dup->table, dup->index, entry);
	}
}

/*************************************************************//**
Compare two tuples.
@return positive, 0, negative if a is greater, equal, less, than b,
respectively */
static MY_ATTRIBUTE((warn_unused_result))
int
row_merge_tuple_cmp(
/*================*/
	ulint			n_uniq,	/*!< in: number of unique fields */
	ulint			n_field,/*!< in: number of fields */
	const mtuple_t&		a,	/*!< in: first tuple to be compared */
	const mtuple_t&		b,	/*!< in: second tuple to be compared */
	row_merge_dup_t*	dup)	/*!< in/out: for reporting duplicates,
					NULL if non-unique index */
{
	int		cmp;
	const dfield_t*	af	= a.fields;
	const dfield_t*	bf	= b.fields;
	ulint		n	= n_uniq;

	ut_ad(n_uniq > 0);
	ut_ad(n_uniq <= n_field);

	/* Compare the fields of the tuples until a difference is
	found or we run out of fields to compare.  If !cmp at the
	end, the tuples are equal. */
	do {
		cmp = cmp_dfield_dfield(af++, bf++);
	} while (!cmp && --n);

	if (cmp) {
		return(cmp);
	}

	if (dup) {
		/* Report a duplicate value error if the tuples are
		logically equal.  NULL columns are logically inequal,
		although they are equal in the sorting order.  Find
		out if any of the fields are NULL. */
		for (const dfield_t* df = a.fields; df != af; df++) {
			if (dfield_is_null(df)) {
				goto no_report;
			}
		}

		row_merge_dup_report(dup, a.fields);
	}

no_report:
	/* The n_uniq fields were equal, but we compare all fields so
	that we will get the same (internal) order as in the B-tree. */
	for (n = n_field - n_uniq + 1; --n; ) {
		cmp = cmp_dfield_dfield(af++, bf++);
		if (cmp) {
			return(cmp);
		}
	}

	/* This should never be reached, except in a secondary index
	when creating a secondary index and a PRIMARY KEY, and there
	is a duplicate in the PRIMARY KEY that has not been detected
	yet. Internally, an index must never contain duplicates. */
	return(cmp);
}

/** Wrapper for row_merge_tuple_sort() to inject some more context to
UT_SORT_FUNCTION_BODY().
@param tuples array of tuples that being sorted
@param aux work area, same size as tuples[]
@param low lower bound of the sorting area, inclusive
@param high upper bound of the sorting area, inclusive */
#define row_merge_tuple_sort_ctx(tuples, aux, low, high)		\
	row_merge_tuple_sort(n_uniq, n_field, dup, tuples, aux, low, high)
/** Wrapper for row_merge_tuple_cmp() to inject some more context to
UT_SORT_FUNCTION_BODY().
@param a first tuple to be compared
@param b second tuple to be compared
@return positive, 0, negative, if a is greater, equal, less, than b,
respectively */
#define row_merge_tuple_cmp_ctx(a,b)			\
	row_merge_tuple_cmp(n_uniq, n_field, a, b, dup)

/**********************************************************************//**
Merge sort the tuple buffer in main memory. */
static
void
row_merge_tuple_sort(
/*=================*/
	ulint			n_uniq,	/*!< in: number of unique fields */
	ulint			n_field,/*!< in: number of fields */
	row_merge_dup_t*	dup,	/*!< in/out: reporter of duplicates
					(NULL if non-unique index) */
	mtuple_t*		tuples,	/*!< in/out: tuples */
	mtuple_t*		aux,	/*!< in/out: work area */
	ulint			low,	/*!< in: lower bound of the
					sorting area, inclusive */
	ulint			high)	/*!< in: upper bound of the
					sorting area, exclusive */
{
	ut_ad(n_field > 0);
	ut_ad(n_uniq <= n_field);

	UT_SORT_FUNCTION_BODY(row_merge_tuple_sort_ctx,
			      tuples, aux, low, high, row_merge_tuple_cmp_ctx);
}

/******************************************************//**
Sort a buffer. */
void
row_merge_buf_sort(
/*===============*/
	row_merge_buf_t*	buf,	/*!< in/out: sort buffer */
	row_merge_dup_t*	dup)	/*!< in/out: reporter of duplicates
					(NULL if non-unique index) */
{
	ut_ad(!dict_index_is_spatial(buf->index));

	row_merge_tuple_sort(dict_index_get_n_unique(buf->index),
			     dict_index_get_n_fields(buf->index),
			     dup,
			     buf->tuples, buf->tmp_tuples, 0, buf->n_tuples);
}

/******************************************************//**
Write a buffer to a block. */
void
row_merge_buf_write(
/*================*/
	const row_merge_buf_t*	buf,	/*!< in: sorted buffer */
	const merge_file_t*	of UNIV_UNUSED,
					/*!< in: output file */
	row_merge_block_t*	block)	/*!< out: buffer for writing to file */
{
	const dict_index_t*	index	= buf->index;
	ulint			n_fields= dict_index_get_n_fields(index);
	byte*			b	= &block[0];

	DBUG_ENTER("row_merge_buf_write");

	for (ulint i = 0; i < buf->n_tuples; i++) {
		const mtuple_t*	entry	= &buf->tuples[i];

		row_merge_buf_encode(&b, index, entry, n_fields);
		ut_ad(b < &block[srv_sort_buf_size]);

		DBUG_PRINT("ib_merge_sort",
			   ("%p,fd=%d,%lu %lu: %s",
			    reinterpret_cast<const void*>(b), of->fd,
			    ulong(of->offset), ulong(i),
			    rec_printer(entry->fields,
					n_fields).str().c_str()));
	}

	/* Write an "end-of-chunk" marker. */
	ut_a(b < &block[srv_sort_buf_size]);
	ut_a(b == &block[0] + buf->total_size);
	*b++ = 0;
#ifdef UNIV_DEBUG_VALGRIND
	/* The rest of the block is uninitialized.  Initialize it
	to avoid bogus warnings. */
	memset(b, 0xff, &block[srv_sort_buf_size] - b);
#endif /* UNIV_DEBUG_VALGRIND */
	DBUG_PRINT("ib_merge_sort",
		   ("write %p,%d,%lu EOF",
		    reinterpret_cast<const void*>(b), of->fd,
		    ulong(of->offset)));
	DBUG_VOID_RETURN;
}

/******************************************************//**
Create a memory heap and allocate space for row_merge_rec_offsets()
and mrec_buf_t[3].
@return memory heap */
static
mem_heap_t*
row_merge_heap_create(
/*==================*/
	const dict_index_t*	index,		/*!< in: record descriptor */
	mrec_buf_t**		buf,		/*!< out: 3 buffers */
	ulint**			offsets1,	/*!< out: offsets */
	ulint**			offsets2)	/*!< out: offsets */
{
	ulint		i	= 1 + REC_OFFS_HEADER_SIZE
		+ dict_index_get_n_fields(index);
	mem_heap_t*	heap	= mem_heap_create(2 * i * sizeof **offsets1
						  + 3 * sizeof **buf);

	*buf = static_cast<mrec_buf_t*>(
		mem_heap_alloc(heap, 3 * sizeof **buf));
	*offsets1 = static_cast<ulint*>(
		mem_heap_alloc(heap, i * sizeof **offsets1));
	*offsets2 = static_cast<ulint*>(
		mem_heap_alloc(heap, i * sizeof **offsets2));

	(*offsets1)[0] = (*offsets2)[0] = i;
	(*offsets1)[1] = (*offsets2)[1] = dict_index_get_n_fields(index);

	return(heap);
}

/********************************************************************//**
Read a merge block from the file system.
@return TRUE if request was successful, FALSE if fail */
ibool
row_merge_read(
/*===========*/
	int			fd,	/*!< in: file descriptor */
	ulint			offset,	/*!< in: offset where to read
					in number of row_merge_block_t
					elements */
	row_merge_block_t*	buf)	/*!< out: data */
{
	os_offset_t	ofs = ((os_offset_t) offset) * srv_sort_buf_size;
	dberr_t		err;

	DBUG_ENTER("row_merge_read");
	DBUG_PRINT("ib_merge_sort", ("fd=%d ofs=" UINT64PF, fd, ofs));
	DBUG_EXECUTE_IF("row_merge_read_failure", DBUG_RETURN(FALSE););

	IORequest	request;

	/* Merge sort pages are never compressed. */
	request.disable_compression();

	err = os_file_read_no_error_handling_int_fd(
		request,
		fd, buf, ofs, srv_sort_buf_size, NULL);

#ifdef POSIX_FADV_DONTNEED
	/* Each block is read exactly once.  Free up the file cache. */
	posix_fadvise(fd, ofs, srv_sort_buf_size, POSIX_FADV_DONTNEED);
#endif /* POSIX_FADV_DONTNEED */

	if (err != DB_SUCCESS) {
		ib::error() << "Failed to read merge block at " << ofs;
	}

	DBUG_RETURN(err == DB_SUCCESS);
}

/********************************************************************//**
Write a merge block to the file system.
@return TRUE if request was successful, FALSE if fail */
ibool
row_merge_write(
/*============*/
	int		fd,	/*!< in: file descriptor */
	ulint		offset,	/*!< in: offset where to write,
				in number of row_merge_block_t elements */
	const void*	buf)	/*!< in: data */
{
	size_t		buf_len = srv_sort_buf_size;
	os_offset_t	ofs = buf_len * (os_offset_t) offset;
	dberr_t		err;

	DBUG_ENTER("row_merge_write");
	DBUG_PRINT("ib_merge_sort", ("fd=%d ofs=" UINT64PF, fd, ofs));
	DBUG_EXECUTE_IF("row_merge_write_failure", DBUG_RETURN(FALSE););

	IORequest	request(IORequest::WRITE);

	request.disable_compression();

	err = os_file_write_int_fd(
		request,
		"(merge)", fd, buf, ofs, buf_len);

#ifdef POSIX_FADV_DONTNEED
	/* The block will be needed on the next merge pass,
	but it can be evicted from the file cache meanwhile. */
	posix_fadvise(fd, ofs, buf_len, POSIX_FADV_DONTNEED);
#endif /* POSIX_FADV_DONTNEED */

	DBUG_RETURN(err == DB_SUCCESS);
}

/********************************************************************//**
Read a merge record.
@return pointer to next record, or NULL on I/O error or end of list */
const byte*
row_merge_read_rec(
/*===============*/
	row_merge_block_t*	block,	/*!< in/out: file buffer */
	mrec_buf_t*		buf,	/*!< in/out: secondary buffer */
	const byte*		b,	/*!< in: pointer to record */
	const dict_index_t*	index,	/*!< in: index of the record */
	int			fd,	/*!< in: file descriptor */
	ulint*			foffs,	/*!< in/out: file offset */
	const mrec_t**		mrec,	/*!< out: pointer to merge record,
					or NULL on end of list
					(non-NULL on I/O error) */
	ulint*			offsets)/*!< out: offsets of mrec */
{
	ulint	extra_size;
	ulint	data_size;
	ulint	avail_size;

	ut_ad(block);
	ut_ad(buf);
	ut_ad(b >= &block[0]);
	ut_ad(b < &block[srv_sort_buf_size]);
	ut_ad(index);
	ut_ad(foffs);
	ut_ad(mrec);
	ut_ad(offsets);

	ut_ad(*offsets == 1 + REC_OFFS_HEADER_SIZE
	      + dict_index_get_n_fields(index));

	DBUG_ENTER("row_merge_read_rec");

	extra_size = *b++;

	if (UNIV_UNLIKELY(!extra_size)) {
		/* End of list */
		*mrec = NULL;
		DBUG_PRINT("ib_merge_sort",
			   ("read %p,%p,%d,%lu EOF\n",
			    reinterpret_cast<const void*>(b),
			    reinterpret_cast<const void*>(block),
			    fd, ulong(*foffs)));
		DBUG_RETURN(NULL);
	}

	if (extra_size >= 0x80) {
		/* Read another byte of extra_size. */

		if (UNIV_UNLIKELY(b >= &block[srv_sort_buf_size])) {
			if (!row_merge_read(fd, ++(*foffs), block)) {
err_exit:
				/* Signal I/O error. */
				*mrec = b;
				DBUG_RETURN(NULL);
			}

			/* Wrap around to the beginning of the buffer. */
			b = &block[0];
		}

		extra_size = (extra_size & 0x7f) << 8;
		extra_size |= *b++;
	}

	/* Normalize extra_size.  Above, value 0 signals "end of list". */
	extra_size--;

	/* Read the extra bytes. */

	if (UNIV_UNLIKELY(b + extra_size >= &block[srv_sort_buf_size])) {
		/* The record spans two blocks.  Copy the entire record
		to the auxiliary buffer and handle this as a special
		case. */

		avail_size = &block[srv_sort_buf_size] - b;
		ut_ad(avail_size < sizeof *buf);
		memcpy(*buf, b, avail_size);

		if (!row_merge_read(fd, ++(*foffs), block)) {

			goto err_exit;
		}

		/* Wrap around to the beginning of the buffer. */
		b = &block[0];

		/* Copy the record. */
		memcpy(*buf + avail_size, b, extra_size - avail_size);
		b += extra_size - avail_size;

		*mrec = *buf + extra_size;

		rec_init_offsets_temp(*mrec, index, offsets);

		data_size = rec_offs_data_size(offsets);

		/* These overflows should be impossible given that
		records are much smaller than either buffer, and
		the record starts near the beginning of each buffer. */
		ut_a(extra_size + data_size < sizeof *buf);
		ut_a(b + data_size < &block[srv_sort_buf_size]);

		/* Copy the data bytes. */
		memcpy(*buf + extra_size, b, data_size);
		b += data_size;

		goto func_exit;
	}

	*mrec = b + extra_size;

	rec_init_offsets_temp(*mrec, index, offsets);

	data_size = rec_offs_data_size(offsets);
	ut_ad(extra_size + data_size < sizeof *buf);

	b += extra_size + data_size;

	if (UNIV_LIKELY(b < &block[srv_sort_buf_size])) {
		/* The record fits entirely in the block.
		This is the normal case. */
		goto func_exit;
	}

	/* The record spans two blocks.  Copy it to buf. */

	b -= extra_size + data_size;
	avail_size = &block[srv_sort_buf_size] - b;
	memcpy(*buf, b, avail_size);
	*mrec = *buf + extra_size;

	/* We cannot invoke rec_offs_make_valid() here, because there
	are no REC_N_NEW_EXTRA_BYTES between extra_size and data_size.
	Similarly, rec_offs_validate() would fail, because it invokes
	rec_get_status(). */
	ut_d(offsets[2] = (ulint) *mrec);
	ut_d(offsets[3] = (ulint) index);

	if (!row_merge_read(fd, ++(*foffs), block)) {

		goto err_exit;
	}

	/* Wrap around to the beginning of the buffer. */
	b = &block[0];

	/* Copy the rest of the record. */
	memcpy(*buf + avail_size, b, extra_size + data_size - avail_size);
	b += extra_size + data_size - avail_size;

func_exit:
	DBUG_PRINT("ib_merge_sort",
		   ("%p,%p,fd=%d,%lu: %s",
		    reinterpret_cast<const void*>(b),
		    reinterpret_cast<const void*>(block),
		    fd, ulong(*foffs),
		    rec_printer(*mrec, 0, offsets).str().c_str()));
	DBUG_RETURN(b);
}

/********************************************************************//**
Write a merge record. */
static
void
row_merge_write_rec_low(
/*====================*/
	byte*		b,	/*!< out: buffer */
	ulint		e,	/*!< in: encoded extra_size */
#ifndef DBUG_OFF
	ulint		size,	/*!< in: total size to write */
	int		fd,	/*!< in: file descriptor */
	ulint		foffs,	/*!< in: file offset */
#endif /* !DBUG_OFF */
	const mrec_t*	mrec,	/*!< in: record to write */
	const ulint*	offsets)/*!< in: offsets of mrec */
#ifdef DBUG_OFF
# define row_merge_write_rec_low(b, e, size, fd, foffs, mrec, offsets)	\
	row_merge_write_rec_low(b, e, mrec, offsets)
#endif /* DBUG_OFF */
{
	DBUG_ENTER("row_merge_write_rec_low");

#ifndef DBUG_OFF
	const byte* const end = b + size;
#endif /* DBUG_OFF */
	DBUG_ASSERT(e == rec_offs_extra_size(offsets) + 1);
	DBUG_PRINT("ib_merge_sort",
		   ("%p,fd=%d,%lu: %s",
		    reinterpret_cast<const void*>(b), fd, ulong(foffs),
		    rec_printer(mrec, 0, offsets).str().c_str()));

	if (e < 0x80) {
		*b++ = (byte) e;
	} else {
		*b++ = (byte) (0x80 | (e >> 8));
		*b++ = (byte) e;
	}

	memcpy(b, mrec - rec_offs_extra_size(offsets), rec_offs_size(offsets));
	DBUG_ASSERT(b + rec_offs_size(offsets) == end);
	DBUG_VOID_RETURN;
}

/********************************************************************//**
Write a merge record.
@return pointer to end of block, or NULL on error */
static
byte*
row_merge_write_rec(
/*================*/
	row_merge_block_t*	block,	/*!< in/out: file buffer */
	mrec_buf_t*		buf,	/*!< in/out: secondary buffer */
	byte*			b,	/*!< in: pointer to end of block */
	int			fd,	/*!< in: file descriptor */
	ulint*			foffs,	/*!< in/out: file offset */
	const mrec_t*		mrec,	/*!< in: record to write */
	const ulint*		offsets)/*!< in: offsets of mrec */
{
	ulint	extra_size;
	ulint	size;
	ulint	avail_size;

	ut_ad(block);
	ut_ad(buf);
	ut_ad(b >= &block[0]);
	ut_ad(b < &block[srv_sort_buf_size]);
	ut_ad(mrec);
	ut_ad(foffs);
	ut_ad(mrec < &block[0] || mrec > &block[srv_sort_buf_size]);
	ut_ad(mrec < buf[0] || mrec > buf[1]);

	/* Normalize extra_size.  Value 0 signals "end of list". */
	extra_size = rec_offs_extra_size(offsets) + 1;

	size = extra_size + (extra_size >= 0x80)
		+ rec_offs_data_size(offsets);

	if (UNIV_UNLIKELY(b + size >= &block[srv_sort_buf_size])) {
		/* The record spans two blocks.
		Copy it to the temporary buffer first. */
		avail_size = &block[srv_sort_buf_size] - b;

		row_merge_write_rec_low(buf[0],
					extra_size, size, fd, *foffs,
					mrec, offsets);

		/* Copy the head of the temporary buffer, write
		the completed block, and copy the tail of the
		record to the head of the new block. */
		memcpy(b, buf[0], avail_size);

		if (!row_merge_write(fd, (*foffs)++, block)) {
			return(NULL);
		}

		UNIV_MEM_INVALID(&block[0], srv_sort_buf_size);

		/* Copy the rest. */
		b = &block[0];
		memcpy(b, buf[0] + avail_size, size - avail_size);
		b += size - avail_size;
	} else {
		row_merge_write_rec_low(b, extra_size, size, fd, *foffs,
					mrec, offsets);
		b += size;
	}

	return(b);
}

/********************************************************************//**
Write an end-of-list marker.
@return pointer to end of block, or NULL on error */
static
byte*
row_merge_write_eof(
/*================*/
	row_merge_block_t*	block,	/*!< in/out: file buffer */
	byte*			b,	/*!< in: pointer to end of block */
	int			fd,	/*!< in: file descriptor */
	ulint*			foffs)	/*!< in/out: file offset */
{
	ut_ad(block);
	ut_ad(b >= &block[0]);
	ut_ad(b < &block[srv_sort_buf_size]);
	ut_ad(foffs);

	DBUG_ENTER("row_merge_write_eof");
	DBUG_PRINT("ib_merge_sort",
		   ("%p,%p,fd=%d,%lu",
		    reinterpret_cast<const void*>(b),
		    reinterpret_cast<const void*>(block),
		    fd, ulong(*foffs)));

	*b++ = 0;
	UNIV_MEM_ASSERT_RW(&block[0], b - &block[0]);
	UNIV_MEM_ASSERT_W(&block[0], srv_sort_buf_size);
#ifdef UNIV_DEBUG_VALGRIND
	/* The rest of the block is uninitialized.  Initialize it
	to avoid bogus warnings. */
	memset(b, 0xff, &block[srv_sort_buf_size] - b);
#endif /* UNIV_DEBUG_VALGRIND */

	if (!row_merge_write(fd, (*foffs)++, block)) {
		DBUG_RETURN(NULL);
	}

	UNIV_MEM_INVALID(&block[0], srv_sort_buf_size);
	DBUG_RETURN(&block[0]);
}

/** Create a temporary file if it has not been created already.
@param[in,out]	tmpfd	temporary file handle
@param[in]	path	location for creating temporary file
@return file descriptor, or -1 on failure */
static MY_ATTRIBUTE((warn_unused_result))
int
row_merge_tmpfile_if_needed(
	int*		tmpfd,
	const char*	path)
{
	if (*tmpfd < 0) {
		*tmpfd = row_merge_file_create_low(path);
		if (*tmpfd >= 0) {
			MONITOR_ATOMIC_INC(MONITOR_ALTER_TABLE_SORT_FILES);
		}
	}

	return(*tmpfd);
}

/** Create a temporary file for merge sort if it was not created already.
@param[in,out]	file	merge file structure
@param[in]	nrec	number of records in the file
@param[in]	path	location for creating temporary file
@return file descriptor, or -1 on failure */
static MY_ATTRIBUTE((warn_unused_result))
int
row_merge_file_create_if_needed(
	merge_file_t*	file,
	int*		tmpfd,
	ulint		nrec,
	const char*	path)
{
	ut_ad(file->fd < 0 || *tmpfd >=0);
	if (file->fd < 0 && row_merge_file_create(file, path) >= 0) {
		MONITOR_ATOMIC_INC(MONITOR_ALTER_TABLE_SORT_FILES);
		if (row_merge_tmpfile_if_needed(tmpfd, path) < 0) {
			return(-1);
		}

		file->n_rec = nrec;
	}

	ut_ad(file->fd < 0 || *tmpfd >=0);
	return(file->fd);
}

/** Copy the merge data tuple from another merge data tuple.
@param[in]	mtuple		source merge data tuple
@param[in,out]	prev_mtuple	destination merge data tuple
@param[in]	n_unique	number of unique fields exist in the mtuple
@param[in,out]	heap		memory heap where last_mtuple allocated */
static
void
row_mtuple_create(
	const mtuple_t*	mtuple,
	mtuple_t*	prev_mtuple,
	ulint		n_unique,
	mem_heap_t*	heap)
{
	memcpy(prev_mtuple->fields, mtuple->fields,
	       n_unique * sizeof *mtuple->fields);

	dfield_t*	field = prev_mtuple->fields;

	for (ulint i = 0; i < n_unique; i++) {
		dfield_dup(field++, heap);
	}
}

/** Compare two merge data tuples.
@param[in]	prev_mtuple	merge data tuple
@param[in]	current_mtuple	merge data tuple
@param[in,out]	dup		reporter of duplicates
@retval positive, 0, negative if current_mtuple is greater, equal, less, than
last_mtuple. */
static
int
row_mtuple_cmp(
	const mtuple_t*		prev_mtuple,
	const mtuple_t*		current_mtuple,
	row_merge_dup_t*	dup)
{
	ut_ad(dict_index_is_clust(dup->index));
	const ulint	n_unique = dict_index_get_n_unique(dup->index);

	return(row_merge_tuple_cmp(
		       n_unique, n_unique, *current_mtuple, *prev_mtuple, dup));
}

/** Insert cached spatial index rows.
@param[in]	trx_id		transaction id
@param[in]	sp_tuples	cached spatial rows
@param[in]	num_spatial	number of spatial indexes
@param[in,out]	row_heap	heap for insert
@param[in,out]	sp_heap		heap for tuples
@param[in,out]	pcur		cluster index cursor
@param[in,out]	mtr		mini transaction
@param[in,out]	mtr_committed	whether scan_mtr got committed
@return DB_SUCCESS or error number */
static
dberr_t
row_merge_spatial_rows(
	trx_id_t		trx_id,
	index_tuple_info_t**	sp_tuples,
	ulint			num_spatial,
	mem_heap_t*		row_heap,
	mem_heap_t*		sp_heap,
	btr_pcur_t*		pcur,
	mtr_t*			mtr,
	bool*			mtr_committed)
{
	dberr_t			err = DB_SUCCESS;

	if (sp_tuples == NULL) {
		return(DB_SUCCESS);
	}

	ut_ad(sp_heap != NULL);

	for (ulint j = 0; j < num_spatial; j++) {
		err = sp_tuples[j]->insert(
			trx_id, row_heap,
			pcur, mtr, mtr_committed);

		if (err != DB_SUCCESS) {
			return(err);
		}
	}

	mem_heap_empty(sp_heap);

	return(err);
}

/** Check if the geometry field is valid.
@param[in]	row		the row
@param[in]	index		spatial index
@return true if it's valid, false if it's invalid. */
static
bool
row_geo_field_is_valid(
	const dtuple_t*		row,
	dict_index_t*		index)
{
	const dict_field_t*	ind_field
		= dict_index_get_nth_field(index, 0);
	const dict_col_t*	col
		= ind_field->col;
	ulint			col_no
		= dict_col_get_no(col);
	const dfield_t*		dfield
		= dtuple_get_nth_field(row, col_no);

	if (dfield_is_null(dfield)
	    || dfield_get_len(dfield) < GEO_DATA_HEADER_SIZE) {
		return(false);
	}

	return(true);
}

/** Reads clustered index of the table and create temporary files
containing the index entries for the indexes to be built.
@param[in]	trx		transaction
@param[in,out]	table		MySQL table object, for reporting erroneous
records
@param[in]	old_table	table where rows are read from
@param[in]	new_table	table where indexes are created; identical to
old_table unless creating a PRIMARY KEY
@param[in]	online		true if creating indexes online
@param[in]	index		indexes to be created
@param[in]	fts_sort_idx	full-text index to be created, or NULL
@param[in]	psort_info	parallel sort info for fts_sort_idx creation,
or NULL
@param[in]	files		temporary files
@param[in]	key_numbers	MySQL key numbers to create
@param[in]	n_index		number of indexes to create
@param[in]	add_cols	default values of added columns, or NULL
@param[in]	add_v		newly added virtual columns along with indexes
@param[in]	col_map		mapping of old column numbers to new ones, or
NULL if old_table == new_table
@param[in]	add_autoinc	number of added AUTO_INCREMENT columns, or
ULINT_UNDEFINED if none is added
@param[in,out]	sequence	autoinc sequence
@param[in,out]	block		file buffer
@param[in]	skip_pk_sort	whether the new PRIMARY KEY will follow
existing order
@param[in,out]	tmpfd		temporary file handle
@param[in,out]	stage		performance schema accounting object, used by
ALTER TABLE. stage->n_pk_recs_inc() will be called for each record read and
stage->inc() will be called for each page read.
@param[in]	eval_table	mysql table used to evaluate virtual column
				value, see innobase_get_computed_value().
@return DB_SUCCESS or error */
static MY_ATTRIBUTE((warn_unused_result))
dberr_t
row_merge_read_clustered_index(
	trx_t*			trx,
	struct TABLE*		table,
	const dict_table_t*	old_table,
	const dict_table_t*	new_table,
	bool			online,
	dict_index_t**		index,
	dict_index_t*		fts_sort_idx,
	fts_psort_t*		psort_info,
	merge_file_t*		files,
	const ulint*		key_numbers,
	ulint			n_index,
	const dtuple_t*		add_cols,
	const dict_add_v_col_t*	add_v,
	const ulint*		col_map,
	ulint			add_autoinc,
	ib_sequence_t&		sequence,
	row_merge_block_t*	block,
	bool			skip_pk_sort,
	int*			tmpfd,
	ut_stage_alter_t*	stage,
	struct TABLE*		eval_table)
{
	dict_index_t*		clust_index;	/* Clustered index */
	mem_heap_t*		row_heap;	/* Heap memory to create
						clustered index tuples */
	row_merge_buf_t**	merge_buf;	/* Temporary list for records*/
	mem_heap_t*		v_heap = NULL;	/* Heap memory to process large
						data for virtual column */
	btr_pcur_t		pcur;		/* Cursor on the clustered
						index */
	mtr_t			mtr;		/* Mini transaction */
	dberr_t			err = DB_SUCCESS;/* Return code */
	ulint			n_nonnull = 0;	/* number of columns
						changed to NOT NULL */
	ulint*			nonnull = NULL;	/* NOT NULL columns */
	dict_index_t*		fts_index = NULL;/* FTS index */
	doc_id_t		doc_id = 0;
	doc_id_t		max_doc_id = 0;
	ibool			add_doc_id = FALSE;
	os_event_t		fts_parallel_sort_event = NULL;
	ibool			fts_pll_sort = FALSE;
	int64_t			sig_count = 0;
	index_tuple_info_t**	sp_tuples = NULL;
	mem_heap_t*		sp_heap = NULL;
	ulint			num_spatial = 0;
	BtrBulk*		clust_btr_bulk = NULL;
	bool			clust_temp_file = false;
	mem_heap_t*		mtuple_heap = NULL;
	mtuple_t		prev_mtuple;
	mem_heap_t*		conv_heap = NULL;
	FlushObserver*		observer = trx->flush_observer;
	DBUG_ENTER("row_merge_read_clustered_index");

	ut_ad((old_table == new_table) == !col_map);
	ut_ad(!add_cols || col_map);

	trx->op_info = "reading clustered index";

#ifdef FTS_INTERNAL_DIAG_PRINT
	DEBUG_FTS_SORT_PRINT("FTS_SORT: Start Create Index\n");
#endif

	/* Create and initialize memory for record buffers */

	merge_buf = static_cast<row_merge_buf_t**>(
		ut_malloc_nokey(n_index * sizeof *merge_buf));

	row_merge_dup_t	clust_dup = {index[0], table, col_map, 0};
	dfield_t*	prev_fields;
	const ulint	n_uniq = dict_index_get_n_unique(index[0]);

	ut_ad(trx->mysql_thd != NULL);

	const char*	path = thd_innodb_tmpdir(trx->mysql_thd);

	ut_ad(!skip_pk_sort || dict_index_is_clust(index[0]));
	/* There is no previous tuple yet. */
	prev_mtuple.fields = NULL;

	for (ulint i = 0; i < n_index; i++) {
		if (index[i]->type & DICT_FTS) {

			/* We are building a FT index, make sure
			we have the temporary 'fts_sort_idx' */
			ut_a(fts_sort_idx);

			fts_index = index[i];

			merge_buf[i] = row_merge_buf_create(fts_sort_idx);

			add_doc_id = DICT_TF2_FLAG_IS_SET(
				new_table, DICT_TF2_FTS_ADD_DOC_ID);

			/* If Doc ID does not exist in the table itself,
			fetch the first FTS Doc ID */
			if (add_doc_id) {
				fts_get_next_doc_id(
					(dict_table_t*) new_table,
					&doc_id);
				ut_ad(doc_id > 0);
			}

			fts_pll_sort = TRUE;
			row_fts_start_psort(psort_info);
			fts_parallel_sort_event =
				 psort_info[0].psort_common->sort_event;
		} else {
			if (dict_index_is_spatial(index[i])) {
				num_spatial++;
			}

			merge_buf[i] = row_merge_buf_create(index[i]);
		}
	}

	if (num_spatial > 0) {
		ulint	count = 0;

		sp_heap = mem_heap_create(512);

		sp_tuples = static_cast<index_tuple_info_t**>(
			ut_malloc_nokey(num_spatial
					* sizeof(*sp_tuples)));

		for (ulint i = 0; i < n_index; i++) {
			if (dict_index_is_spatial(index[i])) {
				sp_tuples[count]
					= UT_NEW_NOKEY(
						index_tuple_info_t(
							sp_heap,
							index[i]));
				count++;
			}
		}

		ut_ad(count == num_spatial);
	}

	mtr_start(&mtr);

	/* Find the clustered index and create a persistent cursor
	based on that. */

	clust_index = dict_table_get_first_index(old_table);

	btr_pcur_open_at_index_side(
		true, clust_index, BTR_SEARCH_LEAF, &pcur, true, 0, &mtr);

	if (old_table != new_table) {
		/* The table is being rebuilt.  Identify the columns
		that were flagged NOT NULL in the new table, so that
		we can quickly check that the records in the old table
		do not violate the added NOT NULL constraints. */

		nonnull = static_cast<ulint*>(
			ut_malloc_nokey(dict_table_get_n_cols(new_table)
				  * sizeof *nonnull));

		for (ulint i = 0; i < dict_table_get_n_cols(old_table); i++) {
			if (dict_table_get_nth_col(old_table, i)->prtype
			    & DATA_NOT_NULL) {
				continue;
			}

			const ulint j = col_map[i];

			if (j == ULINT_UNDEFINED) {
				/* The column was dropped. */
				continue;
			}

			if (dict_table_get_nth_col(new_table, j)->prtype
			    & DATA_NOT_NULL) {
				nonnull[n_nonnull++] = j;
			}
		}

		if (!n_nonnull) {
			ut_free(nonnull);
			nonnull = NULL;
		}
	}

	row_heap = mem_heap_create(sizeof(mrec_buf_t));

	if (dict_table_is_comp(old_table)
	    && !dict_table_is_comp(new_table)) {
		conv_heap = mem_heap_create(sizeof(mrec_buf_t));
	}

	if (skip_pk_sort) {
		prev_fields = static_cast<dfield_t*>(
			ut_malloc_nokey(n_uniq * sizeof *prev_fields));
		mtuple_heap = mem_heap_create(sizeof(mrec_buf_t));
	} else {
		prev_fields = NULL;
	}

	/* Scan the clustered index. */
	for (;;) {
		const rec_t*	rec;
		ulint*		offsets;
		const dtuple_t*	row;
		row_ext_t*	ext;
		page_cur_t*	cur	= btr_pcur_get_page_cur(&pcur);

		mem_heap_empty(row_heap);

		page_cur_move_to_next(cur);

		stage->n_pk_recs_inc();

		if (page_cur_is_after_last(cur)) {

			stage->inc();

			if (UNIV_UNLIKELY(trx_is_interrupted(trx))) {
				err = DB_INTERRUPTED;
				trx->error_key_num = 0;
				goto func_exit;
			}

			if (online && old_table != new_table) {
				err = row_log_table_get_error(clust_index);
				if (err != DB_SUCCESS) {
					trx->error_key_num = 0;
					goto func_exit;
				}
			}

#ifdef DBUG_OFF
# define dbug_run_purge	false
#else /* DBUG_OFF */
			bool	dbug_run_purge = false;
#endif /* DBUG_OFF */
			DBUG_EXECUTE_IF(
				"ib_purge_on_create_index_page_switch",
				dbug_run_purge = true;);

			/* Insert the cached spatial index rows. */
			bool	mtr_committed = false;

			err = row_merge_spatial_rows(
				trx->id, sp_tuples, num_spatial,
				row_heap, sp_heap, &pcur,
				&mtr, &mtr_committed);

			if (err != DB_SUCCESS) {
				goto func_exit;
			}

			if (mtr_committed) {
				goto scan_next;
			}

			if (dbug_run_purge
			    || rw_lock_get_waiters(
				    dict_index_get_lock(clust_index))) {
				/* There are waiters on the clustered
				index tree lock, likely the purge
				thread. Store and restore the cursor
				position, and yield so that scanning a
				large table will not starve other
				threads. */

				/* Store the cursor position on the last user
				record on the page. */
				btr_pcur_move_to_prev_on_page(&pcur);
				/* Leaf pages must never be empty, unless
				this is the only page in the index tree. */
				ut_ad(btr_pcur_is_on_user_rec(&pcur)
				      || btr_pcur_get_block(
					      &pcur)->page.id.page_no()
				      == clust_index->page);

				btr_pcur_store_position(&pcur, &mtr);
				mtr_commit(&mtr);

				if (dbug_run_purge) {
					/* This is for testing
					purposes only (see
					DBUG_EXECUTE_IF above).  We
					signal the purge thread and
					hope that the purge batch will
					complete before we execute
					btr_pcur_restore_position(). */
					trx_purge_run();
					os_thread_sleep(1000000);
				}

				/* Give the waiters a chance to proceed. */
				os_thread_yield();
scan_next:
				mtr_start(&mtr);
				/* Restore position on the record, or its
				predecessor if the record was purged
				meanwhile. */
				btr_pcur_restore_position(
					BTR_SEARCH_LEAF, &pcur, &mtr);
				/* Move to the successor of the
				original record. */
				if (!btr_pcur_move_to_next_user_rec(
					    &pcur, &mtr)) {
end_of_index:
					row = NULL;
					mtr_commit(&mtr);
					mem_heap_free(row_heap);
					ut_free(nonnull);
					goto write_buffers;
				}
			} else {
				ulint		next_page_no;
				buf_block_t*	block;

				next_page_no = btr_page_get_next(
					page_cur_get_page(cur), &mtr);

				if (next_page_no == FIL_NULL) {
					goto end_of_index;
				}

				block = page_cur_get_block(cur);
				block = btr_block_get(
					page_id_t(block->page.id.space(),
						  next_page_no),
					block->page.size,
					BTR_SEARCH_LEAF,
					clust_index, &mtr);

				btr_leaf_page_release(page_cur_get_block(cur),
						      BTR_SEARCH_LEAF, &mtr);
				page_cur_set_before_first(block, cur);
				page_cur_move_to_next(cur);

				ut_ad(!page_cur_is_after_last(cur));
			}
		}

		rec = page_cur_get_rec(cur);

		offsets = rec_get_offsets(rec, clust_index, NULL,
					  ULINT_UNDEFINED, &row_heap);

		if (online) {
			/* Perform a REPEATABLE READ.

			When rebuilding the table online,
			row_log_table_apply() must not see a newer
			state of the table when applying the log.
			This is mainly to prevent false duplicate key
			errors, because the log will identify records
			by the PRIMARY KEY, and also to prevent unsafe
			BLOB access.

			When creating a secondary index online, this
			table scan must not see records that have only
			been inserted to the clustered index, but have
			not been written to the online_log of
			index[]. If we performed READ UNCOMMITTED, it
			could happen that the ADD INDEX reaches
			ONLINE_INDEX_COMPLETE state between the time
			the DML thread has updated the clustered index
			but has not yet accessed secondary index. */
			ut_ad(MVCC::is_view_active(trx->read_view));

			if (!trx->read_view->changes_visible(
				    row_get_rec_trx_id(
					    rec, clust_index, offsets),
				    old_table->name)) {
				rec_t*	old_vers;

				row_vers_build_for_consistent_read(
					rec, &mtr, clust_index, &offsets,
					trx->read_view, &row_heap,
					row_heap, &old_vers, NULL);

				rec = old_vers;

				if (!rec) {
					continue;
				}
			}

			if (rec_get_deleted_flag(
				    rec,
				    dict_table_is_comp(old_table))) {
				/* This record was deleted in the latest
				committed version, or it was deleted and
				then reinserted-by-update before purge
				kicked in. Skip it. */
				continue;
			}

			ut_ad(!rec_offs_any_null_extern(rec, offsets));
		} else if (rec_get_deleted_flag(
				   rec, dict_table_is_comp(old_table))) {
			/* Skip delete-marked records.

			Skipping delete-marked records will make the
			created indexes unuseable for transactions
			whose read views were created before the index
			creation completed, but preserving the history
			would make it tricky to detect duplicate
			keys. */
			continue;
		}

		/* When !online, we are holding a lock on old_table, preventing
		any inserts that could have written a record 'stub' before
		writing out off-page columns. */
		ut_ad(!rec_offs_any_null_extern(rec, offsets));

		/* Build a row based on the clustered index. */

		row = row_build_w_add_vcol(ROW_COPY_POINTERS, clust_index,
					   rec, offsets, new_table,
					   add_cols, add_v, col_map, &ext,
					   row_heap);
		ut_ad(row);

		for (ulint i = 0; i < n_nonnull; i++) {
			const dfield_t*	field	= &row->fields[nonnull[i]];

			ut_ad(dfield_get_type(field)->prtype & DATA_NOT_NULL);

			if (dfield_is_null(field)) {
				err = DB_INVALID_NULL;
				trx->error_key_num = 0;
				goto func_exit;
			}
		}

		/* Get the next Doc ID */
		if (add_doc_id) {
			doc_id++;
		} else {
			doc_id = 0;
		}

		if (add_autoinc != ULINT_UNDEFINED) {

			ut_ad(add_autoinc
			      < dict_table_get_n_user_cols(new_table));

			const dfield_t*	dfield;

			dfield = dtuple_get_nth_field(row, add_autoinc);
			if (dfield_is_null(dfield)) {
				goto write_buffers;
			}

			const dtype_t*  dtype = dfield_get_type(dfield);
			byte*	b = static_cast<byte*>(dfield_get_data(dfield));

			if (sequence.eof()) {
				err = DB_ERROR;
				trx->error_key_num = 0;

				ib_errf(trx->mysql_thd, IB_LOG_LEVEL_ERROR,
					ER_AUTOINC_READ_FAILED, "[NULL]");

				goto func_exit;
			}

			ulonglong	value = sequence++;

			switch (dtype_get_mtype(dtype)) {
			case DATA_INT: {
				ibool	usign;
				ulint	len = dfield_get_len(dfield);

				usign = dtype_get_prtype(dtype) & DATA_UNSIGNED;
				mach_write_ulonglong(b, value, len, usign);

				break;
				}

			case DATA_FLOAT:
				mach_float_write(
					b, static_cast<float>(value));
				break;

			case DATA_DOUBLE:
				mach_double_write(
					b, static_cast<double>(value));
				break;

			default:
				ut_ad(0);
			}
		}

write_buffers:
		/* Build all entries for all the indexes to be created
		in a single scan of the clustered index. */

		ulint	s_idx_cnt = 0;
		bool	skip_sort = skip_pk_sort
			&& dict_index_is_clust(merge_buf[0]->index);

		for (ulint i = 0; i < n_index; i++, skip_sort = false) {
			row_merge_buf_t*	buf	= merge_buf[i];
			merge_file_t*		file	= &files[i];
			ulint			rows_added = 0;

			if (dict_index_is_spatial(buf->index)) {
				if (!row) {
					continue;
				}

				ut_ad(sp_tuples[s_idx_cnt]->get_index()
				      == buf->index);

				/* If the geometry field is invalid, report
				error. */
				if (!row_geo_field_is_valid(row, buf->index)) {
					err = DB_CANT_CREATE_GEOMETRY_OBJECT;
					break;
				}

				sp_tuples[s_idx_cnt]->add(row, ext);
				s_idx_cnt++;

				continue;
			}

			if (UNIV_LIKELY
			    (row && (rows_added = row_merge_buf_add(
					buf, fts_index, old_table, new_table,
					psort_info, row, ext, &doc_id,
					conv_heap, &err,
					&v_heap, eval_table, trx)))) {

				/* If we are creating FTS index,
				a single row can generate more
				records for tokenized word */
				file->n_rec += rows_added;

				if (err != DB_SUCCESS) {
					ut_ad(err == DB_TOO_BIG_RECORD);
					break;
				}

				if (doc_id > max_doc_id) {
					max_doc_id = doc_id;
				}

				if (buf->index->type & DICT_FTS) {
					/* Check if error occurs in child thread */
					for (ulint j = 0;
					     j < fts_sort_pll_degree; j++) {
						if (psort_info[j].error
							!= DB_SUCCESS) {
							err = psort_info[j].error;
							trx->error_key_num = i;
							break;
						}
					}

					if (err != DB_SUCCESS) {
						break;
					}
				}

				if (skip_sort) {
					ut_ad(buf->n_tuples > 0);
					const mtuple_t*	curr =
						&buf->tuples[buf->n_tuples - 1];

					ut_ad(i == 0);
					ut_ad(dict_index_is_clust(merge_buf[0]->index));
					/* Detect duplicates by comparing the
					current record with previous record.
					When temp file is not used, records
					should be in sorted order. */
					if (prev_mtuple.fields != NULL
					    && (row_mtuple_cmp(
						&prev_mtuple, curr,
						&clust_dup) == 0)) {

						err = DB_DUPLICATE_KEY;
						trx->error_key_num
							= key_numbers[0];
						goto func_exit;
					}

					prev_mtuple.fields = curr->fields;
				}

				continue;
			}

			if (err == DB_COMPUTE_VALUE_FAILED) {
				trx->error_key_num = i;
				goto func_exit;
			}

			if (buf->index->type & DICT_FTS) {
				if (!row || !doc_id) {
					continue;
				}
			}

			/* The buffer must be sufficiently large
			to hold at least one record. It may only
			be empty when we reach the end of the
			clustered index. row_merge_buf_add()
			must not have been called in this loop. */
			ut_ad(buf->n_tuples || row == NULL);

			/* We have enough data tuples to form a block.
			Sort them and write to disk if temp file is used
			or insert into index if temp file is not used. */
			ut_ad(old_table == new_table
			      ? !dict_index_is_clust(buf->index)
			      : (i == 0) == dict_index_is_clust(buf->index));

			/* We have enough data tuples to form a block.
			Sort them (if !skip_sort) and write to disk. */

			if (buf->n_tuples) {
				if (skip_sort) {
					/* Temporary File is not used.
					so insert sorted block to the index */
					if (row != NULL) {
						bool	mtr_committed = false;

						/* We have to do insert the
						cached spatial index rows, since
						after the mtr_commit, the cluster
						index page could be updated, then
						the data in cached rows become
						invalid. */
						err = row_merge_spatial_rows(
							trx->id, sp_tuples,
							num_spatial,
							row_heap, sp_heap,
							&pcur, &mtr,
							&mtr_committed);

						if (err != DB_SUCCESS) {
							goto func_exit;
						}

						/* We are not at the end of
						the scan yet. We must
						mtr_commit() in order to be
						able to call log_free_check()
						in row_merge_insert_index_tuples().
						Due to mtr_commit(), the
						current row will be invalid, and
						we must reread it on the next
						loop iteration. */
						if (!mtr_committed) {
							btr_pcur_move_to_prev_on_page(
								&pcur);
							btr_pcur_store_position(
								&pcur, &mtr);

							mtr_commit(&mtr);
						}
					}

					mem_heap_empty(mtuple_heap);
					prev_mtuple.fields = prev_fields;

					row_mtuple_create(
						&buf->tuples[buf->n_tuples - 1],
						&prev_mtuple, n_uniq,
						mtuple_heap);

					if (clust_btr_bulk == NULL) {
						clust_btr_bulk = UT_NEW_NOKEY(
							BtrBulk(index[i],
								trx->id,
								observer));

						clust_btr_bulk->init();
					} else {
						clust_btr_bulk->latch();
					}

					err = row_merge_insert_index_tuples(
						trx->id, index[i], old_table,
						-1, NULL, buf, clust_btr_bulk);

					if (row == NULL) {
						err = clust_btr_bulk->finish(
							err);
						UT_DELETE(clust_btr_bulk);
						clust_btr_bulk = NULL;
					} else {
						/* Release latches for possible
						log_free_chck in spatial index
						build. */
						clust_btr_bulk->release();
					}

					if (err != DB_SUCCESS) {
						break;
					}

					if (row != NULL) {
						/* Restore the cursor on the
						previous clustered index record,
						and empty the buffer. The next
						iteration of the outer loop will
						advance the cursor and read the
						next record (the one which we
						had to ignore due to the buffer
						overflow). */
						mtr_start(&mtr);
						btr_pcur_restore_position(
							BTR_SEARCH_LEAF, &pcur,
							&mtr);
						buf = row_merge_buf_empty(buf);
						/* Restart the outer loop on the
						record. We did not insert it
						into any index yet. */
						ut_ad(i == 0);
						break;
					}
				} else if (dict_index_is_unique(buf->index)) {
					row_merge_dup_t	dup = {
						buf->index, table, col_map, 0};

					row_merge_buf_sort(buf, &dup);

					if (dup.n_dup) {
						err = DB_DUPLICATE_KEY;
						trx->error_key_num
							= key_numbers[i];
						break;
					}
				} else {
					row_merge_buf_sort(buf, NULL);
				}
			} else if (online && new_table == old_table) {
				/* Note the newest transaction that
				modified this index when the scan was
				completed. We prevent older readers
				from accessing this index, to ensure
				read consistency. */

				trx_id_t	max_trx_id;

				ut_a(row == NULL);
				rw_lock_x_lock(
					dict_index_get_lock(buf->index));
				ut_a(dict_index_get_online_status(buf->index)
				     == ONLINE_INDEX_CREATION);

				max_trx_id = row_log_get_max_trx(buf->index);

				if (max_trx_id > buf->index->trx_id) {
					buf->index->trx_id = max_trx_id;
				}

				rw_lock_x_unlock(
					dict_index_get_lock(buf->index));
			}

			/* Secondary index and clustered index which is
			not in sorted order can use the temporary file.
			Fulltext index should not use the temporary file. */
			if (!skip_sort && !(buf->index->type & DICT_FTS)) {
				/* In case we can have all rows in sort buffer,
				we can insert directly into the index without
				temporary file if clustered index does not uses
				temporary file. */
				if (row == NULL && file->fd == -1
				    && !clust_temp_file) {
					DBUG_EXECUTE_IF(
						"row_merge_write_failure",
						err = DB_TEMP_FILE_WRITE_FAIL;
						trx->error_key_num = i;
						goto all_done;);

					DBUG_EXECUTE_IF(
						"row_merge_tmpfile_fail",
						err = DB_OUT_OF_MEMORY;
						trx->error_key_num = i;
						goto all_done;);

					BtrBulk	btr_bulk(index[i], trx->id,
							 observer);
					btr_bulk.init();

					err = row_merge_insert_index_tuples(
						trx->id, index[i], old_table,
						-1, NULL, buf, &btr_bulk);

					err = btr_bulk.finish(err);

					DBUG_EXECUTE_IF(
						"row_merge_insert_big_row",
						err = DB_TOO_BIG_RECORD;);

					if (err != DB_SUCCESS) {
						break;
					}
				} else {
					if (row_merge_file_create_if_needed(
						file, tmpfd,
						buf->n_tuples, path) < 0) {
						err = DB_OUT_OF_MEMORY;
						trx->error_key_num = i;
						goto func_exit;
					}

					/* Ensure that duplicates in the
					clustered index will be detected before
					inserting secondary index records. */
					if (dict_index_is_clust(buf->index)) {
						clust_temp_file = true;
					}

					ut_ad(file->n_rec > 0);

					row_merge_buf_write(buf, file, block);

					if (!row_merge_write(
						    file->fd, file->offset++,
						    block)) {
						err = DB_TEMP_FILE_WRITE_FAIL;
						trx->error_key_num = i;
						break;
					}

					UNIV_MEM_INVALID(
						&block[0], srv_sort_buf_size);
				}
			}
			merge_buf[i] = row_merge_buf_empty(buf);

			if (UNIV_LIKELY(row != NULL)) {
				/* Try writing the record again, now
				that the buffer has been written out
				and emptied. */

				if (UNIV_UNLIKELY
				    (!(rows_added = row_merge_buf_add(
						buf, fts_index, old_table,
						new_table, psort_info, row, ext,
						&doc_id, conv_heap,
						&err, &v_heap, table, trx)))) {
					/* An empty buffer should have enough
					room for at least one record. */
					ut_error;
				}

				if (err != DB_SUCCESS) {
					break;
				}

				file->n_rec += rows_added;
			}
		}

		if (row == NULL) {
			goto all_done;
		}

		if (err != DB_SUCCESS) {
			goto func_exit;
		}

		if (v_heap) {
			mem_heap_empty(v_heap);
		}
	}

func_exit:
	/* row_merge_spatial_rows may have committed
	the mtr	before an error occurs. */
	if (mtr.is_active()) {
		mtr_commit(&mtr);
	}
	mem_heap_free(row_heap);
	ut_free(nonnull);

all_done:
	if (clust_btr_bulk != NULL) {
		ut_ad(err != DB_SUCCESS);
		clust_btr_bulk->latch();
		err = clust_btr_bulk->finish(
			err);
		UT_DELETE(clust_btr_bulk);
	}

	if (prev_fields != NULL) {
		ut_free(prev_fields);
		mem_heap_free(mtuple_heap);
	}

	if (v_heap) {
		mem_heap_free(v_heap);
	}

	if (conv_heap != NULL) {
		mem_heap_free(conv_heap);
	}

#ifdef FTS_INTERNAL_DIAG_PRINT
	DEBUG_FTS_SORT_PRINT("FTS_SORT: Complete Scan Table\n");
#endif
	if (fts_pll_sort) {
		bool	all_exit = false;
		ulint	trial_count = 0;
		const ulint max_trial_count = 10000;

wait_again:
                /* Check if error occurs in child thread */
		for (ulint j = 0; j < fts_sort_pll_degree; j++) {
			if (psort_info[j].error != DB_SUCCESS) {
				err = psort_info[j].error;
				trx->error_key_num = j;
				break;
			}
		}

		/* Tell all children that parent has done scanning */
		for (ulint i = 0; i < fts_sort_pll_degree; i++) {
			if (err == DB_SUCCESS) {
				psort_info[i].state = FTS_PARENT_COMPLETE;
			} else {
				psort_info[i].state = FTS_PARENT_EXITING;
			}
		}

		/* Now wait all children to report back to be completed */
		os_event_wait_time_low(fts_parallel_sort_event,
				       1000000, sig_count);

		for (ulint i = 0; i < fts_sort_pll_degree; i++) {
			if (psort_info[i].child_status != FTS_CHILD_COMPLETE
			    && psort_info[i].child_status != FTS_CHILD_EXITING) {
				sig_count = os_event_reset(
					fts_parallel_sort_event);
				goto wait_again;
			}
		}

		/* Now all children should complete, wait a bit until
		they all finish setting the event, before we free everything.
		This has a 10 second timeout */
		do {
			all_exit = true;

			for (ulint j = 0; j < fts_sort_pll_degree; j++) {
				if (psort_info[j].child_status
				    != FTS_CHILD_EXITING) {
					all_exit = false;
					os_thread_sleep(1000);
					break;
				}
			}
			trial_count++;
		} while (!all_exit && trial_count < max_trial_count);

		if (!all_exit) {
			ib::fatal() << "Not all child sort threads exited"
				" when creating FTS index '"
				<< fts_sort_idx->name << "'";
		}
	}

#ifdef FTS_INTERNAL_DIAG_PRINT
	DEBUG_FTS_SORT_PRINT("FTS_SORT: Complete Tokenization\n");
#endif
	for (ulint i = 0; i < n_index; i++) {
		row_merge_buf_free(merge_buf[i]);
	}

	row_fts_free_pll_merge_buf(psort_info);

	ut_free(merge_buf);

	btr_pcur_close(&pcur);

	if (sp_tuples != NULL) {
		for (ulint i = 0; i < num_spatial; i++) {
			UT_DELETE(sp_tuples[i]);
		}
		ut_free(sp_tuples);

		if (sp_heap) {
			mem_heap_free(sp_heap);
		}
	}

	/* Update the next Doc ID we used. Table should be locked, so
	no concurrent DML */
	if (max_doc_id && err == DB_SUCCESS) {
		/* Sync fts cache for other fts indexes to keep all
		fts indexes consistent in sync_doc_id. */
		err = fts_sync_table(const_cast<dict_table_t*>(new_table),
				     false, true, false);

		if (err == DB_SUCCESS) {
			fts_update_next_doc_id(
				0, new_table,
				old_table->name.m_name, max_doc_id);
		}
	}

	trx->op_info = "";

	DBUG_RETURN(err);
}

/** Write a record via buffer 2 and read the next record to buffer N.
@param N number of the buffer (0 or 1)
@param INDEX record descriptor
@param AT_END statement to execute at end of input */
#define ROW_MERGE_WRITE_GET_NEXT_LOW(N, INDEX, AT_END)			\
	do {								\
		b2 = row_merge_write_rec(&block[2 * srv_sort_buf_size], \
					 &buf[2], b2,			\
					 of->fd, &of->offset,		\
					 mrec##N, offsets##N);		\
		if (UNIV_UNLIKELY(!b2 || ++of->n_rec > file->n_rec)) {	\
			goto corrupt;					\
		}							\
		b##N = row_merge_read_rec(&block[N * srv_sort_buf_size],\
					  &buf[N], b##N, INDEX,		\
					  file->fd, foffs##N,		\
					  &mrec##N, offsets##N);	\
		if (UNIV_UNLIKELY(!b##N)) {				\
			if (mrec##N) {					\
				goto corrupt;				\
			}						\
			AT_END;						\
		}							\
	} while (0)

#ifdef HAVE_PSI_STAGE_INTERFACE
#define ROW_MERGE_WRITE_GET_NEXT(N, INDEX, AT_END)			\
	do {								\
		if (stage != NULL) {					\
			stage->inc();					\
		}							\
		ROW_MERGE_WRITE_GET_NEXT_LOW(N, INDEX, AT_END);		\
	} while (0)
#else /* HAVE_PSI_STAGE_INTERFACE */
#define ROW_MERGE_WRITE_GET_NEXT(N, INDEX, AT_END)			\
	ROW_MERGE_WRITE_GET_NEXT_LOW(N, INDEX, AT_END)
#endif /* HAVE_PSI_STAGE_INTERFACE */

/** Merge two blocks of records on disk and write a bigger block.
@param[in]	dup	descriptor of index being created
@param[in]	file	file containing index entries
@param[in,out]	block	3 buffers
@param[in,out]	foffs0	offset of first source list in the file
@param[in,out]	foffs1	offset of second source list in the file
@param[in,out]	of	output file
@param[in,out]	stage	performance schema accounting object, used by
ALTER TABLE. If not NULL stage->inc() will be called for each record
processed.
@return DB_SUCCESS or error code */
static MY_ATTRIBUTE((warn_unused_result))
dberr_t
row_merge_blocks(
	const row_merge_dup_t*	dup,
	const merge_file_t*	file,
	row_merge_block_t*	block,
	ulint*			foffs0,
	ulint*			foffs1,
	merge_file_t*		of,
	ut_stage_alter_t*	stage)
{
	mem_heap_t*	heap;	/*!< memory heap for offsets0, offsets1 */

	mrec_buf_t*	buf;	/*!< buffer for handling
				split mrec in block[] */
	const byte*	b0;	/*!< pointer to block[0] */
	const byte*	b1;	/*!< pointer to block[srv_sort_buf_size] */
	byte*		b2;	/*!< pointer to block[2 * srv_sort_buf_size] */
	const mrec_t*	mrec0;	/*!< merge rec, points to block[0] or buf[0] */
	const mrec_t*	mrec1;	/*!< merge rec, points to
				block[srv_sort_buf_size] or buf[1] */
	ulint*		offsets0;/* offsets of mrec0 */
	ulint*		offsets1;/* offsets of mrec1 */

	DBUG_ENTER("row_merge_blocks");
	DBUG_PRINT("ib_merge_sort",
		   ("fd=%d,%lu+%lu to fd=%d,%lu",
		    file->fd, ulong(*foffs0), ulong(*foffs1),
		    of->fd, ulong(of->offset)));

	heap = row_merge_heap_create(dup->index, &buf, &offsets0, &offsets1);

	/* Write a record and read the next record.  Split the output
	file in two halves, which can be merged on the following pass. */

	if (!row_merge_read(file->fd, *foffs0, &block[0])
	    || !row_merge_read(file->fd, *foffs1, &block[srv_sort_buf_size])) {
corrupt:
		mem_heap_free(heap);
		DBUG_RETURN(DB_CORRUPTION);
	}

	b0 = &block[0];
	b1 = &block[srv_sort_buf_size];
	b2 = &block[2 * srv_sort_buf_size];

	b0 = row_merge_read_rec(
		&block[0], &buf[0], b0, dup->index,
		file->fd, foffs0, &mrec0, offsets0);
	b1 = row_merge_read_rec(
		&block[srv_sort_buf_size],
		&buf[srv_sort_buf_size], b1, dup->index,
		file->fd, foffs1, &mrec1, offsets1);
	if (UNIV_UNLIKELY(!b0 && mrec0)
	    || UNIV_UNLIKELY(!b1 && mrec1)) {

		goto corrupt;
	}

	while (mrec0 && mrec1) {
		int cmp = cmp_rec_rec_simple(
			mrec0, mrec1, offsets0, offsets1,
			dup->index, dup->table);
		if (cmp < 0) {
			ROW_MERGE_WRITE_GET_NEXT(0, dup->index, goto merged);
		} else if (cmp) {
			ROW_MERGE_WRITE_GET_NEXT(1, dup->index, goto merged);
		} else {
			mem_heap_free(heap);
			DBUG_RETURN(DB_DUPLICATE_KEY);
		}
	}

merged:
	if (mrec0) {
		/* append all mrec0 to output */
		for (;;) {
			ROW_MERGE_WRITE_GET_NEXT(0, dup->index, goto done0);
		}
	}
done0:
	if (mrec1) {
		/* append all mrec1 to output */
		for (;;) {
			ROW_MERGE_WRITE_GET_NEXT(1, dup->index, goto done1);
		}
	}
done1:

	mem_heap_free(heap);
	b2 = row_merge_write_eof(&block[2 * srv_sort_buf_size],
				 b2, of->fd, &of->offset);
	DBUG_RETURN(b2 ? DB_SUCCESS : DB_CORRUPTION);
}

/** Copy a block of index entries.
@param[in]	index	index being created
@param[in]	file	input file
@param[in,out]	block	3 buffers
@param[in,out]	foffs0	input file offset
@param[in,out]	of	output file
@param[in,out]	stage	performance schema accounting object, used by
ALTER TABLE. If not NULL stage->inc() will be called for each record
processed.
@return TRUE on success, FALSE on failure */
static MY_ATTRIBUTE((warn_unused_result))
ibool
row_merge_blocks_copy(
	const dict_index_t*	index,
	const merge_file_t*	file,
	row_merge_block_t*	block,
	ulint*			foffs0,
	merge_file_t*		of,
	ut_stage_alter_t*	stage)
{
	mem_heap_t*	heap;	/*!< memory heap for offsets0, offsets1 */

	mrec_buf_t*	buf;	/*!< buffer for handling
				split mrec in block[] */
	const byte*	b0;	/*!< pointer to block[0] */
	byte*		b2;	/*!< pointer to block[2 * srv_sort_buf_size] */
	const mrec_t*	mrec0;	/*!< merge rec, points to block[0] */
	ulint*		offsets0;/* offsets of mrec0 */
	ulint*		offsets1;/* dummy offsets */

	DBUG_ENTER("row_merge_blocks_copy");
	DBUG_PRINT("ib_merge_sort",
		   ("fd=%d," ULINTPF " to fd=%d," ULINTPF,
		    file->fd, *foffs0,
		    of->fd, of->offset));

	heap = row_merge_heap_create(index, &buf, &offsets0, &offsets1);

	/* Write a record and read the next record.  Split the output
	file in two halves, which can be merged on the following pass. */

	if (!row_merge_read(file->fd, *foffs0, &block[0])) {
corrupt:
		mem_heap_free(heap);
		DBUG_RETURN(FALSE);
	}

	b0 = &block[0];

	b2 = &block[2 * srv_sort_buf_size];

	b0 = row_merge_read_rec(&block[0], &buf[0], b0, index,
				file->fd, foffs0, &mrec0, offsets0);
	if (UNIV_UNLIKELY(!b0 && mrec0)) {

		goto corrupt;
	}

	if (mrec0) {
		/* append all mrec0 to output */
		for (;;) {
			ROW_MERGE_WRITE_GET_NEXT(0, index, goto done0);
		}
	}
done0:

	/* The file offset points to the beginning of the last page
	that has been read.  Update it to point to the next block. */
	(*foffs0)++;

	mem_heap_free(heap);
	DBUG_RETURN(row_merge_write_eof(&block[2 * srv_sort_buf_size],
					b2, of->fd, &of->offset)
		    != NULL);
}

/** Merge disk files.
@param[in]	trx		transaction
@param[in]	dup		descriptor of index being created
@param[in,out]	file		file containing index entries
@param[in,out]	block		3 buffers
@param[in,out]	tmpfd		temporary file handle
@param[in,out]	num_run		Number of runs that remain to be merged
@param[in,out]	run_offset	Array that contains the first offset number
for each merge run
@param[in,out]	stage		performance schema accounting object, used by
ALTER TABLE. If not NULL stage->inc() will be called for each record
processed.
@return DB_SUCCESS or error code */
static
dberr_t
row_merge(
	trx_t*			trx,
	const row_merge_dup_t*	dup,
	merge_file_t*		file,
	row_merge_block_t*	block,
	int*			tmpfd,
	ulint*			num_run,
	ulint*			run_offset,
	ut_stage_alter_t*	stage)
{
	ulint		foffs0;	/*!< first input offset */
	ulint		foffs1;	/*!< second input offset */
	dberr_t		error;	/*!< error code */
	merge_file_t	of;	/*!< output file */
	const ulint	ihalf	= run_offset[*num_run / 2];
				/*!< half the input file */
	ulint		n_run	= 0;
				/*!< num of runs generated from this merge */

	UNIV_MEM_ASSERT_W(&block[0], 3 * srv_sort_buf_size);

	ut_ad(ihalf < file->offset);

	of.fd = *tmpfd;
	of.offset = 0;
	of.n_rec = 0;

#ifdef POSIX_FADV_SEQUENTIAL
	/* The input file will be read sequentially, starting from the
	beginning and the middle.  In Linux, the POSIX_FADV_SEQUENTIAL
	affects the entire file.  Each block will be read exactly once. */
	posix_fadvise(file->fd, 0, 0,
		      POSIX_FADV_SEQUENTIAL | POSIX_FADV_NOREUSE);
#endif /* POSIX_FADV_SEQUENTIAL */

	/* Merge blocks to the output file. */
	foffs0 = 0;
	foffs1 = ihalf;

	UNIV_MEM_INVALID(run_offset, *num_run * sizeof *run_offset);

	for (; foffs0 < ihalf && foffs1 < file->offset; foffs0++, foffs1++) {

		if (trx_is_interrupted(trx)) {
			return(DB_INTERRUPTED);
		}

		/* Remember the offset number for this run */
		run_offset[n_run++] = of.offset;

		error = row_merge_blocks(dup, file, block,
					 &foffs0, &foffs1, &of, stage);

		if (error != DB_SUCCESS) {
			return(error);
		}

	}

	/* Copy the last blocks, if there are any. */

	while (foffs0 < ihalf) {
		if (UNIV_UNLIKELY(trx_is_interrupted(trx))) {
			return(DB_INTERRUPTED);
		}

		/* Remember the offset number for this run */
		run_offset[n_run++] = of.offset;

		if (!row_merge_blocks_copy(dup->index, file, block,
					   &foffs0, &of, stage)) {
			return(DB_CORRUPTION);
		}
	}

	ut_ad(foffs0 == ihalf);

	while (foffs1 < file->offset) {
		if (trx_is_interrupted(trx)) {
			return(DB_INTERRUPTED);
		}

		/* Remember the offset number for this run */
		run_offset[n_run++] = of.offset;

		if (!row_merge_blocks_copy(dup->index, file, block,
					   &foffs1, &of, stage)) {
			return(DB_CORRUPTION);
		}
	}

	ut_ad(foffs1 == file->offset);

	if (UNIV_UNLIKELY(of.n_rec != file->n_rec)) {
		return(DB_CORRUPTION);
	}

	ut_ad(n_run <= *num_run);

	*num_run = n_run;

	/* Each run can contain one or more offsets. As merge goes on,
	the number of runs (to merge) will reduce until we have one
	single run. So the number of runs will always be smaller than
	the number of offsets in file */
	ut_ad((*num_run) <= file->offset);

	/* The number of offsets in output file is always equal or
	smaller than input file */
	ut_ad(of.offset <= file->offset);

	/* Swap file descriptors for the next pass. */
	*tmpfd = file->fd;
	*file = of;

	UNIV_MEM_INVALID(&block[0], 3 * srv_sort_buf_size);

	return(DB_SUCCESS);
}

/** Merge disk files.
@param[in]	trx	transaction
@param[in]	dup	descriptor of index being created
@param[in,out]	file	file containing index entries
@param[in,out]	block	3 buffers
@param[in,out]	tmpfd	temporary file handle
@param[in,out]	stage	performance schema accounting object, used by
ALTER TABLE. If not NULL, stage->begin_phase_sort() will be called initially
and then stage->inc() will be called for each record processed.
@return DB_SUCCESS or error code */
dberr_t
row_merge_sort(
	trx_t*			trx,
	const row_merge_dup_t*	dup,
	merge_file_t*		file,
	row_merge_block_t*	block,
	int*			tmpfd,
	ut_stage_alter_t*	stage /* = NULL */)
{
	const ulint	half	= file->offset / 2;
	ulint		num_runs;
	ulint*		run_offset;
	dberr_t		error	= DB_SUCCESS;
	DBUG_ENTER("row_merge_sort");

	/* Record the number of merge runs we need to perform */
	num_runs = file->offset;

	if (stage != NULL) {
		stage->begin_phase_sort(log2(num_runs));
	}

	/* If num_runs are less than 1, nothing to merge */
	if (num_runs <= 1) {
		DBUG_RETURN(error);
	}

	/* "run_offset" records each run's first offset number */
	run_offset = (ulint*) ut_malloc_nokey(file->offset * sizeof(ulint));

	/* This tells row_merge() where to start for the first round
	of merge. */
	run_offset[half] = half;

	/* The file should always contain at least one byte (the end
	of file marker).  Thus, it must be at least one block. */
	ut_ad(file->offset > 0);

	/* Merge the runs until we have one big run */
	do {
		error = row_merge(trx, dup, file, block, tmpfd,
				  &num_runs, run_offset, stage);

		if (error != DB_SUCCESS) {
			break;
		}

		UNIV_MEM_ASSERT_RW(run_offset, num_runs * sizeof *run_offset);
	} while (num_runs > 1);

	ut_free(run_offset);

	DBUG_RETURN(error);
}

/** Copy externally stored columns to the data tuple.
@param[in]	mrec		record containing BLOB pointers,
or NULL to use tuple instead
@param[in]	offsets		offsets of mrec
@param[in]	zip_size	compressed page size in bytes, or 0
@param[in,out]	tuple		data tuple
@param[in,out]	heap		memory heap */
static
void
row_merge_copy_blobs(
	const mrec_t*		mrec,
	const ulint*		offsets,
	const page_size_t&	page_size,
	dtuple_t*		tuple,
	mem_heap_t*		heap)
{
	ut_ad(mrec == NULL || rec_offs_any_extern(offsets));

	for (ulint i = 0; i < dtuple_get_n_fields(tuple); i++) {
		ulint		len;
		const void*	data;
		dfield_t*	field = dtuple_get_nth_field(tuple, i);
		ulint		field_len;
		const byte*	field_data;

		if (!dfield_is_ext(field)) {
			continue;
		}

		ut_ad(!dfield_is_null(field));

		/* During the creation of a PRIMARY KEY, the table is
		X-locked, and we skip copying records that have been
		marked for deletion. Therefore, externally stored
		columns cannot possibly be freed between the time the
		BLOB pointers are read (row_merge_read_clustered_index())
		and dereferenced (below). */
		if (mrec == NULL) {
			field_data
				= static_cast<byte*>(dfield_get_data(field));
			field_len = dfield_get_len(field);

			ut_a(field_len >= BTR_EXTERN_FIELD_REF_SIZE);

			ut_a(memcmp(field_data + field_len
				     - BTR_EXTERN_FIELD_REF_SIZE,
				     field_ref_zero,
				     BTR_EXTERN_FIELD_REF_SIZE));

			data = btr_copy_externally_stored_field(
				&len, field_data, page_size, field_len, heap);
		} else {
			data = btr_rec_copy_externally_stored_field(
				mrec, offsets, page_size, i, &len, heap);
		}

		/* Because we have locked the table, any records
		written by incomplete transactions must have been
		rolled back already. There must not be any incomplete
		BLOB columns. */
		ut_a(data);

		dfield_set_data(field, data, len);
	}
}

/** Convert a merge record to a typed data tuple. Note that externally
stored fields are not copied to heap.
@param[in,out]	index	index on the table
@param[in]	mtuple	merge record
@param[in]	heap	memory heap from which memory needed is allocated
@return	index entry built. */
static
void
row_merge_mtuple_to_dtuple(
	dict_index_t*	index,
	dtuple_t*	dtuple,
	const mtuple_t* mtuple)
{
	ut_ad(!dict_index_is_ibuf(index));

	memcpy(dtuple->fields, mtuple->fields,
	       dtuple->n_fields * sizeof *mtuple->fields);
}

/** Insert sorted data tuples to the index.
@param[in]	trx_id		transaction identifier
@param[in]	index		index to be inserted
@param[in]	old_table	old table
@param[in]	fd		file descriptor
@param[in,out]	block		file buffer
@param[in]	row_buf		row_buf the sorted data tuples,
or NULL if fd, block will be used instead
@param[in,out]	btr_bulk	btr bulk instance
@param[in,out]	stage		performance schema accounting object, used by
ALTER TABLE. If not NULL stage->begin_phase_insert() will be called initially
and then stage->inc() will be called for each record that is processed.
@return DB_SUCCESS or error number */
static	MY_ATTRIBUTE((warn_unused_result))
dberr_t
row_merge_insert_index_tuples(
	trx_id_t		trx_id,
	dict_index_t*		index,
	const dict_table_t*	old_table,
	int			fd,
	row_merge_block_t*	block,
	const row_merge_buf_t*	row_buf,
	BtrBulk*		btr_bulk,
	ut_stage_alter_t*	stage /* = NULL */)
{
	const byte*		b;
	mem_heap_t*		heap;
	mem_heap_t*		tuple_heap;
	dberr_t			error = DB_SUCCESS;
	ulint			foffs = 0;
	ulint*			offsets;
	mrec_buf_t*		buf;
	ulint			n_rows = 0;
	dtuple_t*		dtuple;
	DBUG_ENTER("row_merge_insert_index_tuples");

	ut_ad(!srv_read_only_mode);
	ut_ad(!(index->type & DICT_FTS));
	ut_ad(!dict_index_is_spatial(index));
	ut_ad(trx_id);

	if (stage != NULL) {
		stage->begin_phase_insert();
	}

	tuple_heap = mem_heap_create(1000);

	{
		ulint i	= 1 + REC_OFFS_HEADER_SIZE
			+ dict_index_get_n_fields(index);
		heap = mem_heap_create(sizeof *buf + i * sizeof *offsets);
		offsets = static_cast<ulint*>(
			mem_heap_alloc(heap, i * sizeof *offsets));
		offsets[0] = i;
		offsets[1] = dict_index_get_n_fields(index);
	}

	if (row_buf != NULL) {
		ut_ad(fd == -1);
		ut_ad(block == NULL);
		DBUG_EXECUTE_IF("row_merge_read_failure",
				error = DB_CORRUPTION;
				goto err_exit;);
		buf = NULL;
		b = NULL;
		dtuple = dtuple_create(
			heap, dict_index_get_n_fields(index));
		dtuple_set_n_fields_cmp(
			dtuple, dict_index_get_n_unique_in_tree(index));
	} else {
		b = block;
		dtuple = NULL;

		if (!row_merge_read(fd, foffs, block)) {
			error = DB_CORRUPTION;
			goto err_exit;
		} else {
			buf = static_cast<mrec_buf_t*>(
				mem_heap_alloc(heap, sizeof *buf));
		}
	}


	for (;;) {
		const mrec_t*	mrec;
		ulint		n_ext;
		mtr_t		mtr;

		if (stage != NULL) {
			stage->inc();
		}

		if (row_buf != NULL) {
			if (n_rows >= row_buf->n_tuples) {
				break;
			}

			/* Convert merge tuple record from
			row buffer to data tuple record */
			row_merge_mtuple_to_dtuple(
				index, dtuple, &row_buf->tuples[n_rows]);

			n_ext = dtuple_get_n_ext(dtuple);
			n_rows++;
			/* BLOB pointers must be copied from dtuple */
			mrec = NULL;
		} else {
			b = row_merge_read_rec(block, buf, b, index,
					       fd, &foffs, &mrec, offsets);
			if (UNIV_UNLIKELY(!b)) {
				/* End of list, or I/O error */
				if (mrec) {
					error = DB_CORRUPTION;
				}
				break;
			}

			dtuple = row_rec_to_index_entry_low(
				mrec, index, offsets, &n_ext, tuple_heap);
		}

		dict_index_t*	old_index
			= dict_table_get_first_index(old_table);

		if (dict_index_is_clust(index)
		    && dict_index_is_online_ddl(old_index)) {
			error = row_log_table_get_error(old_index);
			if (error != DB_SUCCESS) {
				break;
			}
		}

		if (!n_ext) {
			/* There are no externally stored columns. */
		} else {
			ut_ad(dict_index_is_clust(index));
			/* Off-page columns can be fetched safely
			when concurrent modifications to the table
			are disabled. (Purge can process delete-marked
			records, but row_merge_read_clustered_index()
			would have skipped them.)

			When concurrent modifications are enabled,
			row_merge_read_clustered_index() will
			only see rows from transactions that were
			committed before the ALTER TABLE started
			(REPEATABLE READ).

			Any modifications after the
			row_merge_read_clustered_index() scan
			will go through row_log_table_apply().
			Any modifications to off-page columns
			will be tracked by
			row_log_table_blob_alloc() and
			row_log_table_blob_free(). */
			row_merge_copy_blobs(
				mrec, offsets,
				dict_table_page_size(old_table),
				dtuple, tuple_heap);
		}

		ut_ad(dtuple_validate(dtuple));

		error = btr_bulk->insert(dtuple);

		if (error != DB_SUCCESS) {
			goto err_exit;
		}

		mem_heap_empty(tuple_heap);
	}

err_exit:
	mem_heap_free(tuple_heap);
	mem_heap_free(heap);

	DBUG_RETURN(error);
}

/*********************************************************************//**
Sets an exclusive lock on a table, for the duration of creating indexes.
@return error code or DB_SUCCESS */
dberr_t
row_merge_lock_table(
/*=================*/
	trx_t*		trx,		/*!< in/out: transaction */
	dict_table_t*	table,		/*!< in: table to lock */
	enum lock_mode	mode)		/*!< in: LOCK_X or LOCK_S */
{
	ut_ad(!srv_read_only_mode);
	ut_ad(mode == LOCK_X || mode == LOCK_S);

	trx->op_info = "setting table lock for creating or dropping index";
	trx->ddl = true;
	/* Trx for DDL should not be forced to rollback for now */
	trx->in_innodb |= TRX_FORCE_ROLLBACK_DISABLE;

	return(lock_table_for_trx(table, trx, mode));
}

/*********************************************************************//**
Drop an index that was created before an error occurred.
The data dictionary must have been locked exclusively by the caller,
because the transaction will not be committed. */
static
void
row_merge_drop_index_dict(
/*======================*/
	trx_t*		trx,	/*!< in/out: dictionary transaction */
	index_id_t	index_id)/*!< in: index identifier */
{
	static const char sql[] =
		"PROCEDURE DROP_INDEX_PROC () IS\n"
		"BEGIN\n"
		"DELETE FROM SYS_FIELDS WHERE INDEX_ID=:indexid;\n"
		"DELETE FROM SYS_INDEXES WHERE ID=:indexid;\n"
		"END;\n";
	dberr_t		error;
	pars_info_t*	info;

	ut_ad(!srv_read_only_mode);
	ut_ad(mutex_own(&dict_sys->mutex));
	ut_ad(trx->dict_operation_lock_mode == RW_X_LATCH);
	ut_ad(trx_get_dict_operation(trx) == TRX_DICT_OP_INDEX);
	ut_ad(rw_lock_own(dict_operation_lock, RW_LOCK_X));

	info = pars_info_create();
	pars_info_add_ull_literal(info, "indexid", index_id);
	trx->op_info = "dropping index from dictionary";
	error = que_eval_sql(info, sql, FALSE, trx);

	if (error != DB_SUCCESS) {
		/* Even though we ensure that DDL transactions are WAIT
		and DEADLOCK free, we could encounter other errors e.g.,
		DB_TOO_MANY_CONCURRENT_TRXS. */
		trx->error_state = DB_SUCCESS;

		ib::error() << "row_merge_drop_index_dict failed with error "
			<< error;
	}

	trx->op_info = "";
}

/*********************************************************************//**
Drop indexes that were created before an error occurred.
The data dictionary must have been locked exclusively by the caller,
because the transaction will not be committed. */
void
row_merge_drop_indexes_dict(
/*========================*/
	trx_t*		trx,	/*!< in/out: dictionary transaction */
	table_id_t	table_id)/*!< in: table identifier */
{
	static const char sql[] =
		"PROCEDURE DROP_INDEXES_PROC () IS\n"
		"ixid CHAR;\n"
		"found INT;\n"

		"DECLARE CURSOR index_cur IS\n"
		" SELECT ID FROM SYS_INDEXES\n"
		" WHERE TABLE_ID=:tableid AND\n"
		" SUBSTR(NAME,0,1)='" TEMP_INDEX_PREFIX_STR "'\n"
		"FOR UPDATE;\n"

		"BEGIN\n"
		"found := 1;\n"
		"OPEN index_cur;\n"
		"WHILE found = 1 LOOP\n"
		"  FETCH index_cur INTO ixid;\n"
		"  IF (SQL % NOTFOUND) THEN\n"
		"    found := 0;\n"
		"  ELSE\n"
		"    DELETE FROM SYS_FIELDS WHERE INDEX_ID=ixid;\n"
		"    DELETE FROM SYS_INDEXES WHERE CURRENT OF index_cur;\n"
		"  END IF;\n"
		"END LOOP;\n"
		"CLOSE index_cur;\n"

		"END;\n";
	dberr_t		error;
	pars_info_t*	info;

	ut_ad(!srv_read_only_mode);
	ut_ad(mutex_own(&dict_sys->mutex));
	ut_ad(trx->dict_operation_lock_mode == RW_X_LATCH);
	ut_ad(trx_get_dict_operation(trx) == TRX_DICT_OP_INDEX);
	ut_ad(rw_lock_own(dict_operation_lock, RW_LOCK_X));

	/* It is possible that table->n_ref_count > 1 when
	locked=TRUE. In this case, all code that should have an open
	handle to the table be waiting for the next statement to execute,
	or waiting for a meta-data lock.

	A concurrent purge will be prevented by dict_operation_lock. */

	info = pars_info_create();
	pars_info_add_ull_literal(info, "tableid", table_id);
	trx->op_info = "dropping indexes";
	error = que_eval_sql(info, sql, FALSE, trx);

	switch (error) {
	case DB_SUCCESS:
		break;
	default:
		/* Even though we ensure that DDL transactions are WAIT
		and DEADLOCK free, we could encounter other errors e.g.,
		DB_TOO_MANY_CONCURRENT_TRXS. */
		ib::error() << "row_merge_drop_indexes_dict failed with error "
			<< error;
		/* fall through */
	case DB_TOO_MANY_CONCURRENT_TRXS:
		trx->error_state = DB_SUCCESS;
	}

	trx->op_info = "";
}

/*********************************************************************//**
Drop indexes that were created before an error occurred.
The data dictionary must have been locked exclusively by the caller,
because the transaction will not be committed. */
void
row_merge_drop_indexes(
/*===================*/
	trx_t*		trx,	/*!< in/out: dictionary transaction */
	dict_table_t*	table,	/*!< in/out: table containing the indexes */
	ibool		locked)	/*!< in: TRUE=table locked,
				FALSE=may need to do a lazy drop */
{
	dict_index_t*	index;
	dict_index_t*	next_index;

	ut_ad(!srv_read_only_mode);
	ut_ad(mutex_own(&dict_sys->mutex));
	ut_ad(trx->dict_operation_lock_mode == RW_X_LATCH);
	ut_ad(trx_get_dict_operation(trx) == TRX_DICT_OP_INDEX);
	ut_ad(rw_lock_own(dict_operation_lock, RW_LOCK_X));

	index = dict_table_get_first_index(table);
	ut_ad(dict_index_is_clust(index));
	ut_ad(dict_index_get_online_status(index) == ONLINE_INDEX_COMPLETE);

	/* the caller should have an open handle to the table */
	ut_ad(table->get_ref_count() >= 1);

	/* It is possible that table->n_ref_count > 1 when
	locked=TRUE. In this case, all code that should have an open
	handle to the table be waiting for the next statement to execute,
	or waiting for a meta-data lock.

	A concurrent purge will be prevented by dict_operation_lock. */

	if (!locked && table->get_ref_count() > 1) {
		/* We will have to drop the indexes later, when the
		table is guaranteed to be no longer in use.  Mark the
		indexes as incomplete and corrupted, so that other
		threads will stop using them.  Let dict_table_close()
		or crash recovery or the next invocation of
		prepare_inplace_alter_table() take care of dropping
		the indexes. */

		while ((index = dict_table_get_next_index(index)) != NULL) {
			ut_ad(!dict_index_is_clust(index));

			switch (dict_index_get_online_status(index)) {
			case ONLINE_INDEX_ABORTED_DROPPED:
				continue;
			case ONLINE_INDEX_COMPLETE:
				if (index->is_committed()) {
					/* Do nothing to already
					published indexes. */
				} else if (index->type & DICT_FTS) {
					/* Drop a completed FULLTEXT
					index, due to a timeout during
					MDL upgrade for
					commit_inplace_alter_table().
					Because only concurrent reads
					are allowed (and they are not
					seeing this index yet) we
					are safe to drop the index. */
					dict_index_t* prev = UT_LIST_GET_PREV(
						indexes, index);
					/* At least there should be
					the clustered index before
					this one. */
					ut_ad(prev);
					ut_a(table->fts);
					fts_drop_index(table, index, trx);
					/* Since
					INNOBASE_SHARE::idx_trans_tbl
					is shared between all open
					ha_innobase handles to this
					table, no thread should be
					accessing this dict_index_t
					object. Also, we should be
					holding LOCK=SHARED MDL on the
					table even after the MDL
					upgrade timeout. */

					/* We can remove a DICT_FTS
					index from the cache, because
					we do not allow ADD FULLTEXT INDEX
					with LOCK=NONE. If we allowed that,
					we should exclude FTS entries from
					prebuilt->ins_node->entry_list
					in ins_node_create_entry_list(). */
					dict_index_remove_from_cache(
						table, index);
					index = prev;
				} else {
					rw_lock_x_lock(
						dict_index_get_lock(index));
					dict_index_set_online_status(
						index, ONLINE_INDEX_ABORTED);
					index->type |= DICT_CORRUPT;
					table->drop_aborted = TRUE;
					goto drop_aborted;
				}
				continue;
			case ONLINE_INDEX_CREATION:
				rw_lock_x_lock(dict_index_get_lock(index));
				ut_ad(!index->is_committed());
				row_log_abort_sec(index);
			drop_aborted:
				rw_lock_x_unlock(dict_index_get_lock(index));

				DEBUG_SYNC_C("merge_drop_index_after_abort");
				/* covered by dict_sys->mutex */
				MONITOR_INC(MONITOR_BACKGROUND_DROP_INDEX);
				/* fall through */
			case ONLINE_INDEX_ABORTED:
				/* Drop the index tree from the
				data dictionary and free it from
				the tablespace, but keep the object
				in the data dictionary cache. */
				row_merge_drop_index_dict(trx, index->id);
				rw_lock_x_lock(dict_index_get_lock(index));
				dict_index_set_online_status(
					index, ONLINE_INDEX_ABORTED_DROPPED);
				rw_lock_x_unlock(dict_index_get_lock(index));
				table->drop_aborted = TRUE;
				continue;
			}
			ut_error;
		}

		return;
	}

	row_merge_drop_indexes_dict(trx, table->id);

	/* Invalidate all row_prebuilt_t::ins_graph that are referring
	to this table. That is, force row_get_prebuilt_insert_row() to
	rebuild prebuilt->ins_node->entry_list). */
	ut_ad(table->def_trx_id <= trx->id);
	table->def_trx_id = trx->id;

	next_index = dict_table_get_next_index(index);

	while ((index = next_index) != NULL) {
		/* read the next pointer before freeing the index */
		next_index = dict_table_get_next_index(index);

		ut_ad(!dict_index_is_clust(index));

		if (!index->is_committed()) {
			/* If it is FTS index, drop from table->fts
			and also drop its auxiliary tables */
			if (index->type & DICT_FTS) {
				ut_a(table->fts);
				fts_drop_index(table, index, trx);
			}

			switch (dict_index_get_online_status(index)) {
			case ONLINE_INDEX_CREATION:
				/* This state should only be possible
				when prepare_inplace_alter_table() fails
				after invoking row_merge_create_index().
				In inplace_alter_table(),
				row_merge_build_indexes()
				should never leave the index in this state.
				It would invoke row_log_abort_sec() on
				failure. */
			case ONLINE_INDEX_COMPLETE:
				/* In these cases, we are able to drop
				the index straight. The DROP INDEX was
				never deferred. */
				break;
			case ONLINE_INDEX_ABORTED:
			case ONLINE_INDEX_ABORTED_DROPPED:
				/* covered by dict_sys->mutex */
				MONITOR_DEC(MONITOR_BACKGROUND_DROP_INDEX);
			}

			dict_index_remove_from_cache(table, index);
		}
	}

	table->drop_aborted = FALSE;
	ut_d(dict_table_check_for_dup_indexes(table, CHECK_ALL_COMPLETE));
}

/*********************************************************************//**
Drop all partially created indexes during crash recovery. */
void
row_merge_drop_temp_indexes(void)
/*=============================*/
{
	static const char sql[] =
		"PROCEDURE DROP_TEMP_INDEXES_PROC () IS\n"
		"ixid CHAR;\n"
		"found INT;\n"

		"DECLARE CURSOR index_cur IS\n"
		" SELECT ID FROM SYS_INDEXES\n"
		" WHERE SUBSTR(NAME,0,1)='" TEMP_INDEX_PREFIX_STR "'\n"
		"FOR UPDATE;\n"

		"BEGIN\n"
		"found := 1;\n"
		"OPEN index_cur;\n"
		"WHILE found = 1 LOOP\n"
		"  FETCH index_cur INTO ixid;\n"
		"  IF (SQL % NOTFOUND) THEN\n"
		"    found := 0;\n"
		"  ELSE\n"
		"    DELETE FROM SYS_FIELDS WHERE INDEX_ID=ixid;\n"
		"    DELETE FROM SYS_INDEXES WHERE CURRENT OF index_cur;\n"
		"  END IF;\n"
		"END LOOP;\n"
		"CLOSE index_cur;\n"
		"END;\n";
	trx_t*	trx;
	dberr_t	error;

	/* Load the table definitions that contain partially defined
	indexes, so that the data dictionary information can be checked
	when accessing the tablename.ibd files. */
	trx = trx_allocate_for_background();
	trx->op_info = "dropping partially created indexes";
	row_mysql_lock_data_dictionary(trx);
	/* Ensure that this transaction will be rolled back and locks
	will be released, if the server gets killed before the commit
	gets written to the redo log. */
	trx_set_dict_operation(trx, TRX_DICT_OP_INDEX);

	trx->op_info = "dropping indexes";
	error = que_eval_sql(NULL, sql, FALSE, trx);

	if (error != DB_SUCCESS) {
		/* Even though we ensure that DDL transactions are WAIT
		and DEADLOCK free, we could encounter other errors e.g.,
		DB_TOO_MANY_CONCURRENT_TRXS. */
		trx->error_state = DB_SUCCESS;

		ib::error() << "row_merge_drop_temp_indexes failed with error"
			<< error;
	}

	trx_commit_for_mysql(trx);
	row_mysql_unlock_data_dictionary(trx);
	trx_free_for_background(trx);
}


/** Create temporary merge files in the given paramater path, and if
UNIV_PFS_IO defined, register the file descriptor with Performance Schema.
@param[in]	path	location for creating temporary merge files.
@return File descriptor */
int
row_merge_file_create_low(
	const char*	path)
{
    int fd;
    if (path == NULL) {
      path = innobase_mysql_tmpdir();
    }
#ifdef UNIV_PFS_IO
	/* This temp file open does not go through normal
	file APIs, add instrumentation to register with
	performance schema */
	struct PSI_file_locker*	locker = NULL;
        char *filepath = NULL;
        filepath =
            fil_make_filepath(path, "Innodb Merge Temp File", NO_EXT, false);
        PSI_file_locker_state	state;
        locker = PSI_FILE_CALL(get_thread_file_name_locker)(
            &state, innodb_temp_file_key.m_value, PSI_FILE_OPEN, filepath,
            &locker);
        if (locker != NULL) {
		PSI_FILE_CALL(start_file_open_wait)(locker,
						__FILE__,
						__LINE__);
        }
#endif
	fd = innobase_mysql_tmpfile(path);
#ifdef UNIV_PFS_IO
	 if (locker != NULL) {
		PSI_FILE_CALL(end_file_open_wait_and_bind_to_descriptor)(
				locker, fd);
		}
#endif

	if (fd < 0) {
		ib::error() << "Cannot create temporary merge file";
		return(-1);
	}
	return(fd);
}


/** Create a merge file in the given location.
@param[out]	merge_file	merge file structure
@param[in]	path		location for creating temporary file
@return file descriptor, or -1 on failure */
int
row_merge_file_create(
	merge_file_t*	merge_file,
	const char*	path)
{
	merge_file->fd = row_merge_file_create_low(path);
	merge_file->offset = 0;
	merge_file->n_rec = 0;

	if (merge_file->fd >= 0) {
		if (srv_disable_sort_file_cache) {
			os_file_set_nocache(merge_file->fd,
				"row0merge.cc", "sort");
		}
	}
	return(merge_file->fd);
}

/*********************************************************************//**
Destroy a merge file. And de-register the file from Performance Schema
if UNIV_PFS_IO is defined. */
void
row_merge_file_destroy_low(
/*=======================*/
	int		fd)	/*!< in: merge file descriptor */
{
#ifdef UNIV_PFS_IO
	struct PSI_file_locker*	locker = NULL;
	PSI_file_locker_state	state;
	locker = PSI_FILE_CALL(get_thread_file_descriptor_locker)(
			       &state, fd, PSI_FILE_CLOSE);
	if (locker != NULL) {
		PSI_FILE_CALL(start_file_wait)(
			      locker, 0, __FILE__, __LINE__);
	}
#endif
	if (fd >= 0) {
		close(fd);
	}
#ifdef UNIV_PFS_IO
	if (locker != NULL) {
		PSI_FILE_CALL(end_file_wait)(locker, 0);
	}
#endif
}
/*********************************************************************//**
Destroy a merge file. */
void
row_merge_file_destroy(
/*===================*/
	merge_file_t*	merge_file)	/*!< in/out: merge file structure */
{
	ut_ad(!srv_read_only_mode);

	if (merge_file->fd != -1) {
		row_merge_file_destroy_low(merge_file->fd);
		merge_file->fd = -1;
	}
}

/*********************************************************************//**
Rename an index in the dictionary that was created. The data
dictionary must have been locked exclusively by the caller, because
the transaction will not be committed.
@return DB_SUCCESS if all OK */
dberr_t
row_merge_rename_index_to_add(
/*==========================*/
	trx_t*		trx,		/*!< in/out: transaction */
	table_id_t	table_id,	/*!< in: table identifier */
	index_id_t	index_id)	/*!< in: index identifier */
{
	dberr_t		err = DB_SUCCESS;
	pars_info_t*	info = pars_info_create();

	/* We use the private SQL parser of Innobase to generate the
	query graphs needed in renaming indexes. */

	static const char rename_index[] =
		"PROCEDURE RENAME_INDEX_PROC () IS\n"
		"BEGIN\n"
		"UPDATE SYS_INDEXES SET NAME=SUBSTR(NAME,1,LENGTH(NAME)-1)\n"
		"WHERE TABLE_ID = :tableid AND ID = :indexid;\n"
		"END;\n";

	ut_ad(trx);
	ut_a(trx->dict_operation_lock_mode == RW_X_LATCH);
	ut_ad(trx_get_dict_operation(trx) == TRX_DICT_OP_INDEX);

	trx->op_info = "renaming index to add";

	pars_info_add_ull_literal(info, "tableid", table_id);
	pars_info_add_ull_literal(info, "indexid", index_id);

	err = que_eval_sql(info, rename_index, FALSE, trx);

	if (err != DB_SUCCESS) {
		/* Even though we ensure that DDL transactions are WAIT
		and DEADLOCK free, we could encounter other errors e.g.,
		DB_TOO_MANY_CONCURRENT_TRXS. */
		trx->error_state = DB_SUCCESS;

		ib::error() << "row_merge_rename_index_to_add failed with"
			" error " << err;
	}

	trx->op_info = "";

	return(err);
}

/*********************************************************************//**
Rename an index in the dictionary that is to be dropped. The data
dictionary must have been locked exclusively by the caller, because
the transaction will not be committed.
@return DB_SUCCESS if all OK */
dberr_t
row_merge_rename_index_to_drop(
/*===========================*/
	trx_t*		trx,		/*!< in/out: transaction */
	table_id_t	table_id,	/*!< in: table identifier */
	index_id_t	index_id)	/*!< in: index identifier */
{
	dberr_t		err;
	pars_info_t*	info = pars_info_create();

	ut_ad(!srv_read_only_mode);

	/* We use the private SQL parser of Innobase to generate the
	query graphs needed in renaming indexes. */

	static const char rename_index[] =
		"PROCEDURE RENAME_INDEX_PROC () IS\n"
		"BEGIN\n"
		"UPDATE SYS_INDEXES SET NAME=CONCAT('"
		TEMP_INDEX_PREFIX_STR "',NAME)\n"
		"WHERE TABLE_ID = :tableid AND ID = :indexid;\n"
		"END;\n";

	ut_ad(trx);
	ut_a(trx->dict_operation_lock_mode == RW_X_LATCH);
	ut_ad(trx_get_dict_operation(trx) == TRX_DICT_OP_INDEX);

	trx->op_info = "renaming index to drop";

	pars_info_add_ull_literal(info, "tableid", table_id);
	pars_info_add_ull_literal(info, "indexid", index_id);

	err = que_eval_sql(info, rename_index, FALSE, trx);

	if (err != DB_SUCCESS) {
		/* Even though we ensure that DDL transactions are WAIT
		and DEADLOCK free, we could encounter other errors e.g.,
		DB_TOO_MANY_CONCURRENT_TRXS. */
		trx->error_state = DB_SUCCESS;

		ib::error() << "row_merge_rename_index_to_drop failed with"
			" error " << err;
	}

	trx->op_info = "";

	return(err);
}

/*********************************************************************//**
Provide a new pathname for a table that is being renamed if it belongs to
a file-per-table tablespace.  The caller is responsible for freeing the
memory allocated for the return value.
@return new pathname of tablespace file, or NULL if space = 0 */
char*
row_make_new_pathname(
/*==================*/
	dict_table_t*	table,		/*!< in: table to be renamed */
	const char*	new_name)	/*!< in: new name */
{
	char*	new_path;
	char*	old_path;

	ut_ad(!is_system_tablespace(table->space));

	old_path = fil_space_get_first_path(table->space);
	ut_a(old_path);

	new_path = os_file_make_new_pathname(old_path, new_name);

	ut_free(old_path);

	return(new_path);
}

/*********************************************************************//**
Rename the tables in the data dictionary.  The data dictionary must
have been locked exclusively by the caller, because the transaction
will not be committed.
@return error code or DB_SUCCESS */
dberr_t
row_merge_rename_tables_dict(
/*=========================*/
	dict_table_t*	old_table,	/*!< in/out: old table, renamed to
					tmp_name */
	dict_table_t*	new_table,	/*!< in/out: new table, renamed to
					old_table->name */
	const char*	tmp_name,	/*!< in: new name for old_table */
	trx_t*		trx)		/*!< in/out: dictionary transaction */
{
	dberr_t		err	= DB_ERROR;
	pars_info_t*	info;

	ut_ad(!srv_read_only_mode);
	ut_ad(old_table != new_table);
	ut_ad(mutex_own(&dict_sys->mutex));
	ut_a(trx->dict_operation_lock_mode == RW_X_LATCH);
	ut_ad(trx_get_dict_operation(trx) == TRX_DICT_OP_TABLE
	      || trx_get_dict_operation(trx) == TRX_DICT_OP_INDEX);

	trx->op_info = "renaming tables";

	/* We use the private SQL parser of Innobase to generate the query
	graphs needed in updating the dictionary data in system tables. */

	info = pars_info_create();

	pars_info_add_str_literal(info, "new_name", new_table->name.m_name);
	pars_info_add_str_literal(info, "old_name", old_table->name.m_name);
	pars_info_add_str_literal(info, "tmp_name", tmp_name);

	err = que_eval_sql(info,
			   "PROCEDURE RENAME_TABLES () IS\n"
			   "BEGIN\n"
			   "UPDATE SYS_TABLES SET NAME = :tmp_name\n"
			   " WHERE NAME = :old_name;\n"
			   "UPDATE SYS_TABLES SET NAME = :old_name\n"
			   " WHERE NAME = :new_name;\n"
			   "END;\n", FALSE, trx);

	/* Update SYS_TABLESPACES and SYS_DATAFILES if the old table being
	renamed is a single-table tablespace, which must be implicitly
	renamed along with the table. */
	if (err == DB_SUCCESS
	    && dict_table_is_file_per_table(old_table)
	    && !old_table->ibd_file_missing) {
		/* Make pathname to update SYS_DATAFILES. */
		char* tmp_path = row_make_new_pathname(old_table, tmp_name);

		info = pars_info_create();

		pars_info_add_str_literal(info, "tmp_name", tmp_name);
		pars_info_add_str_literal(info, "tmp_path", tmp_path);
		pars_info_add_int4_literal(info, "old_space",
					   (lint) old_table->space);

		err = que_eval_sql(info,
				   "PROCEDURE RENAME_OLD_SPACE () IS\n"
				   "BEGIN\n"
				   "UPDATE SYS_TABLESPACES"
				   " SET NAME = :tmp_name\n"
				   " WHERE SPACE = :old_space;\n"
				   "UPDATE SYS_DATAFILES"
				   " SET PATH = :tmp_path\n"
				   " WHERE SPACE = :old_space;\n"
				   "END;\n", FALSE, trx);

		ut_free(tmp_path);
	}

	/* Update SYS_TABLESPACES and SYS_DATAFILES if the new table being
	renamed is a single-table tablespace, which must be implicitly
	renamed along with the table. */
	if (err == DB_SUCCESS
	    && dict_table_is_file_per_table(new_table)) {
		/* Make pathname to update SYS_DATAFILES. */
		char* old_path = row_make_new_pathname(
			new_table, old_table->name.m_name);

		info = pars_info_create();

		pars_info_add_str_literal(info, "old_name",
					  old_table->name.m_name);
		pars_info_add_str_literal(info, "old_path", old_path);
		pars_info_add_int4_literal(info, "new_space",
					   (lint) new_table->space);

		err = que_eval_sql(info,
				   "PROCEDURE RENAME_NEW_SPACE () IS\n"
				   "BEGIN\n"
				   "UPDATE SYS_TABLESPACES"
				   " SET NAME = :old_name\n"
				   " WHERE SPACE = :new_space;\n"
				   "UPDATE SYS_DATAFILES"
				   " SET PATH = :old_path\n"
				   " WHERE SPACE = :new_space;\n"
				   "END;\n", FALSE, trx);

		ut_free(old_path);
	}

	if (err == DB_SUCCESS && dict_table_is_discarded(new_table)) {
		err = row_import_update_discarded_flag(
			trx, new_table->id, true, true);
	}

	trx->op_info = "";

	return(err);
}

/** Create and execute a query graph for creating an index.
@param[in,out]	trx	trx
@param[in,out]	table	table
@param[in,out]	index	index
@param[in]	add_v	new virtual columns added along with add index call
@return DB_SUCCESS or error code */
static MY_ATTRIBUTE((warn_unused_result))
dberr_t
row_merge_create_index_graph(
	trx_t*			trx,
	dict_table_t*		table,
	dict_index_t*		index,
	const dict_add_v_col_t* add_v)
{
	ind_node_t*	node;		/*!< Index creation node */
	mem_heap_t*	heap;		/*!< Memory heap */
	que_thr_t*	thr;		/*!< Query thread */
	dberr_t		err;

	DBUG_ENTER("row_merge_create_index_graph");

	ut_ad(trx);
	ut_ad(table);
	ut_ad(index);

	heap = mem_heap_create(512);

	index->table = table;
	node = ind_create_graph_create(index, heap, add_v);
	thr = pars_complete_graph_for_exec(node, trx, heap, NULL);

	ut_a(thr == que_fork_start_command(
			static_cast<que_fork_t*>(que_node_get_parent(thr))));

	que_run_threads(thr);

	err = trx->error_state;

	que_graph_free((que_t*) que_node_get_parent(thr));

	DBUG_RETURN(err);
}

/** Create the index and load in to the dictionary.
@param[in,out]	trx		trx (sets error_state)
@param[in,out]	table		the index is on this table
@param[in]	index_def	the index definition
@param[in]	add_v		new virtual columns added along with add
				index call
@return index, or NULL on error */
dict_index_t*
row_merge_create_index(
	trx_t*			trx,
	dict_table_t*		table,
	const index_def_t*	index_def,
	const dict_add_v_col_t*	add_v)
{
	dict_index_t*	index;
	dberr_t		err;
	ulint		n_fields = index_def->n_fields;
	ulint		i;
	bool		has_new_v_col = false;

	DBUG_ENTER("row_merge_create_index");

	ut_ad(!srv_read_only_mode);

	/* Create the index prototype, using the passed in def, this is not
	a persistent operation. We pass 0 as the space id, and determine at
	a lower level the space id where to store the table. */

	index = dict_mem_index_create(table->name.m_name, index_def->name,
				      0, index_def->ind_type, n_fields);

	ut_a(index);

	index->set_committed(index_def->rebuild);

	for (i = 0; i < n_fields; i++) {
		const char*	name;
		index_field_t*	ifield = &index_def->fields[i];

		if (ifield->is_v_col) {
			if (ifield->col_no >= table->n_v_def) {
				ut_ad(ifield->col_no < table->n_v_def
				      + add_v->n_v_col);
				ut_ad(ifield->col_no >= table->n_v_def);
				name = add_v->v_col_name[
					ifield->col_no - table->n_v_def];

				has_new_v_col = true;
			} else {
				name = dict_table_get_v_col_name(
					table, ifield->col_no);
			}
		} else {
			name = dict_table_get_col_name(table, ifield->col_no);

		}

		dict_mem_index_add_field(index, name, ifield->prefix_len);
	}

	/* Add the index to SYS_INDEXES, using the index prototype. */
	err = row_merge_create_index_graph(trx, table, index, add_v);

	if (err == DB_SUCCESS) {

		index = dict_table_get_index_on_name(table, index_def->name,
						     index_def->rebuild);

		ut_a(index);

		index->parser = index_def->parser;
		index->is_ngram = index_def->is_ngram;
		index->has_new_v_col = has_new_v_col;

		/* Note the id of the transaction that created this
		index, we use it to restrict readers from accessing
		this index, to ensure read consistency. */
		ut_ad(index->trx_id == trx->id);
	} else {
		index = NULL;
	}

	DBUG_RETURN(index);
}

/*********************************************************************//**
Check if a transaction can use an index. */
ibool
row_merge_is_index_usable(
/*======================*/
	const trx_t*		trx,	/*!< in: transaction */
	const dict_index_t*	index)	/*!< in: index to check */
{
	if (!dict_index_is_clust(index)
	    && dict_index_is_online_ddl(index)) {
		/* Indexes that are being created are not useable. */
		return(FALSE);
	}

	return(!dict_index_is_corrupted(index)
	       && (dict_table_is_temporary(index->table)
		   || index->trx_id == 0
		   || !MVCC::is_view_active(trx->read_view)
		   || trx->read_view->changes_visible(
			   index->trx_id,
			   index->table->name)));
}

/*********************************************************************//**
Drop a table. The caller must have ensured that the background stats
thread is not processing the table. This can be done by calling
dict_stats_wait_bg_to_stop_using_table() after locking the dictionary and
before calling this function.
@return DB_SUCCESS or error code */
dberr_t
row_merge_drop_table(
/*=================*/
	trx_t*		trx,		/*!< in: transaction */
	dict_table_t*	table)		/*!< in: table to drop */
{
	ut_ad(!srv_read_only_mode);

	/* There must be no open transactions on the table. */
	ut_a(table->get_ref_count() == 0);

	return(row_drop_table_for_mysql(table->name.m_name,
					trx, false, false));
}

/** Write an MLOG_INDEX_LOAD record to indicate in the redo-log
that redo-logging of individual index pages was disabled, and
the flushing of such pages to the data files was completed.
@param[in]	index	an index tree on which redo logging was disabled */
static
void
row_merge_write_redo(
	const dict_index_t*	index)
{
	mtr_t	mtr;
	byte*	log_ptr;

	ut_ad(!dict_table_is_temporary(index->table));
	mtr.start();
	log_ptr = mlog_open(&mtr, 11 + 8);
	log_ptr = mlog_write_initial_log_record_low(
		MLOG_INDEX_LOAD,
		index->space, index->page, log_ptr, &mtr);
	mach_write_to_8(log_ptr, index->id);
	mlog_close(&mtr, log_ptr + 8);
	mtr.commit();
}

/** Build indexes on a table by reading a clustered index, creating a temporary
file containing index entries, merge sorting these index entries and inserting
sorted index entries to indexes.
@param[in]	trx		transaction
@param[in]	old_table	table where rows are read from
@param[in]	new_table	table where indexes are created; identical to
old_table unless creating a PRIMARY KEY
@param[in]	online		true if creating indexes online
@param[in]	indexes		indexes to be created
@param[in]	key_numbers	MySQL key numbers
@param[in]	n_indexes	size of indexes[]
@param[in,out]	table		MySQL table, for reporting erroneous key value
if applicable
@param[in]	add_cols	default values of added columns, or NULL
@param[in]	col_map		mapping of old column numbers to new ones, or
NULL if old_table == new_table
@param[in]	add_autoinc	number of added AUTO_INCREMENT columns, or
ULINT_UNDEFINED if none is added
@param[in,out]	sequence	autoinc sequence
@param[in]	skip_pk_sort	whether the new PRIMARY KEY will follow
existing order
@param[in,out]	stage		performance schema accounting object, used by
ALTER TABLE. stage->begin_phase_read_pk() will be called at the beginning of
this function and it will be passed to other functions for further accounting.
@param[in]	add_v		new virtual columns added along with indexes
@param[in]	eval_table	mysql table used to evaluate virtual column
				value, see innobase_get_computed_value().
@return DB_SUCCESS or error code */
dberr_t
row_merge_build_indexes(
	trx_t*			trx,
	dict_table_t*		old_table,
	dict_table_t*		new_table,
	bool			online,
	dict_index_t**		indexes,
	const ulint*		key_numbers,
	ulint			n_indexes,
	struct TABLE*		table,
	const dtuple_t*		add_cols,
	const ulint*		col_map,
	ulint			add_autoinc,
	ib_sequence_t&		sequence,
	bool			skip_pk_sort,
	ut_stage_alter_t*	stage,
	const dict_add_v_col_t*	add_v,
	struct TABLE*		eval_table)
{
	merge_file_t*		merge_files;
	row_merge_block_t*	block;
	ut_new_pfx_t		block_pfx;
	ulint			i;
	ulint			j;
	dberr_t			error;
	int			tmpfd = -1;
	dict_index_t*		fts_sort_idx = NULL;
	fts_psort_t*		psort_info = NULL;
	fts_psort_t*		merge_info = NULL;
	int64_t			sig_count = 0;
	bool			fts_psort_initiated = false;
	DBUG_ENTER("row_merge_build_indexes");

	ut_ad(!srv_read_only_mode);
	ut_ad((old_table == new_table) == !col_map);
	ut_ad(!add_cols || col_map);

	stage->begin_phase_read_pk(skip_pk_sort && new_table != old_table
				   ? n_indexes - 1
				   : n_indexes);

	/* Allocate memory for merge file data structure and initialize
	fields */

	ut_allocator<row_merge_block_t>	alloc(mem_key_row_merge_sort);

	/* This will allocate "3 * srv_sort_buf_size" elements of type
	row_merge_block_t. The latter is defined as byte. */
	block = alloc.allocate_large(3 * srv_sort_buf_size, &block_pfx);

	if (block == NULL) {
		DBUG_RETURN(DB_OUT_OF_MEMORY);
	}

	trx_start_if_not_started_xa(trx, true);

	/* Check if we need a flush observer to flush dirty pages.
	Since we disable redo logging in bulk load, so we should flush
	dirty pages before online log apply, because online log apply enables
	redo logging(we can do further optimization here).
	1. online add index: flush dirty pages right before row_log_apply().
	2. table rebuild: flush dirty pages before row_log_table_apply().

	we use bulk load to create all types of indexes except spatial index,
	for which redo logging is enabled. If we create only spatial indexes,
	we don't need to flush dirty pages at all. */
	bool	need_flush_observer = (old_table != new_table);

	for (i = 0; i < n_indexes; i++) {
		if (!dict_index_is_spatial(indexes[i])) {
			need_flush_observer = true;
		}
	}

	FlushObserver*	flush_observer = NULL;
	if (need_flush_observer) {
		flush_observer = UT_NEW_NOKEY(
			FlushObserver(new_table->space, trx, stage));

		trx_set_flush_observer(trx, flush_observer);
	}

	merge_files = static_cast<merge_file_t*>(
		ut_malloc_nokey(n_indexes * sizeof *merge_files));

	/* Initialize all the merge file descriptors, so that we
	don't call row_merge_file_destroy() on uninitialized
	merge file descriptor */

	for (i = 0; i < n_indexes; i++) {
		merge_files[i].fd = -1;
	}

	for (i = 0; i < n_indexes; i++) {
		if (indexes[i]->type & DICT_FTS) {
			ibool	opt_doc_id_size = FALSE;

			/* To build FTS index, we would need to extract
			doc's word, Doc ID, and word's position, so
			we need to build a "fts sort index" indexing
			on above three 'fields' */
			fts_sort_idx = row_merge_create_fts_sort_index(
				indexes[i], old_table, &opt_doc_id_size);

			row_merge_dup_t*	dup
				= static_cast<row_merge_dup_t*>(
					ut_malloc_nokey(sizeof *dup));
			dup->index = fts_sort_idx;
			dup->table = table;
			dup->col_map = col_map;
			dup->n_dup = 0;

			row_fts_psort_info_init(
				trx, dup, new_table, opt_doc_id_size,
				&psort_info, &merge_info);

			/* We need to ensure that we free the resources
			allocated */
			fts_psort_initiated = true;
		}
	}

	/* Reset the MySQL row buffer that is used when reporting
	duplicate keys. */
	innobase_rec_reset(table);

	/* Read clustered index of the table and create files for
	secondary index entries for merge sort */
	error = row_merge_read_clustered_index(
		trx, table, old_table, new_table, online, indexes,
		fts_sort_idx, psort_info, merge_files, key_numbers,
		n_indexes, add_cols, add_v, col_map, add_autoinc,
		sequence, block, skip_pk_sort, &tmpfd, stage, eval_table);

	stage->end_phase_read_pk();

	if (error != DB_SUCCESS) {

		goto func_exit;
	}

	DEBUG_SYNC_C("row_merge_after_scan");

	/* Now we have files containing index entries ready for
	sorting and inserting. */

	for (i = 0; i < n_indexes; i++) {
		dict_index_t*	sort_idx = indexes[i];

		if (dict_index_is_spatial(sort_idx)) {
			continue;
		}

		if (indexes[i]->type & DICT_FTS) {
			os_event_t	fts_parallel_merge_event;

			sort_idx = fts_sort_idx;

			fts_parallel_merge_event
				= merge_info[0].psort_common->merge_event;

			if (FTS_PLL_MERGE) {
				ulint	trial_count = 0;
				bool	all_exit = false;

				os_event_reset(fts_parallel_merge_event);
				row_fts_start_parallel_merge(merge_info);
wait_again:
				os_event_wait_time_low(
					fts_parallel_merge_event, 1000000,
					sig_count);

				for (j = 0; j < FTS_NUM_AUX_INDEX; j++) {
					if (merge_info[j].child_status
					    != FTS_CHILD_COMPLETE
					    && merge_info[j].child_status
					    != FTS_CHILD_EXITING) {
						sig_count = os_event_reset(
						fts_parallel_merge_event);

						goto wait_again;
					}
				}

				/* Now all children should complete, wait
				a bit until they all finish using event */
				while (!all_exit && trial_count < 10000) {
					all_exit = true;

					for (j = 0; j < FTS_NUM_AUX_INDEX;
					     j++) {
						if (merge_info[j].child_status
						    != FTS_CHILD_EXITING) {
							all_exit = false;
							os_thread_sleep(1000);
							break;
						}
					}
					trial_count++;
				}

				if (!all_exit) {
					ib::error() << "Not all child merge"
						" threads exited when creating"
						" FTS index '"
						<< indexes[i]->name << "'";
				} else {
					for (j = 0; j < FTS_NUM_AUX_INDEX;
					     j++) {

						os_thread_join(merge_info[j]
							       .thread_hdl);
					}
				}
			} else {
				/* This cannot report duplicates; an
				assertion would fail in that case. */
				error = row_fts_merge_insert(
					sort_idx, new_table,
					psort_info, 0);
			}

#ifdef FTS_INTERNAL_DIAG_PRINT
			DEBUG_FTS_SORT_PRINT("FTS_SORT: Complete Insert\n");
#endif
		} else if (merge_files[i].fd >= 0) {
			row_merge_dup_t	dup = {
				sort_idx, table, col_map, 0};

			error = row_merge_sort(
				trx, &dup, &merge_files[i],
				block, &tmpfd, stage);

			if (error == DB_SUCCESS) {
				BtrBulk	btr_bulk(sort_idx, trx->id,
						 flush_observer);
				btr_bulk.init();

				error = row_merge_insert_index_tuples(
					trx->id, sort_idx, old_table,
					merge_files[i].fd, block, NULL,
					&btr_bulk, stage);

				error = btr_bulk.finish(error);
			}
		}

		/* Close the temporary file to free up space. */
		row_merge_file_destroy(&merge_files[i]);

		if (indexes[i]->type & DICT_FTS) {
			row_fts_psort_info_destroy(psort_info, merge_info);
			fts_psort_initiated = false;
		} else if (error != DB_SUCCESS || !online) {
			/* Do not apply any online log. */
		} else if (old_table != new_table) {
			ut_ad(!sort_idx->online_log);
			ut_ad(sort_idx->online_status
			      == ONLINE_INDEX_COMPLETE);
		} else {
			ut_ad(need_flush_observer);

			flush_observer->flush();
			row_merge_write_redo(indexes[i]);

			DEBUG_SYNC_C("row_log_apply_before");
			error = row_log_apply(trx, sort_idx, table, stage);
			DEBUG_SYNC_C("row_log_apply_after");
		}

		if (error != DB_SUCCESS) {
			trx->error_key_num = key_numbers[i];
			goto func_exit;
		}

		if (indexes[i]->type & DICT_FTS && fts_enable_diag_print) {
			ib::info() << "Finished building full-text index "
				<< indexes[i]->name;
		}
	}

func_exit:
	DBUG_EXECUTE_IF(
		"ib_build_indexes_too_many_concurrent_trxs",
		error = DB_TOO_MANY_CONCURRENT_TRXS;
		trx->error_state = error;);

	if (fts_psort_initiated) {
		/* Clean up FTS psort related resource */
		row_fts_psort_info_destroy(psort_info, merge_info);
		fts_psort_initiated = false;
	}

	row_merge_file_destroy_low(tmpfd);

	for (i = 0; i < n_indexes; i++) {
		row_merge_file_destroy(&merge_files[i]);
	}

	if (fts_sort_idx) {
		dict_mem_index_free(fts_sort_idx);
	}

	ut_free(merge_files);

	alloc.deallocate_large(block, &block_pfx);

	DICT_TF2_FLAG_UNSET(new_table, DICT_TF2_FTS_ADD_DOC_ID);

	if (online && old_table == new_table && error != DB_SUCCESS) {
		/* On error, flag all online secondary index creation
		as aborted. */
		for (i = 0; i < n_indexes; i++) {
			ut_ad(!(indexes[i]->type & DICT_FTS));
			ut_ad(!indexes[i]->is_committed());
			ut_ad(!dict_index_is_clust(indexes[i]));

			/* Completed indexes should be dropped as
			well, and indexes whose creation was aborted
			should be dropped from the persistent
			storage. However, at this point we can only
			set some flags in the not-yet-published
			indexes. These indexes will be dropped later
			in row_merge_drop_indexes(), called by
			rollback_inplace_alter_table(). */

			switch (dict_index_get_online_status(indexes[i])) {
			case ONLINE_INDEX_COMPLETE:
				break;
			case ONLINE_INDEX_CREATION:
				rw_lock_x_lock(
					dict_index_get_lock(indexes[i]));
				row_log_abort_sec(indexes[i]);
				indexes[i]->type |= DICT_CORRUPT;
				rw_lock_x_unlock(
					dict_index_get_lock(indexes[i]));
				new_table->drop_aborted = TRUE;
				/* fall through */
			case ONLINE_INDEX_ABORTED_DROPPED:
			case ONLINE_INDEX_ABORTED:
				MONITOR_ATOMIC_INC(
					MONITOR_BACKGROUND_DROP_INDEX);
			}
		}
	}

	DBUG_EXECUTE_IF("ib_index_crash_after_bulk_load", DBUG_SUICIDE(););

	if (flush_observer != NULL) {
		ut_ad(need_flush_observer);

		DBUG_EXECUTE_IF("ib_index_build_fail_before_flush",
			error = DB_FAIL;
		);

		if (error != DB_SUCCESS) {
			flush_observer->interrupted();
		}

		flush_observer->flush();

		UT_DELETE(flush_observer);

		if (trx_is_interrupted(trx)) {
			error = DB_INTERRUPTED;
		}

		if (error == DB_SUCCESS && old_table != new_table) {
			for (const dict_index_t* index
				     = dict_table_get_first_index(new_table);
			     index != NULL;
			     index = dict_table_get_next_index(index)) {
				row_merge_write_redo(index);
			}
		}
	}

	DBUG_RETURN(error);
}
