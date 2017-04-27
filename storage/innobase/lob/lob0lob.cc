/*****************************************************************************

Copyright (c) 2015, 2017, Oracle and/or its affiliates. All Rights Reserved.

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

#include <sys/types.h>

#include "btr0pcur.h"
#include "fil0fil.h"
#include "lob0fit.h"
#include "lob0lob.h"
#include "lob0zip.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "row0upd.h"

namespace lob {

/** Gets the offset of the pointer to the externally stored part of a field.
@param[in]	offsets		array returned by rec_get_offsets()
@param[in]	n		index of the external field
@return offset of the pointer to the externally stored part */
ulint
btr_rec_get_field_ref_offs(
	const ulint*	offsets,
	ulint		n)
{
	ulint	field_ref_offs;
	ulint	local_len;

	ut_a(rec_offs_nth_extern(offsets, n));
	field_ref_offs = rec_get_nth_field_offs(offsets, n, &local_len);
	ut_a(local_len != UNIV_SQL_NULL);
	ut_a(local_len >= BTR_EXTERN_FIELD_REF_SIZE);

	return(field_ref_offs + local_len - BTR_EXTERN_FIELD_REF_SIZE);
}

/** When bulk load is being done, check if there is enough space in redo
log file. */
void
BtrContext::check_redolog_bulk()
{
	ut_ad(is_bulk());

	FlushObserver* observer = m_mtr->get_flush_observer();

	rec_block_fix();

	commit_btr_mtr();

	DEBUG_SYNC_C("blob_write_middle");

	log_free_check();

	start_btr_mtr();
	m_mtr->set_flush_observer(observer);

	rec_block_unfix();
	ut_ad(validate());
}

/** Allocate one BLOB page.
@return the allocated block of the BLOB page. */
buf_block_t*
BaseInserter::alloc_blob_page()
{
	ulint	r_extents;
	mtr_t	mtr_bulk;
	mtr_t*	alloc_mtr;

	ut_ad(fsp_check_tablespace_size(m_ctx->space()));

	if (m_ctx->is_bulk()) {
		mtr_start(&mtr_bulk);

		alloc_mtr = &mtr_bulk;
	} else {
		alloc_mtr = &m_blob_mtr;
	}

	page_no_t	hint_page_no = m_prev_page_no + 1;

	bool	success = fsp_reserve_free_extents(
		&r_extents, m_ctx->space(), 1, FSP_BLOB, alloc_mtr, 1);

	if (!success) {

		alloc_mtr->commit();
		m_status = DB_OUT_OF_FILE_SPACE;
		return(NULL);
	}

	m_cur_blob_block = btr_page_alloc(
		m_ctx->index(), hint_page_no, FSP_NO_DIR, 0,
		alloc_mtr, &m_blob_mtr);

	fil_space_t*	space = fil_space_get(m_ctx->space());

	space->release_free_extents(r_extents);

	if (m_ctx->is_bulk()) {
		alloc_mtr->commit();
	}

	m_cur_blob_page_no = page_get_page_no(
		buf_block_get_frame(m_cur_blob_block));

	return(m_cur_blob_block);
}

/** Check if there is enough space in log file. Commit and re-start the
mini transaction. */
void
BtrContext::check_redolog_normal()
{
	ut_ad(!is_bulk());

	FlushObserver* observer = m_mtr->get_flush_observer();
	store_position();

	commit_btr_mtr();

	DEBUG_SYNC_C("blob_write_middle");

	log_free_check();

	DEBUG_SYNC_C("blob_write_middle_after_check");

	start_btr_mtr();

	m_mtr->set_flush_observer(observer);

	restore_position();

	ut_ad(validate());
}

#ifdef UNIV_DEBUG
/** Write first blob page.
@param[in]	blob_j		the jth blob object of the record.
@param[in]	field		the big record field.
@return code as returned by the zlib. */
int
zInserter::write_first_page(
	size_t			blob_j,
	big_rec_field_t&	field)
{
	buf_block_t*	rec_block	= m_ctx->block();
	mtr_t*		mtr		= start_blob_mtr();

	buf_page_get(rec_block->page.id,
		     rec_block->page.size, RW_X_LATCH, mtr);

	buf_block_t*	blob_block = alloc_blob_page();

	if (dict_index_is_online_ddl(m_ctx->index())) {
		row_log_table_blob_alloc(m_ctx->index(),
					 m_cur_blob_page_no);
	}

	page_t*	blob_page  = buf_block_get_frame(blob_block);

	log_page_type(blob_page, 0);

	int	err = write_into_single_page();

	ut_ad(!dict_index_is_spatial(m_ctx->index()));

	const ulint	field_no = field.field_no;
	byte*	field_ref = btr_rec_get_field_ref(
		m_ctx->rec(), m_ctx->get_offsets(), field_no);
	ref_t	blobref(field_ref);

	if (err == Z_OK) {
		blobref.set_length(0, nullptr);
	} else if (err == Z_STREAM_END) {
		blobref.set_length(m_stream.total_in, nullptr);
	} else {
		ut_ad(0);
		return(err);
	}

	blobref.update(m_ctx->space(), m_cur_blob_page_no,
		       FIL_PAGE_NEXT, NULL);

	/* After writing the first blob page, update the blob reference. */
	if (!m_ctx->is_bulk()) {
		m_ctx->zblob_write_blobref(field_no, &m_blob_mtr);
	}

	m_prev_page_no = page_get_page_no(blob_page);

	/* Commit mtr and release uncompressed page frame to save memory.*/
	blob_free(m_ctx->index(), m_cur_blob_block, FALSE, mtr);

	return(err);
}

/** For the given blob field, update its length in the blob reference
which is available in the clustered index record.
@param[in]	field	the concerned blob field. */
void
zInserter::update_length_in_blobref(big_rec_field_t& field)
{
	/* After writing the last blob page, update the blob reference
	with the correct length. */

	const ulint	field_no = field.field_no;
	byte*	field_ref = btr_rec_get_field_ref(
		m_ctx->rec(), m_ctx->get_offsets(), field_no);

	ref_t	blobref(field_ref);
	blobref.set_length(m_stream.total_in, nullptr);

	if (!m_ctx->is_bulk()) {
		m_ctx->zblob_write_blobref(field_no, &m_blob_mtr);
	}
}

/** Write one blob field data.
@param[in]	blob_j	the blob field number
@return DB_SUCCESS on success, error code on failure. */
dberr_t
zInserter::write_one_blob(size_t blob_j)
{
	const big_rec_t*	vec = m_ctx->get_big_rec_vec();
	big_rec_field_t&	field = vec->fields[blob_j];

	int     err = deflateReset(&m_stream);
	ut_a(err == Z_OK);

	m_stream.next_in = (Bytef*) field.data;
	m_stream.avail_in = static_cast<uInt>(field.len);

	m_ctx->check_redolog();

	err = write_first_page(blob_j, field);

	for (ulint nth_blob_page = 1; err == Z_OK; ++nth_blob_page) {

		const ulint	commit_freq = 4;

		err = write_single_blob_page(blob_j, field, nth_blob_page);

		if (nth_blob_page % commit_freq == 0) {
			m_ctx->check_redolog();
		}
	}

	ut_ad(err == Z_STREAM_END);
	m_ctx->make_nth_extern(field.field_no);
	return(DB_SUCCESS);
}

/** Write contents into a single BLOB page.
@return code as returned by zlib. */
int
zInserter::write_into_single_page()
{
	const uint in_before = m_stream.avail_in;

	mtr_t*	const	mtr = &m_blob_mtr;

	/* Space available in compressed page to carry blob data */
	const page_size_t page_size = m_ctx->page_size();
	const uint payload_size_zip = page_size.physical() - FIL_PAGE_DATA;

	page_t*	blob_page = buf_block_get_frame(m_cur_blob_block);

	m_stream.next_out = blob_page + FIL_PAGE_DATA;
	m_stream.avail_out = static_cast<uInt>(payload_size_zip);

	int err = deflate(&m_stream, Z_FINISH);
	ut_a(err == Z_OK || err == Z_STREAM_END);
	ut_a(err == Z_STREAM_END || m_stream.avail_out == 0);

	const blob_page_info_t page_info(
		m_cur_blob_page_no,
		in_before - m_stream.avail_in,
		payload_size_zip - m_stream.avail_out);

	add_to_blob_dir(page_info);

	/* Write the "next BLOB page" pointer */
	mlog_write_ulint(blob_page + FIL_PAGE_NEXT, FIL_NULL,
			 MLOG_4BYTES, mtr);

	/* Initialize the unused "prev page" pointer */
	mlog_write_ulint(blob_page + FIL_PAGE_PREV, FIL_NULL,
			 MLOG_4BYTES, mtr);

	/* Write a back pointer to the record into the otherwise unused area.
	This information could be useful in debugging.  Later, we might want
	to implement the possibility to relocate BLOB pages.  Then, we would
	need to be able to adjust the BLOB pointer in the record.  We do not
	store the heap number of the record, because it can change in
	page_zip_reorganize() or btr_page_reorganize().  However, also the
	page number of the record may change when B-tree nodes are split or
	merged. */
	mlog_write_ulint(blob_page + FIL_PAGE_FILE_FLUSH_LSN,
			 m_ctx->space(), MLOG_4BYTES, mtr);

	mlog_write_ulint(blob_page + FIL_PAGE_FILE_FLUSH_LSN + 4,
			 m_ctx->get_page_no(), MLOG_4BYTES, mtr);

	if (m_stream.avail_out > 0) {
		/* Zero out the unused part of the page. */
		memset(blob_page + page_zip_get_size(m_ctx->get_page_zip())
		       - m_stream.avail_out, 0, m_stream.avail_out);
	}

	/* Redo log the page contents (the page is not modified). */
	mlog_log_string(blob_page + FIL_PAGE_FILE_FLUSH_LSN,
			page_zip_get_size(m_ctx->get_page_zip())
			- FIL_PAGE_FILE_FLUSH_LSN, mtr);

	/* Copy the page to compressed storage, because it will be flushed
	to disk from there. */
	page_zip_des_t* blob_page_zip = buf_block_get_page_zip(m_cur_blob_block);

	ut_ad(blob_page_zip);
	ut_ad(page_zip_get_size(blob_page_zip)
	      == page_zip_get_size(m_ctx->get_page_zip()));

	page_zip_des_t* page_zip = buf_block_get_page_zip(m_ctx->block());
	memcpy(blob_page_zip->data, blob_page, page_zip_get_size(page_zip));

	return(err);
}

/** Write one blob page.  This function will be repeatedly called
with an increasing nth_blob_page to completely write a BLOB.
@param[in]	blob_j		the jth blob object of the record.
@param[in]	field		the big record field.
@param[in]	nth_blob_page	count of the BLOB page (starting from 1).
@return code as returned by the zlib. */
int
zInserter::write_single_blob_page(
	size_t			blob_j,
	big_rec_field_t&	field,
	ulint			nth_blob_page)
{
	ut_ad(nth_blob_page > 0);

	buf_block_t*	rec_block	= m_ctx->block();
	mtr_t*		mtr		= start_blob_mtr();

	buf_page_get(rec_block->page.id,
		     rec_block->page.size, RW_X_LATCH, mtr);

	buf_block_t*	blob_block = alloc_blob_page();
	page_t*		blob_page  = buf_block_get_frame(blob_block);

	set_page_next();

	m_prev_page_no = page_get_page_no(blob_page);

	log_page_type(blob_page, nth_blob_page);

	int err = write_into_single_page();

	ut_ad(!dict_index_is_spatial(m_ctx->index()));

	if (err == Z_STREAM_END) {
		update_length_in_blobref(field);
	}

	/* Commit mtr and release uncompressed page frame to save memory.*/
	blob_free(m_ctx->index(), m_cur_blob_block, FALSE, mtr);

	return(err);
}

/** Prepare to write a compressed BLOB. Setup the zlib
compression stream.
@return DB_SUCCESS on success, error code on failure. */
dberr_t zInserter::prepare()
{
	/* Zlib deflate needs 128 kilobytes for the default
	window size, plus 512 << memLevel, plus a few
	kilobytes for small objects.  We use reduced memLevel
	to limit the memory consumption, and preallocate the
	heap, hoping to avoid memory fragmentation. */
	m_heap = mem_heap_create(250000);

	if (m_heap == NULL) {
		return(DB_OUT_OF_MEMORY);
	}

	page_zip_set_alloc(&m_stream, m_heap);
	int ret = deflateInit2(&m_stream, page_zip_level,
			       Z_DEFLATED, 15, 7, Z_DEFAULT_STRATEGY);
	if (ret != Z_OK) {
		return(DB_FAIL);
	}

	return(DB_SUCCESS);
}

/** Write all the BLOBs of the clustered index record.
@return DB_SUCCESS on success, error code on failure. */
dberr_t
zInserter::write()
{
	/* Loop through each blob field of the record and write one blob
	at a time.*/
	for (ulint i = 0;
	     i < m_ctx->get_big_rec_vec_size() && m_status == DB_SUCCESS;
	     i++) {

		ut_d(m_dir.clear(););
		m_status = write_one_blob(i);
	}

	return(m_status);
}

/** Make the current page as next page of previous page.  In other
words, make the page m_cur_blob_page_no as the next page
(FIL_PAGE_NEXT) of page m_prev_page_no.
@return DB_SUCCESS on success, or error code on failure. */
dberr_t
zInserter::set_page_next()
{
	buf_block_t*	prev_block = get_previous_blob_block();
	page_t*	prev_page = buf_block_get_frame(prev_block);

	mlog_write_ulint(prev_page + FIL_PAGE_NEXT, m_cur_blob_page_no,
			 MLOG_4BYTES, &m_blob_mtr);

	memcpy(buf_block_get_page_zip(prev_block)->data + FIL_PAGE_NEXT,
	       prev_page + FIL_PAGE_NEXT, 4);

	return(m_status);
}
#endif /* UNIV_DEBUG */

/** Get the previous BLOB page frame.  This will return a BLOB page.
It should not be called for the first BLOB page, because it will not
have a previous BLOB page.
@return	the previous BLOB page frame. */
page_t*
BaseInserter::get_previous_blob_page()
{
	ut_ad(m_prev_page_no != m_ctx->get_page_no());

	space_id_t	space_id	= m_ctx->space();
	buf_block_t*	rec_block	= m_ctx->block();

	buf_block_t*	prev_block = buf_page_get(
		page_id_t(space_id, m_prev_page_no),
		rec_block->page.size,
		RW_X_LATCH, &m_blob_mtr);

	buf_block_dbg_add_level(prev_block, SYNC_EXTERN_STORAGE);

	return(buf_block_get_frame(prev_block));
}

/** Get the previous BLOB page block.  This will return a BLOB block.
It should not be called for the first BLOB page, because it will not
have a previous BLOB page.
@return	the previous BLOB block. */
buf_block_t*
BaseInserter::get_previous_blob_block()
{
	DBUG_ENTER("BaseInserter::get_previous_blob_block");

	DBUG_LOG("lob", "m_prev_page_no=" << m_prev_page_no);
	ut_ad(m_prev_page_no != m_ctx->get_page_no());

	space_id_t	space_id	= m_ctx->space();
	buf_block_t*	rec_block	= m_ctx->block();

	buf_block_t*	prev_block = buf_page_get(
		page_id_t(space_id, m_prev_page_no),
		rec_block->page.size,
		RW_X_LATCH, &m_blob_mtr);

	buf_block_dbg_add_level(prev_block, SYNC_EXTERN_STORAGE);

	DBUG_RETURN(prev_block);
}

/** Print this blob directory into the given output stream.
@param[in]	out	the output stream.
@return the output stream. */
std::ostream&
blob_dir_t::print(std::ostream& out) const
{
	out << "[blob_dir_t: ";
	for (const blob_page_info_t& info : m_pages) {
		out << info;
	}
	out << "]";
	return(out);
}

/** Print this blob_page_into_t object into the given output stream.
@param[in]	out	the output stream.
@return the output stream. */
std::ostream&
blob_page_info_t::print(std::ostream& out) const
{
	out << "[blob_page_info_t: m_page_no=" << m_page_no << ", m_bytes="
		<< m_bytes << ", m_zbytes=" << m_zbytes << "]";
	return(out);
}

/** Do setup of the zlib stream.
@return code returned by zlib. */
int
zReader::setup_zstream()
{
	const ulint local_prefix = m_rctx.m_local_len
		- BTR_EXTERN_FIELD_REF_SIZE;

	m_stream.next_out = m_rctx.m_buf + local_prefix;
	m_stream.avail_out = static_cast<uInt>(m_rctx.m_len - local_prefix);
	m_stream.next_in = Z_NULL;
	m_stream.avail_in = 0;

	/* Zlib inflate needs 32 kilobytes for the default
	window size, plus a few kilobytes for small objects. */
	m_heap = mem_heap_create(40000);
	page_zip_set_alloc(&m_stream, m_heap);

	int err = inflateInit(&m_stream);
	return(err);
}

/** Fetch the BLOB.
@return DB_SUCCESS on success, DB_FAIL on error. */
dberr_t
zReader::fetch()
{
	DBUG_ENTER("zReader::fetch");

	dberr_t	err = DB_SUCCESS;

	ut_ad(m_rctx.is_valid_blob());
	ut_ad(assert_empty_local_prefix());

	ut_d(m_page_type_ex = m_rctx.is_sdi() ? FIL_PAGE_SDI_ZBLOB : FIL_PAGE_TYPE_ZBLOB);

	setup_zstream();

	m_remaining = m_rctx.m_blobref.length();

	while (m_rctx.m_page_no != FIL_NULL) {

		page_no_t	curr_page_no = m_rctx.m_page_no;

		err = fetch_page();
		if (err != DB_SUCCESS) {
			break;
		}

		m_stream.next_in = m_bpage->zip.data + m_rctx.m_offset;
		m_stream.avail_in = static_cast<uInt>(m_rctx.m_page_size.physical()
						      - m_rctx.m_offset);

		int zlib_err = inflate(&m_stream, Z_NO_FLUSH);
		switch (zlib_err) {
		case Z_OK:
			if (m_stream.avail_out == 0) {
				goto end_of_blob;
			}
			break;
		case Z_STREAM_END:
			if (m_rctx.m_page_no == FIL_NULL) {
				goto end_of_blob;
			}
			/* fall through */
		default:
			err = DB_FAIL;
			ib::error() << "inflate() of compressed BLOB page "
				<< page_id_t(m_rctx.m_space_id, curr_page_no)
				<< " returned " << zlib_err
				<< " (" << m_stream.msg << ")";
			/* fall through */
			ut_error;
		case Z_BUF_ERROR:
			goto end_of_blob;
		}

		buf_page_release_zip(m_bpage);

		m_rctx.m_offset = FIL_PAGE_NEXT;

		ut_d(if (!m_rctx.m_is_sdi) m_page_type_ex = FIL_PAGE_TYPE_ZBLOB2);
	}

end_of_blob:
	buf_page_release_zip(m_bpage);
	inflateEnd(&m_stream);
	mem_heap_free(m_heap);
	UNIV_MEM_ASSERT_RW(m_rctx.m_buf, m_stream.total_out);
	DBUG_RETURN(err);
}

#ifdef UNIV_DEBUG
/** Assert that the local prefix is empty.  For compressed row format,
there is no local prefix stored.  This function doesn't return if the
local prefix is non-empty.
@return true if local prefix is empty*/
bool
zReader::assert_empty_local_prefix()
{
	ut_ad(m_rctx.m_local_len == BTR_EXTERN_FIELD_REF_SIZE);
	return(true);
}

/** Assert that the local prefix is empty.  For compressed row format,
there is no local prefix stored.  This function doesn't return if the
local prefix is non-empty.
@return true if local prefix is empty*/
bool
CompressedReader::assert_empty_local_prefix()
{
	ut_ad(m_rctx.m_local_len == BTR_EXTERN_FIELD_REF_SIZE);
	return(true);
}
#endif /* UNIV_DEBUG */

dberr_t
zReader::fetch_page()
{
	dberr_t	err(DB_SUCCESS);

	m_bpage = buf_page_get_zip(
		page_id_t(m_rctx.m_space_id, m_rctx.m_page_no),
		m_rctx.m_page_size);

	ut_a(m_bpage != NULL);
	ut_ad(fil_page_get_type(m_bpage->zip.data) == m_page_type_ex);
	m_rctx.m_page_no = mach_read_from_4(m_bpage->zip.data + FIL_PAGE_NEXT);

	if (m_rctx.m_offset == FIL_PAGE_NEXT) {
		/* When the BLOB begins at page header,
		the compressed data payload does not
		immediately follow the next page pointer. */
		m_rctx.m_offset = FIL_PAGE_DATA;
	} else {
		m_rctx.m_offset += 4;
	}

	return(err);
}

/** Stores the fields in big_rec_vec to the tablespace and puts pointers to
them in rec.  The extern flags in rec will have to be set beforehand. The
fields are stored on pages allocated from leaf node file segment of the index
tree.

TODO: If the allocation extends the tablespace, it will not be redo logged, in
any mini-transaction.  Tablespace extension should be redo-logged, so that
recovery will not fail when the big_rec was written to the extended portion of
the file, in case the file was somehow truncated in the crash.

@param[in,out]	pcur		a persistent cursor. if btr_mtr is restarted,
				then this can be repositioned.
@param[in]	upd		update vector
@param[in,out]	offsets		rec_get_offsets() on pcur. the "external in
				offsets will correctly correspond storage"
				flagsin offsets will correctly correspond to
				rec when this function returns
@param[in]	big_rec_vec	vector containing fields to be stored
				externally
@param[in,out]	btr_mtr		mtr containing the latches to the clustered
				index. can be committed and restarted.
@param[in]	op		operation code
@return DB_SUCCESS or DB_OUT_OF_FILE_SPACE */
dberr_t
btr_store_big_rec_extern_fields(
	btr_pcur_t*		pcur,
	const upd_t*		upd,
	ulint*			offsets,
	const big_rec_t*	big_rec_vec,
	mtr_t*			btr_mtr,
	opcode			op)
{
	mtr_t		mtr;
	mtr_t		mtr_bulk;
	page_zip_des_t*	page_zip;
	dberr_t		error		= DB_SUCCESS;
	dict_index_t*	index		= pcur->index();
	buf_block_t*	rec_block	= btr_pcur_get_block(pcur);
	rec_t*		rec		= btr_pcur_get_rec(pcur);

	ut_ad(rec_offs_validate(rec, index, offsets));
	ut_ad(rec_offs_any_extern(offsets));
	ut_ad(btr_mtr);
	ut_ad(mtr_memo_contains_flagged(btr_mtr, dict_index_get_lock(index),
					MTR_MEMO_X_LOCK
					| MTR_MEMO_SX_LOCK)
	      || index->table->is_intrinsic());
	ut_ad(mtr_is_block_fix(
		btr_mtr, rec_block, MTR_MEMO_PAGE_X_FIX, index->table));
	ut_ad(buf_block_get_frame(rec_block) == page_align(rec));
	ut_a(index->is_clustered());

	ut_a(dict_table_page_size(index->table)
		.equals_to(rec_block->page.size));

	/* Create a blob operation context. */
	BtrContext btr_ctx(btr_mtr, pcur, index, rec, offsets, rec_block, op);
	InsertContext ctx(btr_ctx, big_rec_vec);

	page_zip = buf_block_get_page_zip(rec_block);
	ut_a(fil_page_index_page_check(page_align(rec))
	     || op == OPCODE_INSERT_BULK);

#if defined UNIV_DEBUG || defined UNIV_BLOB_LIGHT_DEBUG
	/* All pointers to externally stored columns in the record
	must either be zero or they must be pointers to inherited
	columns, owned by this record or an earlier record version. */
	for (uint i = 0; i < big_rec_vec->n_fields; i++) {
		byte*	field_ref = btr_rec_get_field_ref(
			rec, offsets, big_rec_vec->fields[i].field_no);

		ref_t	blobref(field_ref);

		ut_a(blobref.is_owner());
		/* Either this must be an update in place,
		or the BLOB must be inherited, or the BLOB pointer
		must be zero (will be written in this function). */
		ut_a(op == OPCODE_UPDATE
		     || blobref.is_inherited()
		     || blobref.is_null());
	}
#endif /* UNIV_DEBUG || UNIV_BLOB_LIGHT_DEBUG */

	/* Refactored compressed BLOB code */
	if (page_zip != NULL) {

		DBUG_EXECUTE_IF("lob_insert_single_zstream",
				{goto insert_single_zstream;});

		CompressedInserter	zblob_writer(&ctx);
		error = zblob_writer.prepare();
		if (error == DB_SUCCESS) {
			zblob_writer.write();
			error = zblob_writer.finish();
		}
	} else {

		Inserter	blob_writer(&ctx);
		error = blob_writer.write();
	}
	return(error);
#ifdef UNIV_DEBUG
	{
insert_single_zstream:
		/* Insert the LOB as a single zlib stream spanning multiple
		LOB pages.  This is the old way of storing LOBs. */
		zInserter	zblob_writer(&ctx);
		error = zblob_writer.prepare();
		if (error == DB_SUCCESS) {
			zblob_writer.write();
			error = zblob_writer.finish();
		}
		return(error);
	}
#endif /* UNIV_DEBUG */
}

/** Copies an externally stored field of a record to mem heap.
@param[in]	rec		record in a clustered index; must be
				protected by a lock or a page latch
@param[in]	offsets		array returned by rec_get_offsets()
@param[in]	page_size	BLOB page size
@param[in]	no		field number
@param[out]	len		length of the field
@param[in]	is_sdi		true for SDI Indexes
@param[in,out]	heap		mem heap
@return the field copied to heap, or NULL if the field is incomplete */
byte*
btr_rec_copy_externally_stored_field_func(
	const rec_t*		rec,
	const ulint*		offsets,
	const page_size_t&	page_size,
	ulint			no,
	ulint*			len,
#ifdef UNIV_DEBUG
	bool			is_sdi,
#endif /* UNIV_DEBUG */
	mem_heap_t*		heap)
{
	ulint		local_len;
	const byte*	data;

	ut_a(rec_offs_nth_extern(offsets, no));

	/* An externally stored field can contain some initial
	data from the field, and in the last 20 bytes it has the
	space id, page number, and offset where the rest of the
	field data is stored, and the data length in addition to
	the data stored locally. We may need to store some data
	locally to get the local record length above the 128 byte
	limit so that field offsets are stored in two bytes, and
	the extern bit is available in those two bytes. */

	data = rec_get_nth_field(rec, offsets, no, &local_len);

	ut_a(local_len >= BTR_EXTERN_FIELD_REF_SIZE);

	if (UNIV_UNLIKELY
	    (!memcmp(data + local_len - BTR_EXTERN_FIELD_REF_SIZE,
		     field_ref_zero, BTR_EXTERN_FIELD_REF_SIZE))) {
		/* The externally stored field was not written yet.
		This record should only be seen by
		trx_rollback_or_clean_all_recovered() or any
		TRX_ISO_READ_UNCOMMITTED transactions. */

		return(NULL);
	}

	return(btr_copy_externally_stored_field(
			len, data, page_size, local_len, is_sdi, heap));
}

/** Write all the BLOBs of the clustered index record.
@return DB_SUCCESS on success, error code on failure. */
dberr_t
Inserter::write()
{
	/* Loop through each blob field of the record and write one blob
	at a time. */
	for (ulint i = 0;
	     i < m_ctx->get_big_rec_vec_size() && m_status == DB_SUCCESS;
	     i++) {

		ut_d(m_dir.clear(););
		m_status = write_one_blob(i);

		DBUG_EXECUTE_IF("btr_store_big_rec_extern",
				m_status = DB_OUT_OF_FILE_SPACE;);
	}

	ut_ad(m_status != DB_SUCCESS || m_ctx->are_all_blobrefs_valid());

	return(m_status);
}

/** Write one blob field data.
@param[in]	blob_j	the blob field number
@return DB_SUCCESS on success, error code on failure. */
dberr_t
Inserter::write_one_blob(size_t blob_j)
{
	const big_rec_t*	vec = m_ctx->get_big_rec_vec();
	big_rec_field_t&	field = vec->fields[blob_j];

	m_ctx->check_redolog();

	m_status = write_first_page(blob_j, field);

	for (ulint nth_blob_page = 1;
	     is_ok() && m_remaining > 0; ++nth_blob_page) {

		const ulint	commit_freq = 4;

		if (nth_blob_page % commit_freq == 0) {
			m_ctx->check_redolog();
		}

		m_status = write_single_blob_page(blob_j, field, nth_blob_page);
	}

	m_ctx->make_nth_extern(field.field_no);

	ut_ad(m_remaining == 0);

	return(m_status);
}

/** Make the current page as next page of previous page.  In other
words, make the page m_cur_blob_page_no as the next page of page
m_prev_page_no. */
void
Inserter::set_page_next()
{
	page_t*	prev_page = get_previous_blob_page();

	mlog_write_ulint(
		prev_page + FIL_PAGE_DATA + LOB_HDR_NEXT_PAGE_NO,
		m_cur_blob_page_no, MLOG_4BYTES, &m_blob_mtr);
}

/** Write first blob page.
@param[in]	blob_j		the jth blob object of the record.
@param[in]	field		the big record field.
@return DB_SUCCESS on success. */
dberr_t
Inserter::write_first_page(
	size_t			blob_j,
	big_rec_field_t&	field)
{
	buf_block_t*	rec_block	= m_ctx->block();
	mtr_t*		mtr		= start_blob_mtr();

	buf_page_get(rec_block->page.id,
		     rec_block->page.size, RW_X_LATCH, mtr);

	alloc_blob_page();

	if (dict_index_is_online_ddl(m_ctx->index())) {
		row_log_table_blob_alloc(m_ctx->index(),
					 m_cur_blob_page_no);
	}

	log_page_type();

	m_remaining = field.len;
	write_into_single_page(field);

	const ulint	field_no = field.field_no;
	byte*	field_ref = btr_rec_get_field_ref(
		m_ctx->rec(), m_ctx->get_offsets(), field_no);
	ref_t	blobref(field_ref);

	blobref.set_length(field.len - m_remaining, mtr);
	blobref.update(m_ctx->space(), m_cur_blob_page_no,
		       FIL_PAGE_DATA, mtr);

	m_prev_page_no = m_cur_blob_page_no;

	mtr->commit();

	return(m_status);
}

/** Write one blob page.  This function will be repeatedly called
with an increasing nth_blob_page to completely write a BLOB.
@param[in]	blob_j		the jth blob object in big fields vector.
@param[in]	field		the big record field.
@param[in]	nth_blob_page	count of the BLOB page (starting from 1).
@return DB_SUCCESS or DB_FAIL. */
dberr_t
Inserter::write_single_blob_page(
	size_t			blob_j,
	big_rec_field_t&	field,
	ulint			nth_blob_page)
{
	buf_block_t*	rec_block	= m_ctx->block();
	mtr_t*		mtr		= start_blob_mtr();
	ut_a(nth_blob_page > 0);

	buf_page_get(rec_block->page.id,
		     rec_block->page.size, RW_X_LATCH, mtr);

	alloc_blob_page();
	set_page_next();
	log_page_type();
	write_into_single_page(field);
	const ulint	field_no = field.field_no;
	byte*	field_ref = btr_rec_get_field_ref(
		m_ctx->rec(), m_ctx->get_offsets(), field_no);
	ref_t	blobref(field_ref);
	blobref.set_length(field.len - m_remaining, mtr);
	m_prev_page_no = m_cur_blob_page_no;
	mtr->commit();

	return(m_status);
}

/** Write contents into a single BLOB page.
@param[in]	field		the big record field. */
void
Inserter::write_into_single_page(big_rec_field_t&	field)
{
	const ulint	payload_size = payload();
	const ulint	store_len
		= (m_remaining > payload_size) ? payload_size : m_remaining;

	page_t*	page = buf_block_get_frame(m_cur_blob_block);

	mlog_write_string(page + FIL_PAGE_DATA + LOB_HDR_SIZE,
			  (const byte*) field.data + field.len - m_remaining,
			  store_len, &m_blob_mtr);

	mlog_write_ulint(page + FIL_PAGE_DATA + LOB_HDR_PART_LEN,
			 store_len, MLOG_4BYTES, &m_blob_mtr);

	mlog_write_ulint(page + FIL_PAGE_DATA + LOB_HDR_NEXT_PAGE_NO,
			 FIL_NULL, MLOG_4BYTES, &m_blob_mtr);

	m_remaining -= store_len;
}

/** Returns the page number where the next BLOB part is stored.
@param[in]	blob_header	the BLOB header.
@return page number or FIL_NULL if no more pages */
static
inline
page_no_t
btr_blob_get_next_page_no(const byte* blob_header)
{
	return(mach_read_from_4(blob_header + LOB_HDR_NEXT_PAGE_NO));
}

/* Obtain an x-latch on the clustered index record page.*/
void Deleter::x_latch_rec_page()
{
	bool		found;
	page_t*		rec_page = m_ctx.m_blobref.page_align();
	page_no_t	rec_page_no = page_get_page_no(rec_page);
	space_id_t	rec_space_id = page_get_space_id(rec_page);

	const page_size_t& rec_page_size = fil_space_get_page_size(
			rec_space_id, &found);
	ut_ad(found);

	buf_page_get(
		page_id_t(rec_space_id, rec_page_no),
		rec_page_size, RW_X_LATCH, &m_mtr);
}

/** Free the first page of the BLOB and update the BLOB reference
in the clustered index.
@return DB_SUCCESS on pass, error code on failure. */
dberr_t	Deleter::free_first_page()
{
	dberr_t	err(DB_SUCCESS);
	page_no_t	next_page_no;

	mtr_start(&m_mtr);

	m_mtr.set_log_mode(m_ctx.m_mtr->get_log_mode());

	ut_ad(m_ctx.m_pcur == nullptr
	      || !m_ctx.table()->is_temporary()
	      || m_ctx.m_mtr->get_log_mode() == MTR_LOG_NO_REDO);

	page_no_t	page_no = m_ctx.m_blobref.page_no();
	space_id_t	space_id = m_ctx.m_blobref.space_id();

	x_latch_rec_page();

	buf_block_t*	blob_block = buf_page_get(
		page_id_t(space_id, page_no),
		m_ctx.m_page_size,
		RW_X_LATCH, &m_mtr);

	buf_block_dbg_add_level(blob_block, SYNC_EXTERN_STORAGE);
	page_t*	page = buf_block_get_frame(blob_block);

	ut_a(validate_page_type(page));

	if (m_ctx.is_compressed()) {
		next_page_no = mach_read_from_4(page + FIL_PAGE_NEXT);
	} else {
		next_page_no = btr_blob_get_next_page_no(
			page + FIL_PAGE_DATA);
	}

	btr_page_free_low(m_ctx.m_index, blob_block, ULINT_UNDEFINED,
			  &m_mtr);

	if (m_ctx.is_compressed() && m_ctx.get_page_zip() != nullptr) {
		m_ctx.m_blobref.set_page_no(next_page_no, nullptr);
		m_ctx.m_blobref.set_length(0, nullptr);
		page_zip_write_blob_ptr(
			m_ctx.get_page_zip(), m_ctx.m_rec, m_ctx.m_index,
			m_ctx.m_offsets, m_ctx.m_field_no, &m_mtr);
	} else {
		m_ctx.m_blobref.set_page_no(next_page_no, &m_mtr);
		m_ctx.m_blobref.set_length(0, &m_mtr);
	}

	/* Commit mtr and release the BLOB block to save memory. */
	blob_free(m_ctx.m_index, blob_block, TRUE, &m_mtr);

	return(err);
}

/** Free the LOB object.
@return DB_SUCCESS on success. */
dberr_t	Deleter::destroy()
{
	dberr_t	err(DB_SUCCESS);

	if (!can_free()) {
		return(DB_SUCCESS);
	}

	if (dict_index_is_online_ddl(m_ctx.index())) {
		row_log_table_blob_free(m_ctx.index(),
					m_ctx.m_blobref.page_no());
	}

	while (m_ctx.m_blobref.page_no() != FIL_NULL) {
		ut_ad(m_ctx.m_blobref.page_no() > 0);

		err = free_first_page();
		if (err != DB_SUCCESS) {
			break;
		}
	}

	return(err);
}

/** Check if the BLOB can be freed.
@return true if the BLOB can be freed, false otherwise. */
bool
Deleter::can_free() const
{
	if (m_ctx.m_blobref.is_null()) {
		/* In the rollback, we may encounter a clustered index
		record with some unwritten off-page columns. There is
		nothing to free then. */
		ut_a(m_ctx.m_rollback);
		return(false);
	}

	if (!m_ctx.m_blobref.is_owner()
	    || m_ctx.m_blobref.page_no() == FIL_NULL
	    || (m_ctx.m_rollback && m_ctx.m_blobref.is_inherited())) {
		return(false);
	}

	return(true);
}

/** Check the FIL_PAGE_TYPE on an uncompressed BLOB page.
@param[in]	space_id	space identifier.
@param[in]	page_no		page number.
@param[in]	page		the page
@param[in]	read		TRUE=read, FALSE=purge */
static
void
btr_check_blob_fil_page_type(
	space_id_t	space_id,
	page_no_t	page_no,
	const page_t*	page,
	ibool		read)
{
	ulint	type = fil_page_get_type(page);

	ut_a(space_id == page_get_space_id(page));
	ut_a(page_no == page_get_page_no(page));

	switch (type) {
		ulint	flags;
	case FIL_PAGE_TYPE_BLOB:
	case FIL_PAGE_SDI_BLOB:
		break;

	default:
		flags = fil_space_get_flags(space_id);
#ifndef UNIV_DEBUG /* Improve debug test coverage */
		if (!DICT_TF_HAS_ATOMIC_BLOBS(flags)) {
			/* Old versions of InnoDB did not initialize
			FIL_PAGE_TYPE on BLOB pages.  Do not print
			anything about the type mismatch when reading
			a BLOB page that may be from old versions. */
			return;
		}
#endif /* !UNIV_DEBUG */

		ib::fatal() << "FIL_PAGE_TYPE=" << type
			<< " on BLOB " << (read ? "read" : "purge")
			<< " space " << space_id << " page " << page_no
			<< " flags " << flags;
	}
}

/** Returns the length of a BLOB part stored on the header page.
@param[in]	blob_header	the BLOB header.
@return part length */
static
inline
ulint
btr_blob_get_part_len(const byte* blob_header)
{
	return(mach_read_from_4(blob_header + LOB_HDR_PART_LEN));
}

/** Fetch one BLOB page. */
void Reader::fetch_page()
{
	mtr_t	mtr;

	/* Bytes of LOB data available in the current LOB page. */
	ulint	part_len;

	/* Bytes of LOB data obtained from the current LOB page. */
	ulint	copy_len;

	ut_ad(m_rctx.m_page_no != FIL_NULL);
	ut_ad(m_rctx.m_page_no > 0);

	mtr_start(&mtr);

	m_cur_block = buf_page_get(
		page_id_t(m_rctx.m_space_id, m_rctx.m_page_no),
		m_rctx.m_page_size, RW_S_LATCH, &mtr);
	buf_block_dbg_add_level(m_cur_block, SYNC_EXTERN_STORAGE);
	page_t*	page = buf_block_get_frame(m_cur_block);

	btr_check_blob_fil_page_type(m_rctx.m_space_id, m_rctx.m_page_no,
				     page, TRUE);

	byte*	blob_header = page + m_rctx.m_offset;
	part_len = btr_blob_get_part_len(blob_header);
	copy_len = ut_min(part_len, m_rctx.m_len - m_copied_len);

	memcpy(m_rctx.m_buf + m_copied_len, blob_header + LOB_HDR_SIZE,
	       copy_len);

	m_copied_len += copy_len;
	m_rctx.m_page_no = btr_blob_get_next_page_no(blob_header);
	mtr_commit(&mtr);
	m_rctx.m_offset = FIL_PAGE_DATA;
}

/** Fetch the complete or prefix of the uncompressed LOB data.
@return bytes of LOB data fetched. */
ulint	Reader::fetch()
{
	if (m_rctx.m_blobref.is_null()) {
		ut_ad(m_copied_len == 0);
		return(m_copied_len);
	}

	while (m_copied_len < m_rctx.m_len)
	{
		if (m_rctx.m_page_no == FIL_NULL) {
			/* End of LOB has been reached. */
			break;
		}

		fetch_page();
	}

	/* Assure that we have fetched the requested amount or the LOB
	has ended. */
	ut_ad(m_copied_len == m_rctx.m_len
	      || m_rctx.m_page_no == FIL_NULL);

	return(m_copied_len);
}

/** Copies the prefix of an externally stored field of a record.
The clustered index record must be protected by a lock or a page latch.
@param[out]	buf		the field, or a prefix of it
@param[in]	len		length of buf, in bytes
@param[in]	page_size	BLOB page size
@param[in]	data		'internally' stored part of the field
containing also the reference to the external part; must be protected by
a lock or a page latch
@param[in]	is_sdi		true for SDI indexes
@param[in]	local_len	length of data, in bytes
@return the length of the copied field, or 0 if the column was being
or has been deleted */
ulint
btr_copy_externally_stored_field_prefix_func(
	byte*			buf,
	ulint			len,
	const page_size_t&	page_size,
	const byte*		data,
#ifdef UNIV_DEBUG
	bool			is_sdi,
#endif /* UNIV_DEBUG */
	ulint			local_len)
{
	ut_a(local_len >= BTR_EXTERN_FIELD_REF_SIZE);

	if (page_size.is_compressed()) {
		ut_a(local_len == BTR_EXTERN_FIELD_REF_SIZE);
		ReadContext	rctx(page_size, data, local_len,
				     buf, len
#ifdef UNIV_DEBUG
				     , is_sdi
#endif /* UNIV_DEBUG */
				     );
		if (!rctx.is_valid_blob()) {
			return(0);
		}
		CompressedReader	reader(rctx);
		reader.fetch();
		return(reader.length());
	}

	local_len -= BTR_EXTERN_FIELD_REF_SIZE;

	if (UNIV_UNLIKELY(local_len >= len)) {
		memcpy(buf, data, len);
		return(len);
	}

	memcpy(buf, data, local_len);
	data += local_len;

	ut_a(memcmp(data, field_ref_zero, BTR_EXTERN_FIELD_REF_SIZE));

	if (!mach_read_from_4(data + BTR_EXTERN_LEN + 4)) {
		/* The externally stored part of the column has been
		(partially) deleted.  Signal the half-deleted BLOB
		to the caller. */

		return(0);
	}

	ReadContext	rctx(
		page_size, data, local_len + BTR_EXTERN_FIELD_REF_SIZE,
		buf + local_len, len
#ifdef UNIV_DEBUG
		, false
#endif /* UNIV_DEBUG */
		);

	Reader	reader(rctx);
	ulint fetch_len = reader.fetch();
	return(local_len + fetch_len);
}

/** Copies an externally stored field of a record to mem heap.
The clustered index record must be protected by a lock or a page latch.
@param[out]	len		length of the whole field
@param[in]	data		'internally' stored part of the field
containing also the reference to the external part; must be protected by
a lock or a page latch
@param[in]	page_size	BLOB page size
@param[in]	local_len	length of data
@param[in]	is_sdi		true for SDI Indexes
@param[in,out]	heap		mem heap
@return the whole field copied to heap */
byte*
btr_copy_externally_stored_field_func(
	ulint*			len,
	const byte*		data,
	const page_size_t&	page_size,
	ulint			local_len,
#ifdef UNIV_DEBUG
	bool			is_sdi,
#endif /* UNIV_DEBUG */
	mem_heap_t*		heap)
{
	uint32_t	extern_len;
	byte*		buf;

	ut_a(local_len >= BTR_EXTERN_FIELD_REF_SIZE);

	local_len -= BTR_EXTERN_FIELD_REF_SIZE;

	/* Currently a BLOB cannot be bigger than 4 GB; we
	leave the 4 upper bytes in the length field unused */

	extern_len = mach_read_from_4(data + local_len + BTR_EXTERN_LEN + 4);

	buf = (byte*) mem_heap_alloc(heap, local_len + extern_len);

	ReadContext	rctx(page_size, data,
			     local_len + BTR_EXTERN_FIELD_REF_SIZE,
			     buf + local_len, extern_len
#ifdef UNIV_DEBUG
			     , is_sdi
#endif /* UNIV_DEBUG */
			     );

	if (page_size.is_compressed()) {
		ut_ad(local_len == 0);
		CompressedReader	reader(rctx);
		reader.fetch();
		*len = reader.length();
		return(buf);
	} else {
		memcpy(buf, data, local_len);
		Reader	reader(rctx);
		ulint fetch_len = reader.fetch();
		*len = local_len + fetch_len;
		return(buf);
	}

	ut_error;
}

/** Frees the externally stored fields for a record, if the field
is mentioned in the update vector.
@param[in]	update		update vector
@param[in]	rollback	performing rollback? */
void BtrContext::free_updated_extern_fields(
	const upd_t*	update,
	bool		rollback)
{
	ulint	n_fields;
	ulint	i;

	ut_ad(rec_offs_validate());
	ut_ad(mtr_is_page_fix(m_mtr, m_rec, MTR_MEMO_PAGE_X_FIX, m_index->table));

	/* Free possible externally stored fields in the record */

	n_fields = upd_get_n_fields(update);

	for (i = 0; i < n_fields; i++) {
		const upd_field_t* ufield = upd_get_nth_field(update, i);

		if (rec_offs_nth_extern(m_offsets, ufield->field_no)) {
			ulint	len;
			byte*	data = rec_get_nth_field(
				m_rec, m_offsets, ufield->field_no, &len);
			ut_a(len >= BTR_EXTERN_FIELD_REF_SIZE);

			byte*	field_ref = data + len
				- BTR_EXTERN_FIELD_REF_SIZE;

			DeleteContext ctx(*this, field_ref,
					  ufield->field_no, rollback);
			Deleter	free_blob(ctx);
			free_blob.destroy();
		}
	}
}

/** Deallocate a buffer block that was reserved for a BLOB part.
@param[in]	index	index
@param[in]	block	buffer block
@param[in]	all	flag whether remove the compressed page
			if there is one
@param[in]	mtr	mini-transaction to commit */
void
blob_free(
	dict_index_t*	index,
	buf_block_t*	block,
	bool		all,
	mtr_t*		mtr)
{
	buf_pool_t*	buf_pool = buf_pool_from_block(block);
	page_id_t	page_id(block->page.id.space(),
				block->page.id.page_no());
	bool	freed	= false;

	ut_ad(mtr_is_block_fix(mtr, block, MTR_MEMO_PAGE_X_FIX, index->table));

	mtr_commit(mtr);

	mutex_enter(&buf_pool->LRU_list_mutex);
	buf_page_mutex_enter(block);

	/* Only free the block if it is still allocated to
	the same file page. */

	if (buf_block_get_state(block) == BUF_BLOCK_FILE_PAGE
	    && page_id.equals_to(block->page.id)) {

		freed = buf_LRU_free_page(&block->page, all);

		if (!freed && all && block->page.zip.data
		    && buf_block_get_state(block) == BUF_BLOCK_FILE_PAGE
		    && page_id.equals_to(block->page.id)) {

			/* Attempt to deallocate the uncompressed page
			if the whole block cannot be deallocted. */

			freed = buf_LRU_free_page(&block->page, false);
		}
	}

	if (!freed) {
		mutex_exit(&buf_pool->LRU_list_mutex);
		buf_page_mutex_exit(block);
	}
}

/** Flags the data tuple fields that are marked as extern storage in the
update vector.  We use this function to remember which fields we must
mark as extern storage in a record inserted for an update.
@param[in,out]	tuple	data tuple
@param[in]	update	update vector
@param[in]	heap	memory heap
@return number of flagged external columns */
ulint
btr_push_update_extern_fields(
	dtuple_t*	tuple,
	const upd_t*	update,
	mem_heap_t*	heap)
{
	ulint			n_pushed	= 0;
	ulint			n;
	const upd_field_t*	uf;

	ut_ad(tuple);
	ut_ad(update);

	uf = update->fields;
	n = upd_get_n_fields(update);

	for (; n--; uf++) {
		if (dfield_is_ext(&uf->new_val)) {
			dfield_t*	field
				= dtuple_get_nth_field(tuple, uf->field_no);

			if (!dfield_is_ext(field)) {
				dfield_set_ext(field);
				n_pushed++;
			}

			switch (uf->orig_len) {
				byte*	data;
				ulint	len;
				byte*	buf;
			case 0:
				break;
			case BTR_EXTERN_FIELD_REF_SIZE:
				/* Restore the original locally stored
				part of the column.  In the undo log,
				InnoDB writes a longer prefix of externally
				stored columns, so that column prefixes
				in secondary indexes can be reconstructed. */
				dfield_set_data(field,
						(byte*) dfield_get_data(field)
						+ dfield_get_len(field)
						- BTR_EXTERN_FIELD_REF_SIZE,
						BTR_EXTERN_FIELD_REF_SIZE);
				dfield_set_ext(field);
				break;
			default:
				/* Reconstruct the original locally
				stored part of the column.  The data
				will have to be copied. */
				ut_a(uf->orig_len > BTR_EXTERN_FIELD_REF_SIZE);

				data = (byte*) dfield_get_data(field);
				len = dfield_get_len(field);

				buf = (byte*) mem_heap_alloc(heap,
							     uf->orig_len);
				/* Copy the locally stored prefix. */
				memcpy(buf, data,
				       uf->orig_len
				       - BTR_EXTERN_FIELD_REF_SIZE);
				/* Copy the BLOB pointer. */
				memcpy(buf + uf->orig_len
				       - BTR_EXTERN_FIELD_REF_SIZE,
				       data + len - BTR_EXTERN_FIELD_REF_SIZE,
				       BTR_EXTERN_FIELD_REF_SIZE);

				dfield_set_data(field, buf, uf->orig_len);
				dfield_set_ext(field);
			}
		}
	}

	return(n_pushed);
}

/** Gets the externally stored size of a record, in units of a database page.
@param[in]	rec	record
@param[in]	offsets	array returned by rec_get_offsets()
@return externally stored part, in units of a database page */
ulint
btr_rec_get_externally_stored_len(
	const rec_t*	rec,
	const ulint*	offsets)
{
	ulint	n_fields;
	ulint	total_extern_len = 0;
	ulint	i;

	ut_ad(!rec_offs_comp(offsets) || !rec_get_node_ptr_flag(rec));

	if (!rec_offs_any_extern(offsets)) {
		return(0);
	}

	n_fields = rec_offs_n_fields(offsets);

	for (i = 0; i < n_fields; i++) {
		if (rec_offs_nth_extern(offsets, i)) {

			ulint	extern_len = mach_read_from_4(
				btr_rec_get_field_ref(rec, offsets, i)
				+ BTR_EXTERN_LEN + 4);

			total_extern_len += ut_calc_align(extern_len,
							  UNIV_PAGE_SIZE);
		}
	}

	return(total_extern_len / UNIV_PAGE_SIZE);
}

/** Frees the externally stored fields for a record.
@param[in]	rollback	performing rollback? */
void BtrContext::free_externally_stored_fields(bool rollback)
{
	ut_ad(rec_offs_validate());
	ut_ad(mtr_is_page_fix(m_mtr, m_rec, MTR_MEMO_PAGE_X_FIX,
			      m_index->table));

	/* Free possible externally stored fields in the record */
	ut_ad(dict_table_is_comp(m_index->table)
	      == !!rec_offs_comp(m_offsets));
	ulint n_fields = rec_offs_n_fields(m_offsets);

	for (ulint i = 0; i < n_fields; i++) {
		if (rec_offs_nth_extern(m_offsets, i)) {
			byte*	field_ref = btr_rec_get_field_ref(
				m_rec, m_offsets, i);

			DeleteContext ctx(*this, field_ref,
					  i, rollback);

			Deleter	free_blob(ctx);
			free_blob.destroy();
		}
	}
}

/** Prepare to write a compressed BLOB. Setup the zlib
compression stream.
@return DB_SUCCESS on success, error code on failure. */
dberr_t CompressedInserter::prepare()
{
	int ret = m_fitblk.init(page_zip_level);

	if (ret != 0) {
		return(DB_FAIL);
	}

	return(DB_SUCCESS);
}

/** Write all the BLOBs of the clustered index record.
@return DB_SUCCESS on success, error code on failure. */
dberr_t
CompressedInserter::write()
{
	/* loop through each of the blob and write one blob at a time. */
	for (ulint i = 0;
	     i < m_ctx->get_big_rec_vec_size() && m_status == DB_SUCCESS;
	     i++) {

		ut_d(m_dir.clear(););
		m_status = write_one_blob(i);
	}

	return(m_status);
}


/** Write one blob field data.
@param[in]	blob_j	the blob field number
@return DB_SUCCESS on success, error code on failure. */
dberr_t
CompressedInserter::write_one_blob(size_t blob_j)
{
	const big_rec_t*	vec = m_ctx->get_big_rec_vec();
	big_rec_field_t&	field = vec->fields[blob_j];

	m_ctx->check_redolog();

	m_bytes_written = 0;

	m_fitblk.setInputBuffer(field.ptr(), static_cast<uint>(field.len));

	int err = write_first_page(blob_j, field);
	ut_a(err == Z_OK || err == Z_STREAM_END);

	ulint nth_blob_page;
	for (nth_blob_page = 1;
	     m_bytes_written < field.len; ++nth_blob_page) {

		const ulint	commit_freq = 4;

		err = write_single_blob_page(blob_j, field, nth_blob_page);
		ut_a(err == Z_OK || err == Z_STREAM_END);

		if (nth_blob_page % commit_freq == 0) {
			m_ctx->check_redolog();
		}
	}

	// std::cout << "nth_blob_page: " << nth_blob_page << std::endl;
	ut_a(m_bytes_written == field.len);
	m_ctx->make_nth_extern(field.field_no);
	return(DB_SUCCESS);
}

/** Write one blob page.  This function will be repeatedly called
with an increasing nth_blob_page to completely write a BLOB.
@param[in]	blob_j		the jth blob object of the record.
@param[in]	field		the big record field.
@param[in]	nth_blob_page	count of the BLOB page (starting from 1).
@return code as returned by the zlib. */
int
CompressedInserter::write_single_blob_page(
	size_t			blob_j,
	big_rec_field_t&	field,
	ulint			nth_blob_page)
{
	DBUG_ENTER("CompressedInserter::write_single_blob_page");

	ut_ad(nth_blob_page > 0);

	buf_block_t*	rec_block = m_ctx->block();
	mtr_t*		mtr = start_blob_mtr();

	buf_page_get(rec_block->page.id,
		     rec_block->page.size, RW_X_LATCH, mtr);

	buf_block_t*	blob_block = alloc_blob_page();
	page_t*		blob_page  = buf_block_get_frame(blob_block);

	set_page_next();

	m_prev_page_no = page_get_page_no(blob_page);

	log_page_type(blob_page);

	int err = write_into_single_page(field);

	ut_ad(!dict_index_is_spatial(m_ctx->index()));

	DBUG_LOG("lob", "m_bytes_written=" << m_bytes_written);
	DBUG_LOG("lob", "field.len=" << field.len);

	if (m_bytes_written == field.len) {
		update_length_in_blobref(field);
	}

	/* Commit mtr and release uncompressed page frame to save memory.*/
	blob_free(m_ctx->index(), m_cur_blob_block, FALSE, mtr);

	DBUG_RETURN(err);
}

/** Write contents into a single BLOB page.
@param[in]	field	the big record field that is begin written.
@return code as returned by zlib. */
int
CompressedInserter::write_into_single_page(
	big_rec_field_t&	field)
{
	DBUG_ENTER("CompressedInserter::write_into_single_page");
	mtr_t*	const	mtr = &m_blob_mtr;

	/* Space available in compressed page to carry blob data */
	const page_size_t page_size = m_ctx->page_size();

	page_t*	blob_page = buf_block_get_frame(m_cur_blob_block);
	byte*	out = blob_page + ZLOB_PAGE_DATA;
	uint	size = page_size.physical() - ZLOB_PAGE_DATA;

	m_fitblk.fit(out, size);

	m_bytes_written = m_fitblk.getInputBytes();

	const blob_page_info_t page_info(
		m_cur_blob_page_no,
		m_fitblk.getInputBytes(),
		m_fitblk.getOutputBytes());

#ifdef UNIV_DEBUG
	add_to_blob_dir(page_info);
#endif /* UNIV_DEBUG */

	/* Write the "next BLOB page" pointer */
	mlog_write_ulint(blob_page + FIL_PAGE_NEXT, FIL_NULL,
			 MLOG_4BYTES, mtr);

	/* Initialize the unused "prev page" pointer */
	mlog_write_ulint(blob_page + FIL_PAGE_PREV, FIL_NULL,
			 MLOG_4BYTES, mtr);

	/* Write a back pointer to the record into the otherwise unused area.
	This information could be useful in debugging.  Later, we might want
	to implement the possibility to relocate BLOB pages.  Then, we would
	need to be able to adjust the BLOB pointer in the record.  We do not
	store the heap number of the record, because it can change in
	page_zip_reorganize() or btr_page_reorganize().  However, also the
	page number of the record may change when B-tree nodes are split or
	merged. */
	mlog_write_ulint(blob_page + FIL_PAGE_FILE_FLUSH_LSN,
			 m_ctx->space(), MLOG_4BYTES, mtr);

	mlog_write_ulint(blob_page + FIL_PAGE_FILE_FLUSH_LSN + 4,
			 m_ctx->get_page_no(), MLOG_4BYTES, mtr);

	mlog_log_string(blob_page + FIL_PAGE_FILE_FLUSH_LSN,
			page_zip_get_size(m_ctx->get_page_zip())
			- FIL_PAGE_FILE_FLUSH_LSN, mtr);

	/* Copy the page to compressed storage, because it will be flushed
	to disk from there. */
	page_zip_des_t* blob_page_zip = buf_block_get_page_zip(m_cur_blob_block);

	ut_ad(blob_page_zip);
	ut_ad(page_zip_get_size(blob_page_zip)
	      == page_zip_get_size(m_ctx->get_page_zip()));

	page_zip_des_t* page_zip = buf_block_get_page_zip(m_ctx->block());
	memcpy(blob_page_zip->data, blob_page, page_zip_get_size(page_zip));

	DBUG_RETURN(Z_OK);
}

/** Write first blob page.
@param[in]	blob_j		the jth blob object of the record.
@param[in]	field		the big record field.
@return code as returned by the zlib. */
int
CompressedInserter::write_first_page(
	size_t			blob_j,
	big_rec_field_t&	field)
{
	DBUG_ENTER("CompressedInserter::write_first_page");

	buf_block_t*	rec_block = m_ctx->block();
	mtr_t*		mtr = start_blob_mtr();

	buf_page_get(rec_block->page.id,
		     rec_block->page.size, RW_X_LATCH, mtr);

	buf_block_t*	blob_block = alloc_blob_page();

	if (dict_index_is_online_ddl(m_ctx->index())) {
		row_log_table_blob_alloc(m_ctx->index(),
					 m_cur_blob_page_no);
	}

	page_t*	blob_page  = buf_block_get_frame(blob_block);

	log_page_type(blob_page);

	int	err = write_into_single_page(field);

	ut_ad(!dict_index_is_spatial(m_ctx->index()));

	const ulint	field_no = field.field_no;
	byte*	field_ref = btr_rec_get_field_ref(
		m_ctx->rec(), m_ctx->get_offsets(), field_no);
	ref_t	blobref(field_ref);

	if (m_bytes_written == field.len) {
		blobref.set_length(m_bytes_written, nullptr);
	} else {
		blobref.set_length(0, nullptr);
	}

	blobref.update(m_ctx->space(), m_cur_blob_page_no,
		       ZLOB_PAGE_DATA, NULL);

	/* After writing the first blob page, update the blob reference. */
	if (!m_ctx->is_bulk()) {
		m_ctx->zblob_write_blobref(field_no, &m_blob_mtr);
	}

	m_prev_page_no = page_get_page_no(blob_page);

	/* Commit mtr and release uncompressed page frame to save memory.*/
	blob_free(m_ctx->index(), m_cur_blob_block, FALSE, mtr);

	DBUG_LOG("CompressedInserter", "err=" << err);

	DBUG_RETURN(err);
}

/** Make the current page as next page of previous page.  In other
words, make the page m_cur_blob_page_no as the next page
(FIL_PAGE_NEXT) of page m_prev_page_no.
@return DB_SUCCESS on success, or error code on failure. */
dberr_t
CompressedInserter::set_page_next()
{
	buf_block_t*	prev_block = get_previous_blob_block();
	page_t*		prev_page = buf_block_get_frame(prev_block);

	mlog_write_ulint(prev_page + FIL_PAGE_NEXT, m_cur_blob_page_no,
			 MLOG_4BYTES, &m_blob_mtr);

	memcpy(buf_block_get_page_zip(prev_block)->data + FIL_PAGE_NEXT,
	       prev_page + FIL_PAGE_NEXT, 4);

	return(m_status);
}

/** For the given blob field, update its length in the blob reference
which is available in the clustered index record.
@param[in]	field	the concerned blob field. */
void
CompressedInserter::update_length_in_blobref(big_rec_field_t& field)
{
	DBUG_ENTER("CompressedInserter::update_length_in_blobref");

	/* After writing the last blob page, update the blob reference
	with the correct length. */

	const ulint	field_no = field.field_no;
	byte*	field_ref = btr_rec_get_field_ref(
		m_ctx->rec(), m_ctx->get_offsets(), field_no);

	ref_t	blobref(field_ref);
	blobref.set_length(m_bytes_written, nullptr);

	DBUG_LOG("lob", blobref);

	if (!m_ctx->is_bulk()) {
		m_ctx->zblob_write_blobref(field_no, &m_blob_mtr);
	}

	DBUG_VOID_RETURN;
}

/** Cleanup after completing the write of compressed BLOB.
@return DB_SUCCESS on success, error code on failure. */
dberr_t	CompressedInserter::finish()
{
	m_fitblk.destroy();
	return(m_status);
}

/** Fetch the BLOB.
@return DB_SUCCESS on success, DB_FAIL on error. */
dberr_t
CompressedReader::fetch()
{
	DBUG_ENTER("CompressedReader::fetch");

	dberr_t	err = DB_SUCCESS;
	ut_ad(assert_empty_local_prefix());

	ut_d(m_page_type_ex = m_rctx.is_sdi() ? FIL_PAGE_SDI_ZBLOB : FIL_PAGE_TYPE_ZBLOB3);

	m_remaining = length();

	if (m_remaining > m_rctx.m_blobref.length()) {
		m_remaining = m_rctx.m_blobref.length();
		set_length(m_remaining);
	}

	if (m_remaining == 0) {
		DBUG_RETURN(DB_SUCCESS);
	}

	if (is_single_zstream()) {
		zReader	reader(m_rctx);
		err = reader.fetch();
		ut_ad(length() == reader.length());
		DBUG_RETURN(err);
	}

	m_unfit.init();
	m_unfit.setOutput(m_rctx.m_buf, static_cast<uint>(m_rctx.m_len));

	while (m_unfit.m_total_out < m_rctx.m_len) {

		if (m_rctx.m_page_no == FIL_NULL) {
			break;
		}

		err = fetch_page();
		if (err != DB_SUCCESS) {
			break;
		}
		byte*	ptr = m_bpage->zip.data + ZLOB_PAGE_DATA;
		uint	payload = getPayloadSize();

		m_unfit.unfit(ptr, payload);

		buf_page_release_zip(m_bpage);
		ut_d(if (!m_rctx.m_is_sdi) m_page_type_ex = FIL_PAGE_TYPE_ZBLOB3);
	}

	UNIV_MEM_ASSERT_RW(m_rctx.m_buf, m_rctx.m_len);
	DBUG_RETURN(err);
}

/** Check if the LOB is stored as a single zlib stream.  In the older
approach, the LOB was stored as a single zlib stream.
@return true if stored as a single stream, false otherwise. */
bool
CompressedReader::is_single_zstream()
{
	bool	single = false;

	buf_page_t*	bpage = buf_page_get_zip(
		page_id_t(m_rctx.m_space_id, m_rctx.m_page_no),
		m_rctx.m_page_size);

	if (fil_page_get_type(bpage->zip.data) == FIL_PAGE_TYPE_ZBLOB) {
		/* The LOB is stored as a single zlib stream.  This is the
		old way of storing LOB.  Use zReader. */
		single = true;
	}

	buf_page_release_zip(bpage);

	return(single);
}

dberr_t
CompressedReader::fetch_page()
{
	dberr_t	err(DB_SUCCESS);

	m_bpage = buf_page_get_zip(
		page_id_t(m_rctx.m_space_id, m_rctx.m_page_no),
		m_rctx.m_page_size);

	ut_a(m_bpage != NULL);
	ut_ad(fil_page_get_type(m_bpage->zip.data) == m_page_type_ex);
	m_rctx.m_page_no = mach_read_from_4(m_bpage->zip.data + FIL_PAGE_NEXT);

	DBUG_LOG("lob", "Reading LOB from: " << (void *) m_bpage->zip.data);

	m_rctx.m_offset = ZLOB_PAGE_DATA;
	return(err);
}

} // namespace lob
