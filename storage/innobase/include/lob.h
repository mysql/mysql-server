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
#ifndef lob0lob_h
#define lob0lob_h

#include "univ.i"
#include "dict0mem.h"
#include "page0page.h"
#include "row0log.h"
#include <my_dbug.h>

/** Operation code for btr_store_big_rec_extern_fields(). */
enum blob_op {
	/** Store off-page columns for a freshly inserted record */
	BTR_STORE_INSERT = 0,
	/** Store off-page columns for an insert by update */
	BTR_STORE_INSERT_UPDATE,
	/** Store off-page columns for an update */
	BTR_STORE_UPDATE,
	/** Store off-page columns for a freshly inserted record by bulk */
	BTR_STORE_INSERT_BULK
};

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
	MY_ATTRIBUTE((warn_unused_result));

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
	mem_heap_t*		heap);

#ifdef UNIV_DEBUG
# define btr_rec_copy_externally_stored_field(			\
	rec, offsets, page_size, no, len, is_sdi, heap)		\
	btr_rec_copy_externally_stored_field_func(		\
	rec, offsets, page_size, no, len, is_sdi, heap)
#else /* UNIV_DEBUG */
# define btr_rec_copy_externally_stored_field(			\
	rec, offsets, page_size, no, len, is_sdi, heap)		\
	btr_rec_copy_externally_stored_field_func(		\
	rec, offsets, page_size, no, len, heap)
#endif /* UNIV_DEBUG */

/** Gets the offset of the pointer to the externally stored part of a field.
@param[in]	offsets		array returned by rec_get_offsets()
@param[in]	n		index of the external field
@return offset of the pointer to the externally stored part */
ulint
btr_rec_get_field_ref_offs(
	const ulint*	offsets,
	ulint		n);

/** Gets a pointer to the externally stored part of a field.
@param rec record
@param offsets rec_get_offsets(rec)
@param n index of the externally stored field
@return pointer to the externally stored part */
#define btr_rec_get_field_ref(rec, offsets, n)			\
	((rec) + btr_rec_get_field_ref_offs(offsets, n))

/** Deallocate a buffer block that was reserved for a BLOB part.
@param[in]	index	index
@param[in]	block	buffer block
@param[in]	all	TRUE=remove also the compressed page
			if there is one
@param[in]	mtr	mini-transaction to commit */
void
btr_blob_free(
	dict_index_t*	index,
	buf_block_t*	block,
	bool		all,
	mtr_t*		mtr);

/** The context for a blob operation.  It contains the necessary information
to carry out a blob operation. */
class btr_blob_context_t
{
public:
	/** Constructor
	@param[in]	pcur		persistent cursor on a clustered
					index record with blobs.
	@param[in]	mtr		mini-transaction holding latches for
					pcur.
	@param[in]	offsets		offsets of the clust_rec
	@param[in,out]	block		record block containing pcur record
	@param[in,out]	rec		the clustered record pointer
	@param[in]	op		the blob operation code
	@param[in]	big_rec_vec	array of blobs */
	btr_blob_context_t(
		btr_pcur_t*		pcur,
		mtr_t*			mtr,
		ulint* const		offsets,
		buf_block_t**		block,
		rec_t**			rec,
		enum blob_op		op,
		const big_rec_t*	big_rec_vec)
		:
		m_pcur(pcur),
		m_btr_mtr(mtr),
		m_block(block),
		m_btr_page_no(page_get_page_no(buf_block_get_frame(*m_block))),
		m_rec_offset(page_offset(*rec)),
		m_rec(rec),
		m_op(op),
		m_big_rec_vec(big_rec_vec),
		m_offsets(offsets)
	{
		ut_ad(rec_offs_validate(*m_rec, m_pcur->index(), m_offsets));
		ut_ad((*m_block)->frame == page_align(*m_rec));
		ut_ad(*m_rec == btr_pcur_get_rec(m_pcur));
	}

#ifdef UNIV_DEBUG
	/** Validate the current BLOB context object.  The BLOB context object
	is valid if the necessary latches are being held by the
	mini-transaction of the B-tree (btr mtr).  Does not return if the
	validation fails.
	@return true if valid */
	bool validate() const
	{
		DBUG_ENTER("btr_blob_context_t::validate");

		rec_offs_make_valid(rec(), index(), m_offsets);

		ut_ad(m_btr_mtr->memo_contains_page_flagged(
				*m_rec,
				MTR_MEMO_PAGE_X_FIX | MTR_MEMO_PAGE_SX_FIX)
				|| dict_table_is_intrinsic(table()));

		ut_ad(mtr_memo_contains_flagged(m_btr_mtr,
				dict_index_get_lock(index()),
				MTR_MEMO_SX_LOCK | MTR_MEMO_X_LOCK)
				|| dict_table_is_intrinsic(table()));

		DBUG_RETURN(true);
	}
#endif
	bool is_bulk() const {
                return(m_op == BTR_STORE_INSERT_BULK);
	}

	/** Get the clustered index record pointer.
	@return clustered index record pointer. */
	rec_t*	rec() const
	{
		ut_ad(*m_rec == btr_pcur_get_rec(m_pcur));
		return(*m_rec);
	}

	/** Get the beginning of the B-tree clustered index page frame
	that contains the current clustered index record (m_rec). */
	const page_t*	rec_frame() const
	{
		ut_ad((*m_block)->frame == page_align(*m_rec));
		return((*m_block)->frame);
	}

	/** Commit the mini transaction that is holding the latches
	of the clustered index record block. */
	void commit_btr_mtr()
	{
		m_btr_mtr->commit();
	}

	/** Start the mini transaction that will be holding the latches
	of the clustered index record block. */
	void start_btr_mtr()
	{
		const mtr_log_t log_mode = m_btr_mtr->get_log_mode();
		m_btr_mtr->start();
		m_btr_mtr->set_log_mode(log_mode);
		m_btr_mtr->set_named_space(space());
	}

	/** Increment the buffer fix count of the clustered index record
	block. */
	void rec_block_fix()
	{
		m_rec_offset = page_offset(*m_rec);
		m_btr_page_no = page_get_page_no(
			buf_block_get_frame(*m_block));
		buf_block_buf_fix_inc(rec_block(), __FILE__, __LINE__);
	}

	void rec_block_unfix();

	/** Save the position of the persistent cursor. */
	void store_position()
	{
		DBUG_ENTER("btr_blob_context_t::store_position");

		btr_pcur_store_position(m_pcur, m_btr_mtr);

		DBUG_VOID_RETURN;
	}

	/** Restore the position of the persistent cursor. */
	void restore_position()
	{
		DBUG_ENTER("btr_blob_context_t::restore_position");

		ut_ad(m_pcur->rel_pos == BTR_PCUR_ON);
		bool ret = btr_pcur_restore_position(
			BTR_MODIFY_LEAF | BTR_MODIFY_EXTERNAL,
			m_pcur, m_btr_mtr);

		ut_a(ret);

		recalc();
		DBUG_VOID_RETURN;
	}

	/** Gets the compressed page descriptor
	@return the compressed page descriptor. */
	page_zip_des_t*	get_page_zip() const
	{
		return(buf_block_get_page_zip(*m_block));
	}

	/** The offsets of the clustered index record as returned by
	rec_get_offsets().
	@return offsets of clust record. */
	const ulint*	rec_offsets() const
	{
		return(m_offsets);
	}

	/** Get the index object.
	@return index object */
	dict_index_t*	index() const {
		return(m_pcur->index());
	}

	/** Get the table object.
	@return table object */
	dict_table_t*	table() const {
		return(m_pcur->index()->table);
	}

	/** Get the space id.
	@return space id. */
	space_id_t	space() const {
		return(index()->space);
	}

	/** Obtain the page size of the underlying table.
	@return page size of the underlying table. */
	const page_size_t page_size() const
	{
		return(dict_table_page_size(table()));
	}

	/** Determine the extent size (in pages) for the underlying table
	@return extent size in pages */
	ulint pages_in_extent() const {
		return(dict_table_extent_size(table()));
	}

	/** Get the page number of the clustered index record block
	@return page number of the clust rec block. */
	page_no_t btr_page_no() const {
		return(m_btr_page_no);
	}

	/** Get the pointer to the clustered record block.
	@return pointer to the clustered rec block. */
	buf_block_t*	rec_block() const
	{
		return(*m_block);
	}

	/** Check if there is enough space in the redo log file.  The btr
	mini transaction will be restarted. */
	void check_redolog() {
		is_bulk() ? check_redolog_bulk() : check_redolog_normal();
	}

	/** Mark the nth field as externally stored.
	@param[in]	field_no	the field number. */
	void make_nth_extern(ulint field_no)
	{
		rec_offs_make_nth_extern(m_offsets, field_no);
	}

	/** Get the vector containing fields to be stored externally.
	@return the big record vector */
	const big_rec_t* get_big_rec_vec()
	{
		return(m_big_rec_vec);
	}

	/** Get the size of vector containing fields to be stored externally.
	@return the big record vector size */
	ulint	get_big_rec_vec_size()
	{
		return(m_big_rec_vec->n_fields);
	}

	/** Get the log mode of the btr mtr.
	@return the log mode. */
	mtr_log_t get_log_mode()
	{
		return(m_btr_mtr->get_log_mode());
	}

	/** Get flush observer
	@return flush observer */
	FlushObserver* get_flush_observer() const
	{
		return(m_btr_mtr->get_flush_observer());
	}

	/** Write a blob reference of a field into a clustered index record
	in a compressed leaf page. The information must already have been
	updated on the uncompressed page.
	@param[in]	field_no	the blob field number
	@param[in]	mtr		the mini transaction to update
					blob page. */
	void zblob_write_blobref(ulint field_no, mtr_t* mtr)
	{
		page_zip_write_blob_ptr(get_page_zip(),
					rec(),
					index(),
					rec_offsets(),
					field_no, mtr);
	}

private:
	/** Check if there is enough space in log file. Commit and re-start the
	mini transaction. */
	void check_redolog_normal();

	/** When bulk load is being done, check if there is enough space in redo
	log file. */
	void check_redolog_bulk();

	/** Recalculate some of the members after restoring the persistent
	cursor. */
	void recalc() {
		*m_block	= btr_pcur_get_block(m_pcur);
		*m_rec		= btr_pcur_get_rec(m_pcur);
		m_btr_page_no	= page_get_page_no(buf_block_get_frame(*m_block));
		m_rec_offset	= page_offset(*m_rec);

		rec_offs_make_valid(rec(), index(), m_offsets);
	}

	/** Persistent cursor on a clusterex index record with blobs. */
	btr_pcur_t*	m_pcur;

	/** Mini transaction holding the latches for m_pcur */
	mtr_t*		m_btr_mtr;

	/** The block containing clustered record */
	buf_block_t**	m_block;

	/** Page number where the clust rec is present. */
	page_no_t	m_btr_page_no;

	/** Page number where the clust rec is present. */
	ulint		m_rec_offset;

	/** The clustered record pointer */
	rec_t**		m_rec;

	/** The blob operation code */
	enum blob_op	m_op;

	/** vector containing fields to be stored externally */
	const big_rec_t*	m_big_rec_vec;

	/** rec_get_offsets(rec, index); offset of clust_rec */
	ulint* const m_offsets;
};


inline
void btr_blob_context_t::rec_block_unfix()
{
	DBUG_ENTER("btr_blob_context_t::rec_block_unfix");

	space_id_t	space_id = space();
	page_id_t       page_id(space_id, m_btr_page_no);
	page_size_t     page_size(dict_table_page_size(table()));
	page_cur_t*	page_cur = &m_pcur->btr_cur.page_cur;

	mtr_x_lock(dict_index_get_lock(index()), m_btr_mtr);

	page_cur->block = btr_block_get(page_id, page_size,
					RW_X_LATCH, index(),
					m_btr_mtr);
	page_cur->rec = buf_block_get_frame(page_cur->block) + m_rec_offset;
	buf_block_buf_fix_dec(page_cur->block);

	recalc();

	DBUG_VOID_RETURN;
}

/** Information about data stored in one BLOB page. */
struct blob_page_info_t
{
	/** Constructor.
	@param[in]	page_no		the BLOB page number.
	@param[in]	bytes		amount of uncompressed BLOB data
					in BLOB page in bytes.
	@param[in]	zbytes		amount of compressed BLOB data
					in BLOB page in bytes. */
	blob_page_info_t(page_no_t page_no, ulint bytes, ulint zbytes)
		: m_page_no(page_no), m_bytes(bytes), m_zbytes(zbytes)
	{}

	/** Re-initialize the current object. */
	void reset()
	{
		m_page_no = 0;
		m_bytes = 0;
		m_zbytes = 0;
	}

	/** Print this blob_page_into_t object into the given output stream.
	@param[in]	out	the output stream.
	@return the output stream. */
	std::ostream& print(std::ostream& out) const;

	/** Set the compressed data size in bytes.
	@param[in]	bytes	the new compressed data size. */
	void set_compressed_size(ulint bytes)
	{
		m_zbytes = bytes;
	}

	/** Set the uncompressed data size in bytes.
	@param[in]	bytes	the new uncompressed data size. */
	void set_uncompressed_size(ulint bytes)
	{
		m_bytes = bytes;
	}

	/** Set the page number.
	@param[in]	page_no		the page number */
	void set_page_no(page_no_t page_no)
	{
		m_page_no = page_no;
	}

private:
	/** The BLOB page number */
	page_no_t	m_page_no;

	/** Amount of uncompressed data (in bytes) in the BLOB page. */
	ulint	m_bytes;

	/** Amount of compressed data (in bytes) in the BLOB page. */
	ulint	m_zbytes;
};

inline
std::ostream&
operator<<(std::ostream& out, const blob_page_info_t& obj)
{
	return(obj.print(out));
}

/** The in-memory blob directory.  Each blob contains a sequence of pages.
This directory contains a list of those pages along with their metadata. */
struct blob_dir_t
{
	typedef std::vector<blob_page_info_t>::const_iterator const_iterator;

	/** Print this blob directory into the given output stream.
	@param[in]	out	the output stream.
	@return the output stream. */
	std::ostream& print(std::ostream& out) const;

	/** Clear the contents of this blob directory. */
	void clear()
	{
		m_pages.clear();
	}

	/** Append the given blob page information.
	@param[in]	page	the blob page information to be added.
	@return DB_SUCCESS on success, error code on failure. */
	dberr_t	add(const blob_page_info_t& page)
	{
		m_pages.push_back(page);
		return(DB_SUCCESS);
	}

	/** A vectory of blob pages along with its metadata. */
	std::vector<blob_page_info_t>	m_pages;
};

inline
std::ostream&
operator<<(std::ostream& out, const blob_dir_t& obj)
{
	return(obj.print(out));
}

/** Insert or write the compressed BLOB. */
class zblob_writer_t
{
public:
	/** Constructor.
	@param[in]	ctx	blob operation context. */
	zblob_writer_t(btr_blob_context_t* ctx);

	/** Destructor. */
	~zblob_writer_t();

	/** Prepare to write a compressed BLOB. Setup the zlib
	compression stream.
	@return DB_SUCCESS on success, error code on failure. */
	dberr_t prepare();

	/** Write all the BLOBs of the clustered index record.
	@return DB_SUCCESS on success, error code on failure. */
	dberr_t write();

	/** Cleanup after completing the write of compressed BLOB.
	@return DB_SUCCESS on success, error code on failure. */
	dberr_t	finish()
	{
		int ret = deflateEnd(&m_stream);
		ut_ad(ret == Z_OK);
		ut_ad(validate_blobrefs());

		if (ret != Z_OK) {
			m_status = DB_FAIL;
		}

		return(m_status);
	}

	/** Write the page type of the BLOB page and also generate the
	redo log record.
	@param[in]	blob_page	the BLOB page
	@param[in]	nth_blob_page	the count of BLOB page from
					the beginning of the BLOB. */
	void log_page_type(page_t* blob_page, ulint nth_blob_page)
	{
		page_type_t	page_type;

		if (is_index_sdi()) {
			page_type = FIL_PAGE_SDI_ZBLOB;
		} else if (nth_blob_page == 0) {
			page_type = FIL_PAGE_TYPE_ZBLOB;
		} else {
			page_type = FIL_PAGE_TYPE_ZBLOB2;
		}

		mlog_write_ulint(blob_page + FIL_PAGE_TYPE, page_type,
				 MLOG_2BYTES, &m_blob_mtr);
	}

	/** Calculate the total number of pages needed to store
	the given blobs */
	ulint	calc_total_pages()
	{
		const page_size_t page_size = m_ctx->page_size();

		/* Space available in compressed page to carry blob data */
		const ulint payload_size_zip
			= page_size.physical() - FIL_PAGE_DATA;

		const big_rec_t*	vec = m_ctx->get_big_rec_vec();

		ulint total_blob_pages = 0;
		for (ulint i = 0; i < vec->n_fields; i++) {
			total_blob_pages += static_cast<ulint>(
				deflateBound(&m_stream, static_cast<uLong>(
					vec->fields[i].len))
				+ payload_size_zip - 1) / payload_size_zip;
		}

		return(total_blob_pages);
	}

	/** Allocate one BLOB page.
	@return the allocated block of the BLOB page. */
	buf_block_t*	alloc_blob_page();

	/** Write contents into a single BLOB page.
	@return code as returned by zlib. */
	int write_into_page();

	/** Start the BLOB mtr.
	@return pointer to the BLOB mtr. */
	mtr_t* start_blob_mtr()
	{
		mtr_start(&m_blob_mtr);
		m_blob_mtr.set_named_space(m_ctx->space());
		m_blob_mtr.set_log_mode(m_ctx->get_log_mode());
		m_blob_mtr.set_flush_observer(m_ctx->get_flush_observer());
		return(&m_blob_mtr);
	}

	/** Commit the BLOB mtr. */
	void commit_blob_mtr()
	{
		mtr_commit(&m_blob_mtr);
	}

	/** Check if the index is SDI index
	@return true if index is SDI index else false */
	bool is_index_sdi()
	{
		return(dict_index_is_sdi(m_ctx->index()));
	}

	/** Write one blob page.  This function will be repeatedly called
	with an increasing nth_blob_page to completely write a BLOB.
	@param[in]	blob_j		the jth blob object of the record.
	@param[in]	field		the big record field.
	@param[in]	nth_blob_page	count of the BLOB page (starting from 1).
	@return code as returned by the zlib. */
	int write_single_blob_page(int blob_j, big_rec_field_t& field,
				   int nth_blob_page);

	/** Write first blob page.
	@param[in]	blob_j		the jth blob object of the record.
	@param[in]	field		the big record field.
	@return code as returned by the zlib. */
	int write_first_page(ulint blob_j, big_rec_field_t& field);

#ifdef UNIV_DEBUG
	/** Verify that all pointers to externally stored columns in the record
	is be valid.  If validation fails, this function doesn't return.
	@return true if valid. */
	bool validate_blobrefs() const
	{
		const ulint *offsets = m_ctx->rec_offsets();

		for (ulint i = 0; i < rec_offs_n_fields(offsets); i++) {
			if (!rec_offs_nth_extern(offsets, i)) {
				continue;
			}

			byte* field_ref = btr_rec_get_field_ref(
				m_ctx->rec(), offsets, i);

			blobref_t	blobref(field_ref);

			/* The pointer must not be zero if the operation
			succeeded. */
			ut_a(!blobref.is_zero() || m_status != DB_SUCCESS);

			/* The column must not be disowned by this record. */
			ut_a(blobref.is_owner());
		}
		return(true);
	}
#endif /* UNIV_DEBUG */

	/** For the given blob field, update its length in the blob reference
	which is available in the clustered index record.
	@param[in]	field	the concerned blob field. */
	void update_length_in_blobref(big_rec_field_t& field);

	/** Make the current page as next page of previous page.  In other
	words, make the page m_cur_blob_page_no as the next page
	(FIL_PAGE_NEXT) of page m_prev_page_no.
	@return DB_SUCCESS on success, or error code on failure. */
	dberr_t append_page();

private:
#ifdef UNIV_DEBUG
	/** Add the BLOB page information to the directory
	@param[in]	page_info	BLOB page information. */
	void add_to_blob_dir(const blob_page_info_t&	page_info)
	{
		m_dir.add(page_info);
	}
#endif /* UNIV_DEBUG */

	/** Write one blob field data.
	@param[in]	blob_j	the blob field number
	@return DB_SUCCESS on success, error code on failure. */
	dberr_t write_one_blob(int blob_j);

	mem_heap_t*		m_heap;
	btr_blob_context_t*	m_ctx;
	z_stream		m_stream;
	buf_block_t*		m_cur_blob_block;
	ulint			m_cur_blob_page_no;

	/** The mini trx used to write into blob pages */
	mtr_t			m_blob_mtr;
	page_no_t		m_prev_page_no;
	dberr_t			m_status;

#ifdef UNIV_DEBUG
	/** The BLOB directory information. */
	blob_dir_t		m_dir;
#endif /* UNIV_DEBUG */
};

/** Constructor.
@param[in]	ctx	blob operation context. */
inline
zblob_writer_t::zblob_writer_t(btr_blob_context_t* ctx)
	: m_heap(NULL), m_ctx(ctx),
	  m_prev_page_no(FIL_NULL), m_status(DB_SUCCESS)
{
	m_prev_page_no = m_ctx->btr_page_no();
}

inline
zblob_writer_t::~zblob_writer_t()
{
	if (m_heap != NULL) {
		mem_heap_free(m_heap);
	}
}

/** Fetch compressed BLOB */
struct zblob_reader_t
{
	/** Constructor.
	@param[in]	page_size	the page size information.
	@param[in]	buf		the buffer into which data is read.
	@param[in]	len		the length of buffer.
	@param[in]	data		the 'internally' stored part of BLOB.
	@param[in]	local_len	the length of 'internally' stored
					part of BLOB. */
	zblob_reader_t(
		const page_size_t&	page_size,
		byte			*buf,
		ulint			len,
		const byte*		data,
		ulint			local_len)
	:
	m_page_size(page_size),
	m_buf(buf),
	m_len(len),
	m_data(data),
	m_local_len(local_len),
	m_blobref(const_cast<byte*>(data) + local_len
		  - BTR_EXTERN_FIELD_REF_SIZE)
#ifdef UNIV_DEBUG
	, m_is_sdi(false)
#endif /* UNIV_DEBUG */
	{
		read_blobref();
	}

	/** Check if the BLOB reference is valid.  For this particular check,
	if the length of the BLOB is greater than 0, it is considered
	valid.
	@return true if valid. */
	bool	is_valid_blob() const {
		return(m_blobref.length() > 0);
	}

	/** Fetch the BLOB.
	@return DB_SUCCESS on success. */
	dberr_t	fetch();

	/** Fetch one BLOB page.
	@return DB_SUCCESS on success. */
	dberr_t fetch_page();

	/** Get the length of data that has been read.
	@return the length of data that has been read. */
	ulint length() const
	{
		return(m_stream.total_out);
	}

#ifdef UNIV_DEBUG
	/** Is it a space dictionary index (SDI)?
	@return true if SDI, false otherwise. */
	bool is_sdi() const {
		return(m_is_sdi);
	}

	/** Set whether it is an SDI or not.
	@param[in]	sdi	 true if SDI, false otherwise. */
	void set_sdi(bool sdi)
	{
		m_is_sdi = sdi;
	}
#endif /* UNIV_DEBUG */

private:
	/** Do setup of the zlib stream.
	@return code returned by zlib. */
	int setup_zstream();

	/** Read the space_id, page_no and offset information from the BLOB
	reference object and update the member variables. */
	void read_blobref()
	{
		m_space_id	= m_blobref.space_id();
		m_page_no	= m_blobref.page_no();
		m_offset	= m_blobref.offset();
	}

#ifdef UNIV_DEBUG
	/** Assert that the local prefix is empty.  For compressed row format,
	there is no local prefix stored.  This function doesn't return if the
	local prefix is non-empty.
	@return true if local prefix is empty*/
	bool assert_empty_local_prefix();
#endif /* UNIV_DEBUG */

	/** The page size information. */
	const page_size_t&	m_page_size;

	/** Buffer into which data is read. */
	byte*	m_buf;

	/** Length of the buffer m_buf. */
	ulint	m_len;

	/** Bytes yet to be read. */
	ulint	m_remaining;

	/** The 'internally' stored part of the field containing also the
	reference to the external part; must be protected by a lock or a page
	latch */
	const byte*	m_data;

	/** Length of m_data in bytes. */
	ulint	m_local_len;

	/** The zlib stream used to uncompress while fetching blob. */
	z_stream	m_stream;

	/** The memory heap that will be used by zlib allocator. */
	mem_heap_t*	m_heap;

	/** The blob reference of the blob that is being read. */
	const blobref_t	m_blobref;

	/** The identifier of the space in which blob is available. */
	space_id_t	m_space_id;

	/** The page number obtained from the blob reference. */
	page_no_t	m_page_no;

	/** The offset information obtained from the blob reference. */
	ulint	m_offset;

	/* There is no latch on m_bpage directly.  Instead,
	m_bpage is protected by the B-tree page latch that
	is being held on the clustered index record, or,
	in row_merge_copy_blobs(), by an exclusive table lock. */
	buf_page_t*	m_bpage;

#ifdef UNIV_DEBUG
	/** The expected page type. */
	ulint	m_page_type_ex;

	/** Is it a tablespace dictionary index (SDI)? */
	bool	m_is_sdi;
#endif /* UNIV_DEBUG */
};

struct blob_delete_context_t
{
	blob_delete_context_t(byte *field_ref) : m_blobref(field_ref)
	{}

	blob_delete_context_t(
		byte*		field_ref,
		dict_index_t*	index,
		byte*		rec,
		const ulint*	offsets,
		page_zip_des_t*	page_zip,
		ulint		field_no,
		bool		rollback,
		mtr_t*		btr_mtr)
	:
	m_index(index),
	m_blobref(field_ref),
	m_rec(rec),
	m_offsets(offsets),
	m_page_zip(page_zip),
	m_field_no(field_no),
	m_rollback(rollback),
	m_btr_mtr(btr_mtr)
	{}

#ifdef UNIV_DEBUG
	bool validate_blobref() const
	{
		if (m_rec != NULL) {
			const byte*	v2 = btr_rec_get_field_ref(
				m_rec, m_offsets, m_field_no);

			ut_ad(m_blobref.equals(v2));
		}
		return(true);
	}

	bool rec_offs_validate() const
	{
		if (m_rec != NULL) {
			ut_ad(::rec_offs_validate(m_rec, m_index, m_offsets));
		}
		return(true);
	}

	bool is_named_space() const
	{
		ut_ad(m_btr_mtr->is_named_space(page_get_space_id(
					m_blobref.page_align())));
		return(true);
	}
#endif /* UNIV_DEBUG */

	dict_table_t*	table() const
	{
		return(m_index->table);
	}

	/** Index of the data, the index tree MUST be X-latched; if the tree
	height is 1, then also the root page must be X-latched! (this is
	relevant in the case this function is called from purge where 'data'
	is located on an undo log page, not an index page) */
	dict_index_t*	m_index;

	/** the BLOB reference or external field reference. */
	blobref_t	m_blobref;

	/** record containing m_blobref, for page_zip_write_blob_ptr()
	or NULL */
	const rec_t*	m_rec;

	/** the record offsets as returned by rec_get_offsets(m_rec, m_index),
	or NULL. */
	const ulint*	m_offsets;

	/** compressed page corresponding to m_rec, or NULL */
	page_zip_des_t*	m_page_zip;

	/** field number of externally stored column; ignored if rec == NULL */
	ulint		m_field_no;

	/** Is this operation part of rollback? */
	bool		m_rollback;

	/** mtr containing the latch to data an X-latch to the index tree */
	mtr_t*		m_btr_mtr;
};

/* Delete a BLOB */
class zblob_delete_t
{
public:
	zblob_delete_t(blob_delete_context_t&	ctx): m_ctx(ctx)
	{
		ut_ad(dict_index_is_clust(ctx.m_index));
		ut_ad(mtr_memo_contains_flagged(
				ctx.m_btr_mtr,
				dict_index_get_lock(ctx.m_index),
				MTR_MEMO_X_LOCK | MTR_MEMO_SX_LOCK)
		      || dict_table_is_intrinsic(ctx.table()));
		ut_ad(mtr_is_page_fix(ctx.m_btr_mtr,
				      ctx.m_blobref.page_align(),
				      MTR_MEMO_PAGE_X_FIX, ctx.table()));
		ut_ad(ctx.rec_offs_validate());
		ut_ad(ctx.validate_blobref());
		ut_ad(ctx.is_named_space());
	}

	dberr_t	destroy()
	{
		dberr_t	err(DB_SUCCESS);

		if (!can_free()) {
			return(DB_SUCCESS);
		}

		if (dict_index_is_online_ddl(m_ctx.m_index)) {
			row_log_table_blob_free(m_ctx.m_index,
						m_ctx.m_blobref.page_no());
		}

		while (m_ctx.m_blobref.page_no() == FIL_NULL) {
			err = free_first_page();
			if (err != DB_SUCCESS) {
				break;
			}
		}

		return(err);
	}

	dberr_t	free_first_page()
	{
		dberr_t	err(DB_SUCCESS);

		mtr_start(&m_mtr);
		m_mtr.set_spaces(*m_ctx.m_btr_mtr);
		m_mtr.set_log_mode(m_ctx.m_btr_mtr->get_log_mode());

		ut_ad(!dict_table_is_temporary(m_ctx.table())
		      || m_ctx.m_btr_mtr->get_log_mode() == MTR_LOG_NO_REDO);

		page_no_t	page_no = m_ctx.m_blobref.page_no();
		space_id_t	space_id = m_ctx.m_blobref.space_id();

		buf_block_t*	blob_block = buf_page_get(
			page_id_t(space_id, page_no),
			dict_table_page_size(m_ctx.table()),
			RW_X_LATCH, &m_mtr);

		buf_block_dbg_add_level(blob_block, SYNC_EXTERN_STORAGE);
		page_t*	page = buf_block_get_frame(blob_block);

		switch (fil_page_get_type(page)) {
		case FIL_PAGE_TYPE_ZBLOB:
		case FIL_PAGE_TYPE_ZBLOB2:
		case FIL_PAGE_SDI_ZBLOB:
			break;
		default:
			ut_error;
		}

		page_no_t	next_page_no = mach_read_from_4(
			page + FIL_PAGE_NEXT);

		btr_page_free_low(m_ctx.m_index, blob_block, ULINT_UNDEFINED,
				  &m_mtr);

		m_ctx.m_blobref.set_page_no(next_page_no);
		m_ctx.m_blobref.set_length(0);
		page_zip_write_blob_ptr(m_ctx.m_page_zip, m_ctx.m_rec,
					m_ctx.m_index,
					m_ctx.m_offsets, m_ctx.m_field_no,
					&m_mtr);

		/* Commit mtr and release the BLOB block to save memory. */
		btr_blob_free(m_ctx.m_index, blob_block, TRUE, &m_mtr);

		return(err);
	}

private:
	bool can_free() const
	{
		if (m_ctx.m_blobref.is_zero()) {
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

	blob_delete_context_t&	m_ctx;
	mtr_t			m_mtr;
};

/** Determine if an operation on off-page columns is an update.
@param[in]	op	type of BLOB operation.
@return TRUE if op != BTR_STORE_INSERT */
inline
bool
btr_blob_op_is_update(
	enum blob_op	op)
{
	switch (op) {
	case BTR_STORE_INSERT:
	case BTR_STORE_INSERT_BULK:
		return(false);
	case BTR_STORE_INSERT_UPDATE:
	case BTR_STORE_UPDATE:
		return(true);
	}

	ut_ad(0);
	return(FALSE);
}

#endif /* lob0lob_h */
