/*****************************************************************************

Copyright (c) 2015, 2016, Oracle and/or its affiliates. All Rights Reserved.

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

#include "btr0pcur.h"
#include "lob.h"
#include "fil0fil.h"

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
btr_blob_context_t::check_redolog_bulk()
{
	DBUG_ENTER("btr_blob_context_t::check_redolog_bulk");

	ut_ad(is_bulk());

	FlushObserver* observer = m_btr_mtr->get_flush_observer();

	rec_block_fix();

	commit_btr_mtr();

	DEBUG_SYNC_C("blob_write_middle");

	log_free_check();

	start_btr_mtr();
	m_btr_mtr->set_flush_observer(observer);

	rec_block_unfix();
	ut_ad(validate());

	DBUG_VOID_RETURN;
}

/** Allocate one BLOB page.
@return the allocated block of the BLOB page. */
buf_block_t*
zblob_writer_t::alloc_blob_page()
{
	ulint	r_extents;
	mtr_t	mtr_bulk;
	mtr_t*	alloc_mtr;

	DBUG_ENTER("zblob_writer_t::alloc_blob_page");

	ut_ad(fsp_check_tablespace_size(m_ctx->space()));

	if (m_ctx->is_bulk()) {
		mtr_start(&mtr_bulk);
		mtr_bulk.set_named_space(m_ctx->space());
		alloc_mtr = &mtr_bulk;
	} else {
		alloc_mtr = &m_blob_mtr;
	}

	page_no_t	hint_page_no = m_prev_page_no + 1;

	if (!fsp_reserve_free_extents(&r_extents, m_ctx->space(), 1,
		FSP_BLOB, alloc_mtr, 1)) {

		alloc_mtr->commit();
		m_status = DB_OUT_OF_FILE_SPACE;
		return(NULL);
	}

	m_cur_blob_block = btr_page_alloc(
		m_ctx->index(), hint_page_no, FSP_NO_DIR, 0,
		alloc_mtr, &m_blob_mtr);

	alloc_mtr->release_free_extents(r_extents);

	if (m_ctx->is_bulk()) {
		alloc_mtr->commit();
	}

	m_cur_blob_page_no = page_get_page_no(
		buf_block_get_frame(m_cur_blob_block));

	DBUG_LOG("blob", "Current blob page no: " << m_cur_blob_page_no);

	DBUG_RETURN(m_cur_blob_block);
}

/** Write first blob page.
@param[in]	blob_j		the jth blob object of the record.
@param[in]	field		the big record field.
@return code as returned by the zlib. */
int
zblob_writer_t::write_first_page(
	ulint			blob_j,
	big_rec_field_t&	field)
{
	DBUG_ENTER("zblob_writer_t::write_first_page");

	DBUG_LOG("blob", "blob_j: " << blob_j
		<< ", writing first page");

	buf_block_t*	rec_block	= m_ctx->rec_block();
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

	int	err = write_into_page();

	ut_ad(!dict_index_is_spatial(m_ctx->index()));

	DBUG_LOG("blob", "err: " << err);

	const ulint	field_no = field.field_no;
	byte*	field_ref = btr_rec_get_field_ref(m_ctx->rec(),
						m_ctx->rec_offsets(), field_no);
	blobref_t	blobref(field_ref);

	if (err == Z_OK) {
		blobref.set_length(0);
	} else if (err == Z_STREAM_END) {
		blobref.set_length(m_stream.total_in);
	} else {
		ut_ad(0);
		DBUG_RETURN(err);
	}

	blobref.update(m_ctx->space(), m_cur_blob_page_no, FIL_PAGE_NEXT);

	/* After writing the first blob page, update the blob reference. */
	if (!m_ctx->is_bulk()) {
		m_ctx->zblob_write_blobref(field_no, &m_blob_mtr);
	}

	m_prev_page_no = page_get_page_no(blob_page);
	DBUG_LOG("blob", "Updating prev page to: " << m_prev_page_no);

	/* Commit mtr and release uncompressed page frame to save memory.*/
	btr_blob_free(m_ctx->index(), m_cur_blob_block, FALSE, mtr);

	DBUG_RETURN(err);
}

/** For the given blob field, update its length in the blob reference
which is available in the clustered index record.
@param[in]	field	the concerned blob field. */
void
zblob_writer_t::update_length_in_blobref(big_rec_field_t& field)
{
	DBUG_ENTER("zblob_writer_t::update_length_in_blobref");

	/* After writing the last blob page, update the blob reference
	with the correct length. */

	const ulint	field_no = field.field_no;
	byte* field_ref = btr_rec_get_field_ref(
		m_ctx->rec(), m_ctx->rec_offsets(), field_no);

	blobref_t	blobref(field_ref);
	blobref.set_length(m_stream.total_in);

	DBUG_LOG("blob", "field_no: " << field_no
		<< ", total bytes written: " << m_stream.total_in);

	if (!m_ctx->is_bulk()) {
		m_ctx->zblob_write_blobref(field_no, &m_blob_mtr);
	}

	DBUG_LOG("blob", blobref);

	DBUG_VOID_RETURN;
}

/** Write one blob field data.
@param[in]	blob_j	the blob field number
@return DB_SUCCESS on success, error code on failure. */
dberr_t
zblob_writer_t::write_one_blob(int blob_j)
{
	DBUG_ENTER("zblob_writer_t::write_one_blob");
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

		err = write_single_blob_page(
			blob_j, field, static_cast<int>(nth_blob_page));

		if (nth_blob_page % commit_freq == 0) {
			m_ctx->check_redolog();
		}
	}

	ut_ad(err == Z_STREAM_END);

	DBUG_LOG("blob", "Mark external field number: " << field.field_no);

	m_ctx->make_nth_extern(field.field_no);
	DBUG_RETURN(DB_SUCCESS);
}

/** Write contents into a single BLOB page.
@return code as returned by zlib. */
int
zblob_writer_t::write_into_page()
{
	DBUG_ENTER("zblob_writer_t::write_into_page");

	const ulint in_before = m_stream.avail_in;

	mtr_t*	const	mtr = &m_blob_mtr;

	/* Space available in compressed page to carry blob data */
	const page_size_t page_size = m_ctx->page_size();
        const ulint payload_size_zip = page_size.physical() - FIL_PAGE_DATA;

	page_t*	blob_page = buf_block_get_frame(m_cur_blob_block);

	m_stream.next_out = blob_page + FIL_PAGE_DATA;
	m_stream.avail_out = static_cast<uInt>(payload_size_zip);

	DBUG_LOG("blob", "payload size: " << payload_size_zip);
	DBUG_LOG("blob", "in_before: " << in_before);
	DBUG_LOG("blob", "next_in: " << (void *) m_stream.next_in);
	DBUG_LOG("blob", "total_in: " << m_stream.total_in);

	int err = deflate(&m_stream, Z_FINISH);
	ut_a(err == Z_OK || err == Z_STREAM_END);
	ut_a(err == Z_STREAM_END || m_stream.avail_out == 0);

	DBUG_LOG("blob", "avail_in: " << m_stream.avail_in);
	DBUG_LOG("blob", "next_in: " << (void *) m_stream.next_in);
	DBUG_LOG("blob", "total_in: " << m_stream.total_in);

	const blob_page_info_t page_info(
		m_cur_blob_page_no,
		in_before - m_stream.avail_in,
		payload_size_zip - m_stream.avail_out);

#ifdef UNIV_DEBUG
	add_to_blob_dir(page_info);
#endif /* UNIV_DEBUG */

	/* Write the "next BLOB page" pointer */
	mlog_write_ulint(blob_page + FIL_PAGE_NEXT, FIL_NULL,
			 MLOG_4BYTES, mtr);

	/* Initialize the unused "prev page" pointer */
	mlog_write_ulint(blob_page + FIL_PAGE_PREV, FIL_NULL,
			 MLOG_4BYTES, mtr);

	mlog_write_ulint(blob_page + FIL_PAGE_FILE_FLUSH_LSN,
			 m_ctx->space(), MLOG_4BYTES, mtr);

	mlog_write_ulint(blob_page + FIL_PAGE_FILE_FLUSH_LSN + 4,
			 m_ctx->btr_page_no(), MLOG_4BYTES, mtr);

	if (m_stream.avail_out > 0) {
		/* Zero out the unused part of the page. */
		memset(blob_page + page_zip_get_size(m_ctx->get_page_zip())
		       - m_stream.avail_out, 0, m_stream.avail_out);
	}

	mlog_log_string(blob_page + FIL_PAGE_FILE_FLUSH_LSN,
			page_zip_get_size(m_ctx->get_page_zip())
			- FIL_PAGE_FILE_FLUSH_LSN, mtr);

	/* Copy the page to compressed storage, because it will be flushed
	to disk from there. */
	page_zip_des_t* blob_page_zip = buf_block_get_page_zip(m_cur_blob_block);

	ut_ad(blob_page_zip);
	ut_ad(page_zip_get_size(blob_page_zip)
	      == page_zip_get_size(m_ctx->get_page_zip()));

	page_zip_des_t* page_zip = buf_block_get_page_zip(m_ctx->rec_block());
	memcpy(blob_page_zip->data, blob_page, page_zip_get_size(page_zip));

	DBUG_RETURN(err);
}

/** Check if there is enough space in log file. Commit and re-start the
mini transaction. */
void
btr_blob_context_t::check_redolog_normal()
{
	DBUG_ENTER("btr_blob_context_t::check_redolog_normal");

	ut_ad(!is_bulk());

	FlushObserver* observer = m_btr_mtr->get_flush_observer();
	store_position();

	commit_btr_mtr();

	DEBUG_SYNC_C("blob_write_middle");

	log_free_check();

	DEBUG_SYNC_C("blob_write_middle_after_check");

	start_btr_mtr();

	m_btr_mtr->set_flush_observer(observer);

	restore_position();

	ut_ad(validate());

	DBUG_VOID_RETURN;
}

/** Write one blob page.  This function will be repeatedly called
with an increasing nth_blob_page to completely write a BLOB.
@param[in]	blob_j		the jth blob object of the record.
@param[in]	field		the big record field.
@param[in]	nth_blob_page	count of the BLOB page (starting from 1).
@return code as returned by the zlib. */
int
zblob_writer_t::write_single_blob_page(
	int			blob_j,
	big_rec_field_t&	field,
	int			nth_blob_page)
{
	DBUG_ENTER("zblob_writer_t::write_single_blob_page");
	DBUG_LOG("blob", "blob_j: " << blob_j << ", nth_blob_page: "
		<< nth_blob_page);

	buf_block_t*	rec_block	= m_ctx->rec_block();
	mtr_t*		mtr		= start_blob_mtr();

	buf_page_get(rec_block->page.id,
		     rec_block->page.size, RW_X_LATCH, mtr);

	buf_block_t*	blob_block = alloc_blob_page();
	page_t*		blob_page  = buf_block_get_frame(blob_block);

	append_page();

	m_prev_page_no = page_get_page_no(blob_page);
	DBUG_LOG("blob", "Updating prev page to: " << m_prev_page_no);

	log_page_type(blob_page, nth_blob_page);

	int err = write_into_page();

	ut_ad(!dict_index_is_spatial(m_ctx->index()));

	DBUG_LOG("blob", "err: " << err);

	if (err == Z_STREAM_END) {
		update_length_in_blobref(field);
	}

	/* Commit mtr and release uncompressed page frame to save memory.*/
	btr_blob_free(m_ctx->index(), m_cur_blob_block, FALSE, mtr);

	DBUG_RETURN(err);
}

dberr_t zblob_writer_t::prepare()
{
	DBUG_ENTER("zblob_writer_t::prepare");

	/* Zlib deflate needs 128 kilobytes for the default
	window size, plus 512 << memLevel, plus a few
	kilobytes for small objects.  We use reduced memLevel
	to limit the memory consumption, and preallocate the
	heap, hoping to avoid memory fragmentation. */
	m_heap = mem_heap_create(250000);

	if (m_heap == NULL) {
		DBUG_RETURN(DB_OUT_OF_MEMORY);
	}

	page_zip_set_alloc(&m_stream, m_heap);
	int ret = deflateInit2(&m_stream, page_zip_level,
			       Z_DEFLATED, 15, 7, Z_DEFAULT_STRATEGY);
	if (ret != Z_OK) {
		DBUG_RETURN(DB_FAIL);
	}

	DBUG_RETURN(DB_SUCCESS);
}

/** Write all the BLOBs of the clustered index record.
@return DB_SUCCESS on success, error code on failure. */
dberr_t
zblob_writer_t::write()
{
	DBUG_ENTER("zblob_writer_t::write");

	/* loop through each of the blob and write one blob at a time. */
	for (ulint i = 0;
	     i < m_ctx->get_big_rec_vec_size() && m_status == DB_SUCCESS;
	     i++) {

		ut_d(m_dir.clear(););
		DBUG_LOG("blob", "Writing blob: " << i);
		m_status = write_one_blob(static_cast<int>(i));

		DBUG_LOG("blob", "BLOB directory: " << m_dir);
	}

	DBUG_RETURN(m_status);
}

/** Make the current page as next page of previous page.  In other
words, make the page m_cur_blob_page_no as the next page
(FIL_PAGE_NEXT) of page m_prev_page_no.
@return DB_SUCCESS on success, or error code on failure. */
dberr_t
zblob_writer_t::append_page()
{
	DBUG_ENTER("zblob_writer_t::append_page");

	DBUG_LOG("blob", "prev_page: " << m_prev_page_no
		 << ", curr_page: " << m_cur_blob_page_no);

	space_id_t	space_id	= m_ctx->space();
	buf_block_t*	rec_block	= m_ctx->rec_block();

	buf_block_t*	prev_block = buf_page_get(
		page_id_t(space_id, m_prev_page_no),
		rec_block->page.size,
		RW_X_LATCH, &m_blob_mtr);

	buf_block_dbg_add_level(prev_block, SYNC_EXTERN_STORAGE);

	page_t*	prev_page = buf_block_get_frame(prev_block);

	mlog_write_ulint(prev_page + FIL_PAGE_NEXT, m_cur_blob_page_no,
			 MLOG_4BYTES, &m_blob_mtr);

	memcpy(buf_block_get_page_zip(prev_block)->data + FIL_PAGE_NEXT,
	       prev_page + FIL_PAGE_NEXT, 4);

	DBUG_RETURN(m_status);
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
zblob_reader_t::setup_zstream()
{
	DBUG_ENTER("zblob_reader_t::setup_zstream");
	const ulint local_prefix = m_local_len - BTR_EXTERN_FIELD_REF_SIZE;

	m_stream.next_out = m_buf + local_prefix;
	m_stream.avail_out = static_cast<uInt>(m_len - local_prefix);
	m_stream.next_in = Z_NULL;
	m_stream.avail_in = 0;

	/* Zlib inflate needs 32 kilobytes for the default
	window size, plus a few kilobytes for small objects. */
	m_heap = mem_heap_create(40000);
	page_zip_set_alloc(&m_stream, m_heap);

	int err = inflateInit(&m_stream);
	DBUG_RETURN(err);
}

/** Fetch the BLOB.
@return DB_SUCCESS on success, DB_FAIL on error. */
dberr_t
zblob_reader_t::fetch()
{
	dberr_t	err = DB_SUCCESS;
	DBUG_ENTER("zblob_reader_t::fetch");

	ut_ad(is_valid_blob());
	ut_ad(assert_empty_local_prefix());

	ut_d(m_page_type_ex = is_sdi() ? FIL_PAGE_SDI_ZBLOB : FIL_PAGE_TYPE_ZBLOB);

	setup_zstream();

	m_remaining = m_blobref.length();

	while (m_page_no != FIL_NULL) {

		page_no_t	curr_page_no = m_page_no;

		DBUG_LOG("lob", "m_remaining=" << m_remaining);

		err = fetch_page();
		if (err != DB_SUCCESS) {
			break;
		}

		m_stream.next_in = m_bpage->zip.data + m_offset;
		m_stream.avail_in = static_cast<uInt>(m_page_size.physical()
						      - m_offset);

		int zlib_err = inflate(&m_stream, Z_NO_FLUSH);
		switch (zlib_err) {
		case Z_OK:
			if (m_stream.avail_out == 0) {
				goto end_of_blob;
			}
			break;
		case Z_STREAM_END:
			if (m_page_no == FIL_NULL) {
				goto end_of_blob;
			}
			/* fall through */
		default:
			err = DB_FAIL;
			ib::error() << "inflate() of compressed BLOB page "
				<< page_id_t(m_space_id, curr_page_no)
				<< " returned " << zlib_err
				<< " (" << m_stream.msg << ")";
			/* fall through */
		case Z_BUF_ERROR:
			goto end_of_blob;
		}

		buf_page_release_zip(m_bpage);

		m_offset = FIL_PAGE_NEXT;

		ut_d(if (!m_is_sdi) m_page_type_ex = FIL_PAGE_TYPE_ZBLOB2);
	}

end_of_blob:
	buf_page_release_zip(m_bpage);
	DBUG_LOG("lob", "m_stream.total_out=" << m_stream.total_out);
	inflateEnd(&m_stream);
	mem_heap_free(m_heap);
	UNIV_MEM_ASSERT_RW(m_buf, m_stream.total_out);
	DBUG_RETURN(err);
}

#ifdef UNIV_DEBUG
/** Assert that the local prefix is empty.  For compressed row format,
there is no local prefix stored.  This function doesn't return if the
local prefix is non-empty.
@return true if local prefix is empty*/
bool
zblob_reader_t::assert_empty_local_prefix()
{
	ut_ad(m_local_len == BTR_EXTERN_FIELD_REF_SIZE);
	return(true);
}
#endif /* UNIV_DEBUG */

dberr_t
zblob_reader_t::fetch_page()
{
	dberr_t	err(DB_SUCCESS);

	DBUG_ENTER("zblob_reader_t::fetch_page");

	m_bpage = buf_page_get_zip(page_id_t(m_space_id, m_page_no),
				 m_page_size);
	ut_a(m_bpage != NULL);
	ut_ad(fil_page_get_type(m_bpage->zip.data) == m_page_type_ex);
	m_page_no = mach_read_from_4(m_bpage->zip.data + FIL_PAGE_NEXT);

	if (m_offset == FIL_PAGE_NEXT) {
		/* When the BLOB begins at page header,
		the compressed data payload does not
		immediately follow the next page pointer. */
		m_offset = FIL_PAGE_DATA;
	} else {
		m_offset += 4;
	}

	DBUG_LOG("lob", "m_offset=" << m_offset);
	DBUG_RETURN(err);
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
	enum blob_op		op)
{
	DBUG_ENTER("btr_store_big_rec_extern_fields");
	page_no_t	rec_page_no;
	ulint		extern_len;
	ulint		store_len;
	page_no_t	page_no;
	space_id_t	space_id;
	page_no_t	prev_page_no;
	page_no_t	hint_page_no;
	ulint		total_blob_pages = 0;
	ulint		i;
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
	      || dict_table_is_intrinsic(index->table));
	ut_ad(mtr_is_block_fix(
		btr_mtr, rec_block, MTR_MEMO_PAGE_X_FIX, index->table));
	ut_ad(buf_block_get_frame(rec_block) == page_align(rec));
	ut_a(dict_index_is_clust(index));

	ut_a(dict_table_page_size(index->table)
		.equals_to(rec_block->page.size));

	/* Create a blob operation context. */
	btr_blob_context_t ctx(pcur, btr_mtr, offsets, &rec_block,
			       &rec, op, big_rec_vec);

	page_zip = buf_block_get_page_zip(rec_block);
	space_id = rec_block->page.id.space();
	rec_page_no = rec_block->page.id.page_no();
	ut_a(fil_page_index_page_check(page_align(rec))
	     || op == BTR_STORE_INSERT_BULK);

#if defined UNIV_DEBUG || defined UNIV_BLOB_LIGHT_DEBUG
	/* All pointers to externally stored columns in the record
	must either be zero or they must be pointers to inherited
	columns, owned by this record or an earlier record version. */
	for (i = 0; i < big_rec_vec->n_fields; i++) {
		byte* field_ref = btr_rec_get_field_ref(
			rec, offsets, big_rec_vec->fields[i].field_no);

		blobref_t	blobref(field_ref);

		ut_a(blobref.is_owner());
		/* Either this must be an update in place,
		or the BLOB must be inherited, or the BLOB pointer
		must be zero (will be written in this function). */
		ut_a(op == BTR_STORE_UPDATE
		     || blobref.is_inherited()
		     || blobref.is_zero());
	}
#endif /* UNIV_DEBUG || UNIV_BLOB_LIGHT_DEBUG */

	/* Refactored compressed BLOB code */
	if (page_zip != NULL) {

		zblob_writer_t	zblob_writer(&ctx);
		dberr_t err = zblob_writer.prepare();
		if (err != DB_SUCCESS) {
			DBUG_RETURN(err);
		}

		zblob_writer.write();

		DBUG_RETURN(zblob_writer.finish());
	}

	ut_a(page_zip == NULL);

	const page_size_t	page_size(dict_table_page_size(index->table));

	/* Space available in uncompressed page to carry blob data */
	const ulint	payload_size = page_size.physical()
		- FIL_PAGE_DATA - BTR_BLOB_HDR_SIZE - FIL_PAGE_DATA_END;

	for (ulint i = 0; i < big_rec_vec->n_fields; i++) {
		total_blob_pages += (big_rec_vec->fields[i].len
				     + payload_size - 1)
			/ payload_size;
	}

	/* We have to create a file segment to the tablespace
	for each field and put the pointer to the field in rec */

	for (i = 0; i < big_rec_vec->n_fields; i++) {

		DBUG_LOG("blob", "Processing blob field: " << i);
		const ulint field_no = big_rec_vec->fields[i].field_no;

		byte	*field_ref = btr_rec_get_field_ref(
			ctx.rec(), ctx.rec_offsets(), field_no);

		blobref_t	blobref(field_ref);

#if defined UNIV_DEBUG || defined UNIV_BLOB_LIGHT_DEBUG
		/* A zero BLOB pointer should have been initially inserted. */
		ut_a(blobref.is_zero());
#endif /* UNIV_DEBUG || UNIV_BLOB_LIGHT_DEBUG */
		extern_len = big_rec_vec->fields[i].len;
		UNIV_MEM_ASSERT_RW(big_rec_vec->fields[i].data,
				   extern_len);

		ut_a(extern_len > 0);

		prev_page_no = FIL_NULL;

		for (ulint blob_npages = 0;; ++blob_npages) {
			buf_block_t*	block;
			page_t*		page;
			const ulint	commit_freq = 4;
			ulint		r_extents;

			ut_ad(blobref.equals(field_ref));

			ut_ad(page_align(field_ref) == page_align(rec));

			if (!(blob_npages % commit_freq)) {

				ctx.check_redolog();

				field_ref = btr_rec_get_field_ref(
					ctx.rec(),
					ctx.rec_offsets(), field_no);

				blobref.set_blobref(field_ref);
				rec_block	= ctx.rec_block();
				page_zip = buf_block_get_page_zip(rec_block);
				rec_page_no = rec_block->page.id.page_no();
			}

			mtr_start(&mtr);
			mtr.set_named_space(index->space);
			mtr.set_log_mode(ctx.get_log_mode());
			mtr.set_flush_observer(ctx.get_flush_observer());

			buf_page_get(ctx.rec_block()->page.id,
				     ctx.rec_block()->page.size, RW_X_LATCH, &mtr);

			if (prev_page_no == FIL_NULL) {
				hint_page_no = 1 + rec_page_no;
			} else {
				hint_page_no = prev_page_no + 1;
			}

			mtr_t	*alloc_mtr;

			if (op == BTR_STORE_INSERT_BULK) {
				mtr_start(&mtr_bulk);
				mtr_bulk.set_spaces(mtr);
				alloc_mtr = &mtr_bulk;
			} else {
				alloc_mtr = &mtr;
			}

			if (!fsp_reserve_free_extents(&r_extents, space_id, 1,
						      FSP_BLOB, alloc_mtr,
						      1)) {

				alloc_mtr->commit();
				error = DB_OUT_OF_FILE_SPACE;
				goto func_exit;
			}

			block = btr_page_alloc(index, hint_page_no, FSP_NO_DIR,
					       0, alloc_mtr, &mtr);

			alloc_mtr->release_free_extents(r_extents);

			if (op == BTR_STORE_INSERT_BULK) {
				mtr_bulk.commit();
			}

			ut_a(block != NULL);

			page_no = block->page.id.page_no();
			page = buf_block_get_frame(block);

			if (prev_page_no != FIL_NULL) {
				buf_block_t*	prev_block;
				page_t*		prev_page;

				prev_block = buf_page_get(
					page_id_t(space_id, prev_page_no),
					ctx.rec_block()->page.size,
					RW_X_LATCH, &mtr);

				buf_block_dbg_add_level(prev_block,
							SYNC_EXTERN_STORAGE);
				prev_page = buf_block_get_frame(prev_block);

				mlog_write_ulint(prev_page + FIL_PAGE_DATA
						 + BTR_BLOB_HDR_NEXT_PAGE_NO,
						 page_no, MLOG_4BYTES, &mtr);

			} else if (dict_index_is_online_ddl(index)) {
				row_log_table_blob_alloc(index, page_no);
			}

			page_type_t	page_type =
				dict_index_is_sdi(index)
				? FIL_PAGE_SDI_BLOB
				: FIL_PAGE_TYPE_BLOB;

			mlog_write_ulint(page + FIL_PAGE_TYPE, page_type,
					 MLOG_2BYTES, &mtr);

			if (extern_len > payload_size) {
				store_len = payload_size;
			} else {
				store_len = extern_len;
			}

			mlog_write_string(page + FIL_PAGE_DATA
					  + BTR_BLOB_HDR_SIZE,
					  (const byte*)
					  big_rec_vec->fields[i].data
					  + big_rec_vec->fields[i].len
					  - extern_len,
					  store_len, &mtr);
			mlog_write_ulint(page + FIL_PAGE_DATA
					 + BTR_BLOB_HDR_PART_LEN,
					 store_len, MLOG_4BYTES, &mtr);
			mlog_write_ulint(page + FIL_PAGE_DATA
					 + BTR_BLOB_HDR_NEXT_PAGE_NO,
					 FIL_NULL, MLOG_4BYTES, &mtr);

			extern_len -= store_len;

			mlog_write_ulint(field_ref + BTR_EXTERN_LEN, 0,
					 MLOG_4BYTES, &mtr);
			mlog_write_ulint(field_ref
					 + BTR_EXTERN_LEN + 4,
					 big_rec_vec->fields[i].len
					 - extern_len,
					 MLOG_4BYTES, &mtr);

			if (prev_page_no == FIL_NULL) {
				ut_ad(blob_npages == 0);

				blobref.update(space_id, page_no,
					       FIL_PAGE_DATA, &mtr);
			}

			prev_page_no = page_no;

			mtr.commit();

			if (extern_len == 0) {
				break;
			}
		}

		DBUG_EXECUTE_IF("btr_store_big_rec_extern",
				error = DB_OUT_OF_FILE_SPACE;
				goto func_exit;);

		rec_offs_make_nth_extern(offsets, field_no);
	}

func_exit:

#if defined UNIV_DEBUG || defined UNIV_BLOB_LIGHT_DEBUG
	/* All pointers to externally stored columns in the record
	must be valid. */
	for (i = 0; i < rec_offs_n_fields(offsets); i++) {
		if (!rec_offs_nth_extern(offsets, i)) {
			continue;
		}

		byte* field_ref = btr_rec_get_field_ref(rec, offsets, i);

		blobref_t	blobref(field_ref);

		/* The pointer must not be zero if the operation
		succeeded. */
		ut_a(!blobref.is_zero() || error != DB_SUCCESS);

		/* The column must not be disowned by this record. */
		ut_a(blobref.is_owner());
	}
#endif /* UNIV_DEBUG || UNIV_BLOB_LIGHT_DEBUG */
	DBUG_RETURN(error);
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
	DBUG_ENTER("btr_rec_copy_externally_stored_field");

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

		DBUG_RETURN(NULL);
	}

	DBUG_RETURN(btr_copy_externally_stored_field(
			len, data, page_size, local_len, is_sdi, heap));
}
