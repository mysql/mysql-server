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
#ifndef lob0lob_h
#define lob0lob_h

#include "univ.i"
#include "dict0mem.h"
#include "page0page.h"
#include "row0log.h"
#include <my_dbug.h>
#include "btr0pcur.h"
#include "row0upd.h"

/**
@file
@brief Implements the large objects (LOB) module.

InnoDB supports large objects (LOB).  Previously, the LOB was called as
externally stored fields. A large object contains a singly linked list of
database pages, aka LOB pages.  A reference to the first LOB page is stored
along with the clustered index record.  This reference is called the LOB
reference (lob::ref_t). A single clustered index record can have many LOB
references.  Secondary indexes cannot have LOB references.

There are two types of LOB - compressed and uncompressed.

The main operations implemented for LOB are - INSERT, DELETE and FETCH.  To
carry out these main operations the following classes are provided.

Inserter     - for inserting uncompressed LOB data.
zInserter    - for inserting compressed LOB data.
BaseInserter - a base class containing common state and functions useful for
               both Inserter and zInserter.  Inserter and zInserter derives
               from this base class.
Reader       - for reading uncompressed LOB data.
zReader      - for reading compressed LOB data.
Deleter      - for deleting both compressed and uncompressed LOB data.

For each main operation, the context information is identified separately.
They are as follows:

InsertContext - context information for doing insert of LOB. `
DeleteContext - context information for doing delete of LOB. `
ReadContext   - context information for doing fetch of LOB. `

*/

/** Provides the large objects (LOB) module.  Previously, the LOB was called as
externally stored fields. */
namespace lob {

/** The maximum size possible for an LOB */
const ulint MAX_SIZE = UINT32_MAX;

/** The reference in a field for which data is stored on a different page.
The reference is at the end of the 'locally' stored part of the field.
'Locally' means storage in the index record.
We store locally a long enough prefix of each column so that we can determine
the ordering parts of each index record without looking into the externally
stored part. */
/*-------------------------------------- @{ */

/** Space identifier where stored. */
const ulint BTR_EXTERN_SPACE_ID	= 0;

/** page number where stored */
const ulint BTR_EXTERN_PAGE_NO = 4;

/** offset of BLOB header on that page */
const ulint BTR_EXTERN_OFFSET = 8;

/** 8 bytes containing the length of the externally stored part of the LOB.
The 2 highest bits are reserved to the flags below. */
const ulint BTR_EXTERN_LEN = 12;

/*-------------------------------------- @} */

/** The most significant bit of BTR_EXTERN_LEN (i.e., the most
significant bit of the byte at smallest address) is set to 1 if this
field does not 'own' the externally stored field; only the owner field
is allowed to free the field in purge! */
const ulint BTR_EXTERN_OWNER_FLAG = 128UL;

/** If the second most significant bit of BTR_EXTERN_LEN (i.e., the
second most significant bit of the byte at smallest address) is 1 then
it means that the externally stored field was inherited from an
earlier version of the row.  In rollback we are not allowed to free an
inherited external field. */
const ulint BTR_EXTERN_INHERITED_FLAG = 64UL;

/** The structure of uncompressed LOB page header */

/** Offset within header of LOB length on this page. */
const ulint LOB_HDR_PART_LEN		= 0;

/** Offset within header of next BLOB part page no.
FIL_NULL if none */
const ulint LOB_HDR_NEXT_PAGE_NO	= 4;

/** Size of an uncompressed LOB page header, in bytes */
const ulint LOB_HDR_SIZE		= 8;

/** Start of the data on an LOB page */
const uint	ZLOB_PAGE_DATA		= FIL_PAGE_DATA;

/** The struct 'lob::ref_t' represents an external field reference. The
reference in a field for which data is stored on a different page.  The
reference is at the end of the 'locally' stored part of the field.  'Locally'
means storage in the index record. We store locally a long enough prefix of
each column so that we can determine the ordering parts of each index record
without looking into the externally stored part. */
struct ref_t {

public:
	/** Constructor.
	@param[in]	ptr	Pointer to the external field reference. */
	explicit ref_t(byte* ptr): m_ref(ptr)
	{}

	/** Check whether the stored external field reference is equal to the
	given field reference.
	@param[in]	ptr	supplied external field reference. */
	bool is_equal(const byte* ptr) const
	{
		return(m_ref == ptr);
	}

	/** Set the external field reference to the given memory location.
	@param[in]	ptr	the new external field reference. */
	void set_ref(byte* ptr)
	{
		m_ref = ptr;
	}

	/** Initialize the external field reference to zeroes. */
	void set_null()
	{
		memset(m_ref, 0x00, SIZE);
	}

	/** Check if the field reference is made of zeroes.
	@return true if field reference is made of zeroes, false otherwise. */
	bool is_null() const
	{
		return(memcmp(field_ref_zero, m_ref, SIZE) == 0);
	}

	/** Set the ownership flag in the blob reference.
	@param[in]	owner	whether to own or disown.  if owner, unset
				the owner flag.
	@param[in]	mtr	the mini-transaction or NULL.*/
	void set_owner(bool owner, mtr_t* mtr)
	{
		ulint byte_val = mach_read_from_1(m_ref + BTR_EXTERN_LEN);

		if (owner) {
			/* owns the blob */
			byte_val &= ~BTR_EXTERN_OWNER_FLAG;
		} else {
			byte_val |= BTR_EXTERN_OWNER_FLAG;
		}

		mlog_write_ulint(m_ref + BTR_EXTERN_LEN,
				 byte_val, MLOG_1BYTE, mtr);
	}

	/** Set the inherited flag in the field reference.
	@param[in]	inherited	true, if inherited.
	@param[in]	mtr		the mini transaction context.*/
	void set_inherited(bool inherited, mtr_t* mtr)
	{
		ulint byte_val = mach_read_from_1(m_ref + BTR_EXTERN_LEN);

		if (inherited) {
			byte_val |= BTR_EXTERN_INHERITED_FLAG;
		} else {
			byte_val &= ~BTR_EXTERN_INHERITED_FLAG;
		}

		mlog_write_ulint(m_ref + BTR_EXTERN_LEN,
				 byte_val, MLOG_1BYTE, mtr);
	}

	/** Check if the current row is the owner of the blob.
	@return true if owner, false otherwise. */
	bool is_owner() const
	{
		ulint byte_val = mach_read_from_1(
			m_ref + BTR_EXTERN_LEN);
		return(!(byte_val & BTR_EXTERN_OWNER_FLAG));
	}

	/** Check if the current row inherited the blob from parent row.
	@return true if inherited, false otherwise. */
	bool is_inherited() const
	{
		const ulint byte_val = mach_read_from_1(
			m_ref + BTR_EXTERN_LEN);
		return (byte_val & BTR_EXTERN_INHERITED_FLAG);
	}

	/** Read the space id from the blob reference.
	@return the space id */
	space_id_t space_id() const
	{
		return(mach_read_from_4(m_ref));
	}

	/** Read the page number from the blob reference.
	@return the page number */
	page_no_t page_no() const
	{
		return(mach_read_from_4(m_ref + BTR_EXTERN_PAGE_NO));
	}

	/** Read the offset of blob header from the blob reference.
	@return the offset of the blob header */
	ulint offset() const
	{
		return(mach_read_from_4(m_ref + BTR_EXTERN_OFFSET));
	}

	/** Read the length from the blob reference.
	@return length of the blob */
	ulint length() const
	{
		return(mach_read_from_4(m_ref + BTR_EXTERN_LEN + 4));
	}

	/** Update the information stored in the external field reference.
	@param[in]	space_id	the space identifier.
	@param[in]	page_no		the page number.
	@param[in]	offset		the offset within the page_no
	@param[in]	mtr		the mini trx or NULL. */
	void update(
		space_id_t	space_id,
		ulint		page_no,
		ulint		offset,
		mtr_t*		mtr)
	{
		set_space_id(space_id, mtr);
		set_page_no(page_no, mtr);
		set_offset(offset, mtr);
	}

	/** Set the space_id in the external field reference.
	@param[in]	space_id	the space identifier.
	@param[in]	mtr		mini-trx or NULL. */
	void set_space_id(const space_id_t space_id, mtr_t* mtr)
	{
		mlog_write_ulint(m_ref + BTR_EXTERN_SPACE_ID,
				 space_id, MLOG_4BYTES, mtr);
	}

	/** Set the page number in the external field reference.
	@param[in]	page_no	the page number .
	@param[in]	mtr	mini-trx or NULL. */
	void set_page_no(const ulint page_no, mtr_t* mtr)
	{
		mlog_write_ulint(m_ref + BTR_EXTERN_PAGE_NO,
				 page_no, MLOG_4BYTES, mtr);
	}

	/** Set the offset information in the external field reference.
	@param[in]	offset	the offset.
	@param[in]	mtr	mini-trx or NULL. */
	void set_offset(const ulint offset, mtr_t* mtr)
	{
		mlog_write_ulint(m_ref + BTR_EXTERN_OFFSET,
				 offset, MLOG_4BYTES, mtr);
	}

	/** Set the length of blob in the external field reference.
	@param[in]	len	the blob length .
	@param[in]	mtr	mini-trx or NULL. */
	void set_length(const ulint len, mtr_t* mtr)
	{
		ut_ad(len <= MAX_SIZE);
		mlog_write_ulint(m_ref + BTR_EXTERN_LEN,
				 0, MLOG_4BYTES, mtr);
		mlog_write_ulint(m_ref + BTR_EXTERN_LEN + 4,
				 len, MLOG_4BYTES, mtr);
	}

	/** Get the start of a page containing this blob reference.
	@return start of the page */
	page_t*	page_align() const
	{
		return(::page_align(m_ref));
	}

	/** The size of an LOB reference object (in bytes) */
	static const uint SIZE = BTR_EXTERN_FIELD_REF_SIZE;

private:
	/** Pointing to a memory of size BTR_EXTERN_FIELD_REF_SIZE */
	byte	*m_ref;
};

#ifdef UNIV_DEBUG
/** Overload the global output stream operator to easily print the
lob::ref_t object into the output stream.
@param[in,out]	out		the output stream.
@param[in]	blobref		the lob::ref_t object to be printed
@return the output stream. */
inline
std::ostream& operator<<(std::ostream& out, const ref_t& blobref)
{
	out << "[ref_t: space_id=" << blobref.space_id() << ", page_no="
		<< blobref.page_no() << ", offset=" << blobref.offset()
		<< ", length=" << blobref.length() << "]";
	return(out);
}
#endif /* UNIV_DEBUG */

/** LOB operation code for btr_store_big_rec_extern_fields(). */
enum opcode {
	/** Store off-page columns for a freshly inserted record */
	OPCODE_INSERT = 0,
	/** Store off-page columns for an insert by update */
	OPCODE_INSERT_UPDATE,
	/** Store off-page columns for an update */
	OPCODE_UPDATE,
	/** Store off-page columns for a freshly inserted record by bulk */
	OPCODE_INSERT_BULK,
	/** The operation code is unknown or not important. */
	OPCODE_UNKNOWN
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
	opcode			op)
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
blob_free(
	dict_index_t*	index,
	buf_block_t*	block,
	bool		all,
	mtr_t*		mtr);

/** The B-tree context under which the LOB operation is done. */
class BtrContext
{
public:
	/** Default Constructor */
	BtrContext()
	:
	m_mtr(NULL),
	m_pcur(NULL),
	m_index(NULL),
	m_rec(NULL),
	m_offsets(NULL),
	m_block(NULL),
	m_op(OPCODE_UNKNOWN),
	m_btr_page_no(FIL_NULL)
	{}

	/** Constructor **/
	BtrContext(
		mtr_t*		mtr,
		btr_pcur_t*	pcur,
		dict_index_t*	index,
		rec_t*		rec,
		ulint*		offsets,
		buf_block_t*	block)
	:
	m_mtr(mtr),
	m_pcur(pcur),
	m_index(index),
	m_rec(rec),
	m_offsets(offsets),
	m_block(block),
	m_op(OPCODE_UNKNOWN),
	m_btr_page_no(FIL_NULL)
	{
		ut_ad(m_pcur == NULL || rec_offs_validate());
		ut_ad(m_block == NULL || m_rec == NULL
		      || m_block->frame == page_align(m_rec));
		ut_ad(m_pcur == NULL || m_rec == btr_pcur_get_rec(m_pcur));
	}

	/** Constructor **/
	BtrContext(
		mtr_t*		mtr,
		btr_pcur_t*	pcur,
		dict_index_t*	index,
		rec_t*		rec,
		ulint*		offsets,
		buf_block_t*	block,
		opcode		op)
	:
	m_mtr(mtr),
	m_pcur(pcur),
	m_index(index),
	m_rec(rec),
	m_offsets(offsets),
	m_block(block),
	m_op(op),
	m_btr_page_no(FIL_NULL)
	{
		ut_ad(m_pcur == NULL || rec_offs_validate());
		ut_ad(m_block->frame == page_align(m_rec));
		ut_ad(m_pcur == NULL || m_rec == btr_pcur_get_rec(m_pcur));
	}

	/** Copy Constructor **/
	BtrContext(const BtrContext& other)
	:
	m_mtr(other.m_mtr),
	m_pcur(other.m_pcur),
	m_index(other.m_index),
	m_rec(other.m_rec),
	m_offsets(other.m_offsets),
	m_block(other.m_block),
	m_op(other.m_op),
	m_btr_page_no(other.m_btr_page_no)
	{}

	/** Marks non-updated off-page fields as disowned by this record.
	The ownership must be transferred to the updated record which is
	inserted elsewhere in the index tree. In purge only the owner of
	externally stored field is allowed to free the field.
	@param[in]	update		update vector. */
	void disown_inherited_fields(const upd_t* update)
	{
		ut_ad(rec_offs_validate());
		ut_ad(!rec_offs_comp(m_offsets)
		      || !rec_get_node_ptr_flag(m_rec));
		ut_ad(rec_offs_any_extern(m_offsets));
		ut_ad(m_mtr);

		for (ulint i = 0; i < rec_offs_n_fields(m_offsets); i++) {

			if (rec_offs_nth_extern(m_offsets, i)
			    && !upd_get_field_by_field_no(update, i, false)) {
				set_ownership_of_extern_field(i, FALSE);
			}
		}
	}

	/** Sets the ownership bit of an externally stored field in a record.
	@param[in]		i		field number
	@param[in]		val		value to set */
	void
	set_ownership_of_extern_field(
		ulint	i,
		ibool	val)
	{
		byte*	data;
		ulint	local_len;

		data = rec_get_nth_field(m_rec, m_offsets, i, &local_len);
		ut_ad(rec_offs_nth_extern(m_offsets, i));
		ut_a(local_len >= BTR_EXTERN_FIELD_REF_SIZE);

		local_len -= BTR_EXTERN_FIELD_REF_SIZE;

		ref_t	ref(data + local_len);

		ut_a(val || ref.is_owner());

		page_zip_des_t*	page_zip = get_page_zip();
		if (page_zip) {
			ref.set_owner(val, NULL);
			page_zip_write_blob_ptr(page_zip, m_rec, m_index,
						m_offsets, i, m_mtr);
		} else {
			ref.set_owner(val, m_mtr);
		}
	}

	/** Marks all extern fields in a record as owned by the record.
	This function should be called if the delete mark of a record is
	removed: a not delete marked record always owns all its extern
	fields.*/
	void unmark_extern_fields()
	{
		ut_ad(!rec_offs_comp(m_offsets)
		      || !rec_get_node_ptr_flag(m_rec));

		ulint n = rec_offs_n_fields(m_offsets);

		if (!rec_offs_any_extern(m_offsets)) {

			return;
		}

		for (ulint i = 0; i < n; i++) {
			if (rec_offs_nth_extern(m_offsets, i)) {

				set_ownership_of_extern_field(i, TRUE);
			}
		}
	}

	/** Frees the externally stored fields for a record.
	@param[in]	rollback	performing rollback? */
	void free_externally_stored_fields(bool rollback);

	/** Frees the externally stored fields for a record, if the field
	is mentioned in the update vector.
	@param[in]	update		update vector
	@param[in]	rollback	performing rollback? */
	void free_updated_extern_fields(
		const upd_t*	update,
		bool		rollback);

	/** Gets the compressed page descriptor
	@return the compressed page descriptor. */
	page_zip_des_t*	get_page_zip() const
	{
		return(buf_block_get_page_zip(m_block));
	}

	/** Get the page number of clustered index block.
	@return the page number. */
	page_no_t get_page_no() const
	{
		return(page_get_page_no(buf_block_get_frame(m_block)));
	}

	/** Get the record offset within page of the clustered index record.
	@return the record offset. */
	ulint get_rec_offset() const
	{
		return(page_offset(m_rec));
	}

	/** Get the clustered index record pointer.
	@return clustered index record pointer. */
	rec_t*	rec() const
	{
		ut_ad(m_pcur == NULL || m_rec == btr_pcur_get_rec(m_pcur));
		return(m_rec);
	}

#ifdef UNIV_DEBUG
	/** Validate the current BLOB context object.  The BLOB context object
	is valid if the necessary latches are being held by the
	mini-transaction of the B-tree (btr mtr).  Does not return if the
	validation fails.
	@return true if valid */
	bool validate() const
	{
		rec_offs_make_valid(rec(), index(), m_offsets);

		ut_ad(m_mtr->memo_contains_page_flagged(
				m_rec,
				MTR_MEMO_PAGE_X_FIX | MTR_MEMO_PAGE_SX_FIX)
				|| table()->is_intrinsic());

		ut_ad(mtr_memo_contains_flagged(m_mtr,
				dict_index_get_lock(index()),
				MTR_MEMO_SX_LOCK | MTR_MEMO_X_LOCK)
				|| table()->is_intrinsic());

		return(true);
	}

	/** Check to see if all pointers to externally stored columns in
	the record must be valid.
	@return true if all blob references are valid.
	@return will not return if any blob reference is invalid. */
	bool	are_all_blobrefs_valid() const
	{
		for (ulint i = 0; i < rec_offs_n_fields(m_offsets); i++) {

			if (!rec_offs_nth_extern(m_offsets, i)) {
				continue;
			}

			byte* field_ref = btr_rec_get_field_ref(
				rec(), m_offsets, i);

			ref_t	blobref(field_ref);

			/* The pointer must not be zero if the operation
			succeeded. */
			ut_a(!blobref.is_null());

			/* The column must not be disowned by this record. */
			ut_a(blobref.is_owner());
		}

		return(true);
	}
#endif /* UNIV_DEBUG */

	/** Determine whether current operation is a bulk insert operation.
	@return true, if bulk insert operation, false otherwise. */
	bool is_bulk() const
	{
                return(m_op == OPCODE_INSERT_BULK);
	}

	/** Get the beginning of the B-tree clustered index page frame
	that contains the current clustered index record (m_rec).
	@return the page frame containing the clust rec. */
	const page_t*	rec_frame() const
	{
		ut_ad(m_block->frame == page_align(m_rec));
		return(m_block->frame);
	}

	/** Commit the mini transaction that is holding the latches
	of the clustered index record block. */
	void commit_btr_mtr()
	{
		m_mtr->commit();
	}

	/** Start the mini transaction that will be holding the latches
	of the clustered index record block. */
	void start_btr_mtr()
	{
		mtr_log_t log_mode = m_mtr->get_log_mode();
		m_mtr->start();
		m_mtr->set_log_mode(log_mode);
	}

#ifndef UNIV_HOTBACKUP
	/** Increment the buffer fix count of the clustered index record
	block. */
	void rec_block_fix()
	{
		m_rec_offset = page_offset(m_rec);
		m_btr_page_no = page_get_page_no(
			buf_block_get_frame(m_block));
		buf_block_buf_fix_inc(m_block, __FILE__, __LINE__);
	}

	/** Decrement the buffer fix count of the clustered index record
	block. */
	void rec_block_unfix()
	{
		space_id_t	space_id = space();
		page_id_t       page_id(space_id, m_btr_page_no);
		page_size_t     page_size(dict_table_page_size(table()));
		page_cur_t*	page_cur = &m_pcur->btr_cur.page_cur;

		mtr_x_lock(dict_index_get_lock(index()), m_mtr);

		page_cur->block = btr_block_get(page_id, page_size,
						RW_X_LATCH, index(),
						m_mtr);
		page_cur->rec = buf_block_get_frame(page_cur->block)
			+ m_rec_offset;
		buf_block_buf_fix_dec(page_cur->block);

		recalc();
	}
#endif  /* !UNIV_HOTBACKUP */

	/** Restore the position of the persistent cursor. */
	void restore_position()
	{
		ut_ad(m_pcur->rel_pos == BTR_PCUR_ON);
		bool ret = btr_pcur_restore_position(
			BTR_MODIFY_LEAF | BTR_MODIFY_EXTERNAL,
			m_pcur, m_mtr);

		ut_a(ret);

		recalc();
	}

	/** Get the index object.
	@return index object */
	dict_index_t*	index() const
	{
		return(m_index);
	}

	/** Get the table object.
	@return table object or NULL. */
	dict_table_t*	table() const
	{
		dict_table_t*	result = nullptr;

		if (m_pcur != nullptr && m_pcur->index() != nullptr) {
			result = m_pcur->index()->table;
		}

		return(result);
	}

	/** Get the space id.
	@return space id. */
	space_id_t	space() const
	{
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
	page_no_t pages_in_extent() const
	{
		return(dict_table_extent_size(table()));
	}

	/** Check if there is enough space in the redo log file.  The btr
	mini transaction will be restarted. */
	void check_redolog()
	{
		is_bulk() ? check_redolog_bulk() : check_redolog_normal();
	}

	/** Mark the nth field as externally stored.
	@param[in]	field_no	the field number. */
	void make_nth_extern(ulint field_no)
	{
		rec_offs_make_nth_extern(m_offsets, field_no);
	}

	/** Get the log mode of the btr mtr.
	@return the log mode. */
	mtr_log_t get_log_mode()
	{
		return(m_mtr->get_log_mode());
	}

	/** Get flush observer
	@return flush observer */
	FlushObserver* get_flush_observer() const
	{
		return(m_mtr->get_flush_observer());
	}

	/** Get the record offsets array.
	@return the record offsets array. */
	ulint* get_offsets() const
	{
		return(m_offsets);
	}

	/** Validate the record offsets array.
	@return true if validation succeeds, false otherwise. */
	bool rec_offs_validate() const
	{
		if (m_rec != NULL) {
			ut_ad(::rec_offs_validate(m_rec, m_index, m_offsets));
		}
		return(true);
	}

	/** Get the associated mini-transaction.
	@return the mini transaction. */
	mtr_t* get_mtr()
	{
		return(m_mtr);
	}

	/** Get the pointer to the clustered record block.
	@return pointer to the clustered rec block. */
	buf_block_t* block() const
	{
		return(m_block);
	}

	/** Save the position of the persistent cursor. */
	void store_position()
	{
		btr_pcur_store_position(m_pcur, m_mtr);
	}

	/** Check if there is enough space in log file. Commit and re-start the
	mini transaction. */
	void check_redolog_normal();

	/** When bulk load is being done, check if there is enough space in redo
	log file. */
	void check_redolog_bulk();

	/** Recalculate some of the members after restoring the persistent
	cursor. */
	void recalc()
	{
		m_block	= btr_pcur_get_block(m_pcur);
		m_rec = btr_pcur_get_rec(m_pcur);
		m_btr_page_no = page_get_page_no(buf_block_get_frame(m_block));
		m_rec_offset = page_offset(m_rec);

		rec_offs_make_valid(rec(), index(), m_offsets);
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
					m_rec,
					index(),
					m_offsets,
					field_no, mtr);
	}

	mtr_t*		m_mtr;
	btr_pcur_t*	m_pcur;
	dict_index_t*	m_index;
	rec_t*		m_rec;
	ulint*		m_offsets;
	buf_block_t*	m_block;
	opcode		m_op;

	/** Record offset within the page. */
	ulint		m_rec_offset;

	/** Page number where the clust rec is present. */
	page_no_t	m_btr_page_no;
};

/** The context for a LOB operation.  It contains the necessary information
to carry out a LOB operation. */
struct InsertContext : public BtrContext
{
	/** Constructor
	@param[in]	btr_ctx		b-tree context for lob operation.
	@param[in]	big_rec_vec	array of blobs */
	InsertContext(
		const BtrContext&	btr_ctx,
		const big_rec_t*	big_rec_vec)
		:
		BtrContext(btr_ctx),
		m_big_rec_vec(big_rec_vec)
	{}

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

	/** The B-tree Context */
	// const BtrContext	m_btr_ctx;

	/** vector containing fields to be stored externally */
	const big_rec_t*	m_big_rec_vec;
};

/** Information about data stored in one BLOB page. */
struct blob_page_info_t
{
	/** Constructor.
	@param[in]	page_no		the BLOB page number.
	@param[in]	bytes		amount of uncompressed BLOB data
					in BLOB page in bytes.
	@param[in]	zbytes		amount of compressed BLOB data
					in BLOB page in bytes. */
	blob_page_info_t(page_no_t page_no, uint bytes, uint zbytes)
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
	void set_compressed_size(uint bytes)
	{
		m_zbytes = bytes;
	}

	/** Set the uncompressed data size in bytes.
	@param[in]	bytes	the new uncompressed data size. */
	void set_uncompressed_size(uint bytes)
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
	uint		m_bytes;

	/** Amount of compressed data (in bytes) in the BLOB page. */
	uint		m_zbytes;
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

	/** A vector of blob pages along with its metadata. */
	std::vector<blob_page_info_t>	m_pages;
};

/** Overloading the global output operator to print the blob_dir_t
object into an output stream.
@param[in,out]	out	the output stream.
@param[in]	obj	the object to be printed.
@return the output stream. */
inline
std::ostream&
operator<<(std::ostream& out, const blob_dir_t& obj)
{
	return(obj.print(out));
}

/** This struct can hold BLOB routines/functions, and state variables,
that are common for compressed and uncompressed BLOB. */
struct BaseInserter
{
	/** Constructor.
	@param[in]	ctx	blob operation context. */
	BaseInserter(InsertContext* ctx)
	:
	m_ctx(ctx),
	m_status(DB_SUCCESS),
	m_prev_page_no(ctx->get_page_no()),
	m_cur_blob_block(NULL),
	m_cur_blob_page_no(FIL_NULL)
	{}

	/** Start the BLOB mtr.
	@return pointer to the BLOB mtr. */
	mtr_t* start_blob_mtr()
	{
		mtr_start(&m_blob_mtr);
		m_blob_mtr.set_log_mode(m_ctx->get_log_mode());
		m_blob_mtr.set_flush_observer(m_ctx->get_flush_observer());
		return(&m_blob_mtr);
	}

	/** Allocate one BLOB page.
	@return the allocated block of the BLOB page. */
	buf_block_t* alloc_blob_page();

	/** Get the previous BLOB page frame.  This will return a BLOB page.
	It should not be called for the first BLOB page, because it will not
	have a previous BLOB page.
	@return	the previous BLOB page frame. */
	page_t*	get_previous_blob_page();

	/** Get the previous BLOB page block.  This will return a BLOB block.
	It should not be called for the first BLOB page, because it will not
	have a previous BLOB page.
	@return	the previous BLOB block. */
	buf_block_t* get_previous_blob_block();

	/** Check if the index is SDI index
	@return true if index is SDI index else false */
	bool is_index_sdi()
	{
		return(dict_index_is_sdi(m_ctx->index()));
	}

	/** Get the current BLOB page frame.
	@return the current BLOB page frame. */
	page_t*	cur_page() const
	{
		return(buf_block_get_frame(m_cur_blob_block));
	}

protected:
	/** The BLOB operation context */
	InsertContext*	m_ctx;

	/** Success or failure status of the operation so far. */
	dberr_t		m_status;

	/** The mini trx used to write into blob pages */
	mtr_t		m_blob_mtr;

	/** The previous BLOB page number.  This is needed to maintain
	the linked list of BLOB pages. */
	page_no_t	m_prev_page_no;

	/** The current BLOB buf_block_t object. */
	buf_block_t*	m_cur_blob_block;

	/** The current BLOB page number. */
	page_no_t	m_cur_blob_page_no;
};

/** Insert or write an uncompressed BLOB */
class Inserter : private BaseInserter
{
public:
	/** Constructor.
	@param[in]	ctx	blob operation context. */
	Inserter(InsertContext* ctx)
	:
	BaseInserter(ctx)
	{}

	/** Destructor. */
	~Inserter()
	{}

	/** Write all the BLOBs of the clustered index record.
	@return DB_SUCCESS on success, error code on failure. */
	dberr_t write();

	/** Write one blob field data.
	@param[in]	blob_j	the blob field number
	@return DB_SUCCESS on success, error code on failure. */
	dberr_t write_one_blob(size_t blob_j);

	/** Write one blob page.  This function will be repeatedly called
	with an increasing nth_blob_page to completely write a BLOB.
	@param[in]	blob_j		the jth blob object of the record.
	@param[in]	field		the big record field.
	@param[in]	nth_blob_page	count of the BLOB page (starting from 1).
	@return DB_SUCCESS or DB_FAIL. */
	dberr_t	write_single_blob_page(
		size_t			blob_j,
		big_rec_field_t&	field,
		ulint			nth_blob_page);

	/** Check if the BLOB operation has reported any errors.
	@return	true if BLOB operation is successful, false otherwise. */
	bool is_ok() const
	{
		return(m_status == DB_SUCCESS);
	}

	/** Make the current page as next page of previous page.  In other
	words, make the page m_cur_blob_page_no as the next page of page
	m_prev_page_no. */
	void set_page_next();

	/** Write the page type of the current BLOB page and also generate the
	redo log record. */
	void log_page_type()
	{
		page_type_t	page_type;
		page_t*		blob_page = cur_page();

		if (is_index_sdi()) {
			page_type = FIL_PAGE_SDI_BLOB;
		} else {
			page_type = FIL_PAGE_TYPE_BLOB;
		}

		mlog_write_ulint(blob_page + FIL_PAGE_TYPE, page_type,
				 MLOG_2BYTES, &m_blob_mtr);
	}

	/** Calculate the payload size of the BLOB page.
	@return	payload size in bytes. */
	ulint	payload() const
	{
		const page_size_t page_size = m_ctx->page_size();
		const ulint	payload_size
			= page_size.physical() - FIL_PAGE_DATA
			- LOB_HDR_SIZE - FIL_PAGE_DATA_END;
		return(payload_size);
	}

	/** Write contents into a single BLOB page.
	@param[in]	field		the big record field. */
	void write_into_single_page(big_rec_field_t&	field);

	/** Write first blob page.
	@param[in]	blob_j	the jth blob object of the record.
	@param[in]	field	the big record field.
	@return DB_SUCCESS on success. */
	dberr_t
	write_first_page(
		size_t			blob_j,
		big_rec_field_t&	field);

private:
#ifdef UNIV_DEBUG
	/** The BLOB directory information. */
	blob_dir_t	m_dir;
#endif /* UNIV_DEBUG */

	/** Data remaining to be written. */
	ulint		m_remaining;
};

/** The context information for reading a single BLOB */
struct ReadContext
{
	/** Constructor
	@param[in]	page_size	page size information.
	@param[in]	data		'internally' stored part of the field
					containing also the reference to the
					external part; must be protected by
					a lock or a page latch.
	@param[in]	prefix_len	length of BLOB data stored inline in
					the clustered index record, including
					the blob reference.
	@param[out]	buf		the output buffer.
	@param[in]	len		the output buffer length.
	@param[in]	is_sdi		true for SDI Indexes. */
	ReadContext(
		const page_size_t&	page_size,
		const byte*		data,
		ulint			prefix_len,
		byte*			buf,
		ulint			len
#ifdef UNIV_DEBUG
		, bool		is_sdi
#endif /* UNIV_DEBUG */
		)
	:
	m_page_size(page_size),
	m_data(data),
	m_local_len(prefix_len),
	m_blobref(const_cast<byte*>(data) + prefix_len
		  - BTR_EXTERN_FIELD_REF_SIZE),
	m_buf(buf),
	m_len(len)
#ifdef UNIV_DEBUG
	, m_is_sdi(is_sdi)
#endif /* UNIV_DEBUG */
	{
		read_blobref();
	}

	/** Read the space_id, page_no and offset information from the BLOB
	reference object and update the member variables. */
	void read_blobref()
	{
		m_space_id	= m_blobref.space_id();
		m_page_no	= m_blobref.page_no();
		m_offset	= m_blobref.offset();
	}

	/** Check if the BLOB reference is valid.  For this particular check,
	if the length of the BLOB is greater than 0, it is considered
	valid.
	@return true if valid. */
	bool is_valid_blob() const
	{
		return(m_blobref.length() > 0);
	}

	/** The page size information. */
	const page_size_t&	m_page_size;

	/** The 'internally' stored part of the field containing also the
	reference to the external part; must be protected by a lock or a page
	latch */
	const byte*	m_data;

	/** Length (in bytes) of BLOB prefix stored inline in clustered
	index record. */
	ulint	m_local_len;

	/** The blob reference of the blob that is being read. */
	const ref_t	m_blobref;

	/** Buffer into which data is read. */
	byte*	m_buf;

	/** Length of the buffer m_buf. */
	ulint	m_len;

	/** The identifier of the space in which blob is available. */
	space_id_t	m_space_id;

	/** The page number obtained from the blob reference. */
	page_no_t	m_page_no;

	/** The offset information obtained from the blob reference. */
	ulint	m_offset;

#ifdef UNIV_DEBUG
	/** Is it a space dictionary index (SDI)?
	@return true if SDI, false otherwise. */
	bool is_sdi() const
	{
		return(m_is_sdi);
	}

	/** Is it a tablespace dictionary index (SDI)? */
	const bool	m_is_sdi;
#endif /* UNIV_DEBUG */
};

/** Fetch compressed BLOB */
struct zReader
{
	/** Constructor. */
	explicit zReader(const ReadContext&	ctx)
	:
	m_rctx(ctx)
	{}

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

private:
	/** Do setup of the zlib stream.
	@return code returned by zlib. */
	int setup_zstream();

#ifdef UNIV_DEBUG
	/** Assert that the local prefix is empty.  For compressed row format,
	there is no local prefix stored.  This function doesn't return if the
	local prefix is non-empty.
	@return true if local prefix is empty*/
	bool assert_empty_local_prefix();
#endif /* UNIV_DEBUG */

	ReadContext	m_rctx;

	/** Bytes yet to be read. */
	ulint	m_remaining;

	/** The zlib stream used to uncompress while fetching blob. */
	z_stream	m_stream;

	/** The memory heap that will be used by zlib allocator. */
	mem_heap_t*	m_heap;

	/* There is no latch on m_bpage directly.  Instead,
	m_bpage is protected by the B-tree page latch that
	is being held on the clustered index record, or,
	in row_merge_copy_blobs(), by an exclusive table lock. */
	buf_page_t*	m_bpage;

#ifdef UNIV_DEBUG
	/** The expected page type. */
	ulint	m_page_type_ex;
#endif /* UNIV_DEBUG */
};

/** Fetch uncompressed BLOB */
struct Reader
{
	/** Constructor. */
	Reader(const ReadContext&	ctx)
	:
	m_rctx(ctx),
	m_cur_block(NULL),
	m_copied_len(0)
	{}

	/** Fetch the complete or prefix of the uncompressed LOB data.
	@return bytes of LOB data fetched. */
	ulint fetch();

	/** Fetch one BLOB page. */
	void fetch_page();

	ReadContext	m_rctx;

	/** Buffer block of the current BLOB page */
	buf_block_t*	m_cur_block;

	/** Total bytes of LOB data that has been copied from multiple
	LOB pages. This is a cumulative value.  When this value reaches
	m_rctx.m_len, then the read operation is completed. */
	ulint		m_copied_len;
};

/** The context information when the delete operation on LOB is
taking place. */
struct DeleteContext : public BtrContext
{
	DeleteContext(byte *field_ref)
	:
	m_blobref(field_ref),
	m_page_size(table() == nullptr ? get_page_size()
		: dict_table_page_size(table()))
	{}

	DeleteContext(
		const BtrContext&	btr,
		byte*			field_ref,
		ulint			field_no,
		bool			rollback)
	:
	BtrContext(btr),
	m_blobref(field_ref),
	m_field_no(field_no),
	m_rollback(rollback),
	m_page_size(table() == nullptr ? get_page_size()
		: dict_table_page_size(table()))
	{}

	/** Determine if it is compressed page format.
	@return true if compressed. */
	bool is_compressed() const
	{
		return(m_page_size.is_compressed());
	}

	/** Check if tablespace supports atomic blobs.
	@return true if tablespace has atomic blobs. */
	bool has_atomic_blobs() const
	{
		space_id_t	space_id = m_blobref.space_id();
		ulint	flags = fil_space_get_flags(space_id);
		return(DICT_TF_HAS_ATOMIC_BLOBS(flags));
	}

#ifdef UNIV_DEBUG
	/** Validate the LOB reference object.
	@return true if valid, false otherwise. */
	bool validate_blobref() const
	{
		rec_t*	clust_rec = rec();
		if (clust_rec != NULL) {
			const byte*	v2 = btr_rec_get_field_ref(
				clust_rec, get_offsets(), m_field_no);

			ut_ad(m_blobref.is_equal(v2));
		}
		return(true);
	}


#endif /* UNIV_DEBUG */

	/** the BLOB reference or external field reference. */
	ref_t	m_blobref;

	/** field number of externally stored column; ignored if rec == NULL */
	ulint	m_field_no;

	/** Is this operation part of rollback? */
	bool	m_rollback;

	page_size_t	m_page_size;

private:
	/** Obtain the page size from the tablespace flags.
	@return the page size. */
	page_size_t get_page_size() const
	{
		bool	found;
		space_id_t	space_id = m_blobref.space_id();
		const page_size_t& tmp = fil_space_get_page_size(
			space_id, &found);
		ut_ad(found);
		return(tmp);
	}
};

/* Delete a LOB */
class Deleter
{
public:
	/** Constructor */
	Deleter(DeleteContext&	ctx): m_ctx(ctx)
	{
		ut_ad(ctx.index()->is_clustered());
		ut_ad(mtr_memo_contains_flagged(
				ctx.get_mtr(),
				dict_index_get_lock(ctx.index()),
				MTR_MEMO_X_LOCK | MTR_MEMO_SX_LOCK)
		      || ctx.table()->is_intrinsic());
		ut_ad(mtr_is_page_fix(ctx.get_mtr(),
				      ctx.m_blobref.page_align(),
				      MTR_MEMO_PAGE_X_FIX, ctx.table()));
		ut_ad(ctx.rec_offs_validate());
		ut_ad(ctx.validate_blobref());
	}

	/** Free the LOB object.
	@return DB_SUCCESS on success. */
	dberr_t	destroy();

	/** Free the first page of the BLOB and update the BLOB reference
	in the clustered index.
	@return DB_SUCCESS on pass, error code on failure. */
	dberr_t	free_first_page();

private:
	/* Obtain an x-latch on the clustered index record page.*/
	void x_latch_rec_page();

	bool validate_page_type(const page_t*	page) const
	{
		return(m_ctx.is_compressed()
			? zblob_validate_page_type(page)
			: blob_validate_page_type(page));
	}

	bool zblob_validate_page_type(const page_t*	page) const
	{
		const page_type_t	pt = fil_page_get_type(page);
		switch (pt) {
		case FIL_PAGE_TYPE_ZBLOB:
		case FIL_PAGE_TYPE_ZBLOB2:
		case FIL_PAGE_TYPE_ZBLOB3:
		case FIL_PAGE_SDI_ZBLOB:
			break;
		default:
			ut_error;
		}
		return(true);
	}

	bool blob_validate_page_type(const page_t*	page) const
	{
		const page_type_t	type = fil_page_get_type(page);

		switch (type) {
		case FIL_PAGE_TYPE_BLOB:
		case FIL_PAGE_SDI_BLOB:
		break;
		default:
#ifndef UNIV_DEBUG /* Improve debug test coverage */
			if (!m_ctx.has_atomic_blobs()) {
				/* Old versions of InnoDB did not initialize
				FIL_PAGE_TYPE on BLOB pages.  Do not print
				anything about the type mismatch when reading
				a BLOB page that may be from old versions. */
				return(true);
			}
#endif /* !UNIV_DEBUG */
			ut_error;
		}
		return(true);
	}

	/** Check if the BLOB can be freed.
	@return true if the BLOB can be freed, false otherwise. */
	bool can_free() const;

	DeleteContext&	m_ctx;
	mtr_t		m_mtr;
};

/** Determine if an operation on off-page columns is an update.
@param[in]	op	type of BLOB operation.
@return TRUE if op != OPCODE_INSERT */
inline
bool
btr_lob_op_is_update(
	opcode	op)
{
	switch (op) {
	case OPCODE_INSERT:
	case OPCODE_INSERT_BULK:
		return(false);
	case OPCODE_INSERT_UPDATE:
	case OPCODE_UPDATE:
		return(true);
	case OPCODE_UNKNOWN:
		break;
	}

	ut_ad(0);
	return(FALSE);
}

#ifdef UNIV_DEBUG
# define btr_copy_externally_stored_field_prefix(		\
		buf, len, page_size, data, is_sdi, local_len)	\
	btr_copy_externally_stored_field_prefix_func(		\
		buf, len, page_size, data, is_sdi, local_len)

# define btr_copy_externally_stored_field(			\
		len, data, page_size, local_len, is_sdi, heap)	\
	btr_copy_externally_stored_field_func(			\
		len, data, page_size, local_len, is_sdi, heap)

#else /* UNIV_DEBUG */
# define btr_copy_externally_stored_field_prefix(		\
		buf, len, page_size, data, is_sdi, local_len)	\
	btr_copy_externally_stored_field_prefix_func(		\
		buf, len, page_size, data, local_len)

# define btr_copy_externally_stored_field(			\
		len, data, page_size, local_len, is_sdi, heap)	\
	btr_copy_externally_stored_field_func(			\
		len, data, page_size, local_len, heap)
#endif /* UNIV_DEBUG */

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
	ulint			local_len);

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
	mem_heap_t*		heap);

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
	mem_heap_t*	heap);

/** Gets the externally stored size of a record, in units of a database page.
@param[in]	rec	record
@param[in]	offsets	array returned by rec_get_offsets()
@return externally stored part, in units of a database page */
ulint
btr_rec_get_externally_stored_len(
	const rec_t*	rec,
	const ulint*	offsets);

} // namespace lob

#endif /* lob0lob_h */
