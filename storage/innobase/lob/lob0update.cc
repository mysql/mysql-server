/*****************************************************************************

Copyright (c) 2016, 2017, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

#include "table.h"
#include "field.h"
#include "lob0impl.h"
#include "lob0lob.h"
#include "row0upd.h"
#include "trx0trx.h"
#include "lob0pages.h"
#include "lob0index.h"
#include "my_dbug.h"

namespace lob {

/** Replace a large object (LOB) with the given new data of equal length.
@param[in]	ctx		replace operation context.
@param[in]	trx		the transaction that is doing the read.
@param[in]	index		the clustered index containing the LOB.
@param[in]	ref		the LOB reference identifying the LOB.
@param[in]	first_page	the first page of the LOB.
@param[in]	offset		replace the LOB from the given offset.
@param[in]	len		the length of LOB data that needs to be
				replaced.
@param[in]	buf		the buffer (owned by caller) with new data
				(len bytes).
@param[in]	count		number of replace done on current LOB.
@return DB_SUCCESS on success, error code on failure. */
dberr_t
replace(
	InsertContext&	ctx,
	trx_t*		trx,
	dict_index_t*	index,
	ref_t		ref,
	first_page_t&	first_page,
	ulint		offset,
	ulint		len,
	byte*		buf,
	int		count);

#ifdef UNIV_DEBUG
/** Print an information message in the server log file, informing
that the LOB partial update feature code is hit.
@param[in]	uf	the update field information
@param[in]	index	index where partial update happens.*/
static
void
print_partial_update_hit(
	upd_field_t*	uf,
	dict_index_t*	index)
{
	ib::info() << "LOB partial update of field=("
		<< uf->mysql_field->field_name << ") on index=("
		<< index->name << ") in table=(" << index->table_name << ")";
}
#endif /* UNIV_DEBUG */

/** Update a portion of the given LOB.
@param[in] trx       the transaction that is doing the modification.
@param[in] index     the clustered index containing the LOB.
@param[in] upd       update vector
@param[in] field_no  the LOB field number
@return DB_SUCCESS on success, error code on failure. */
dberr_t
update(
	InsertContext&	ctx,
	trx_t*		trx,
	dict_index_t*	index,
	const upd_t*	upd,
	ulint		field_no,
	ref_t		blobref)
{
	DBUG_ENTER("lob::update");
	dberr_t	err = DB_SUCCESS;
	mtr_t*	mtr = ctx.get_mtr();
	const undo_no_t undo_no = (trx == nullptr ? 0 : trx->undo_no-1);

	const Binary_diff_vector* bdiff_vector
		= upd->get_binary_diff_by_field_no(field_no);

	upd_field_t* uf = upd->get_field_by_field_no(field_no, index);

#ifdef UNIV_DEBUG
	/* Print information on server error log file, which can be
	used to confirm if InnoDB did partial update or not. */
	DBUG_EXECUTE_IF("lob_print_partial_update_hit",
			print_partial_update_hit(uf, index););
#endif /* UNIV_DEBUG */

	page_no_t first_page_no = blobref.page_no();
	space_id_t space_id = blobref.space_id();

	const page_size_t page_size = dict_table_page_size(index->table);
	const page_id_t first_page_id(space_id, first_page_no);

	first_page_t first_page(mtr, index);
	first_page.load_x(first_page_id, page_size);
	first_page.set_last_trx_id(trx->id);
	first_page.set_last_trx_undo_no(undo_no);
	const uint32_t lob_version = first_page.incr_lob_version();

	int count = 0;
	for (Binary_diff_vector::const_iterator iter = bdiff_vector->begin();
	     iter != bdiff_vector->end(); ++iter, ++count) {

		const Binary_diff* bdiff = iter;

		err = replace(
			ctx, trx, index, blobref, first_page,
			bdiff->offset(), bdiff->length(),
			(byte*) bdiff->new_data(uf->mysql_field), count);

		if (err != DB_SUCCESS) {
			break;
		}
	}

	blobref.set_offset(lob_version, mtr);

	DBUG_RETURN(err);
}

/** Find the file location of the index entry which gives the portion of LOB
containing the requested offset.
@param[in]	trx		the current transaction.
@param[in]	index		the clustered index containing LOB.
@param[in]	node_loc	Location of first index entry.
@param[in]	offset		the LOB offset whose location we seek.
@param[in]	mtr		mini-transaction context.
@return file location of index entry which contains requested LOB offset.*/
fil_addr_t
find_offset(
	trx_t*		trx,
	dict_index_t*	index,
	fil_addr_t	node_loc,
	ulint&		offset,
	mtr_t*		mtr)
{
	DBUG_ENTER("find_offset");

	buf_block_t* block = nullptr;

	index_entry_t entry(mtr, index);

	while (!fil_addr_is_null(node_loc)) {

		if (block == nullptr) {
			block = entry.load_x(node_loc);
		} else if (block->page.id.page_no() != node_loc.page) {
			block = entry.load_x(node_loc);
		} else {
			/* Next entry in the same page. */
			ut_ad(block == entry.get_block());
			entry.reset(node_loc);
		}

		/* Get the amount of data */
		ulint data_len = entry.get_data_len();

		if (offset < data_len) {
			break;
		}

		offset -= data_len;

		/* The next node should not be the same as the current node. */
		ut_ad(!node_loc.is_equal(entry.get_next()));

		node_loc = entry.get_next();
	}

	DBUG_RETURN(node_loc);
}

/** Replace a large object (LOB) with the given new data of equal length.
@param[in]	ctx		replace operation context.
@param[in]	trx		the transaction that is doing the read.
@param[in]	index		the clustered index containing the LOB.
@param[in]	ref		the LOB reference identifying the LOB.
@param[in]	first_page	the first page of the LOB.
@param[in]	offset		replace the LOB from the given offset.
@param[in]	len		the length of LOB data that needs to be
				replaced.
@param[in]	buf		the buffer (owned by caller) with new data
				(len bytes).
@param[in]	count		number of replace done on current LOB.
@return DB_SUCCESS on success, error code on failure. */
dberr_t
replace(
	InsertContext&	ctx,
	trx_t*		trx,
	dict_index_t*	index,
	ref_t		ref,
	first_page_t&	first_page,
	ulint		offset,
	ulint		len,
	byte*		buf,
	int		count)
{
	DBUG_ENTER("lob::replace");

	mtr_t*	mtr = ctx.get_mtr();
	uint32_t new_entries = 0;
	const undo_no_t undo_no = (trx == nullptr ? 0 : trx->undo_no-1);
	const uint32_t lob_version = first_page.get_lob_version();

#ifdef LOB_DEBUG
	std::cout << "thread=" << std::this_thread::get_id()
		<< ", lob::replace(): table=" << index->table->name
		<< ", ref=" << ref << ", count=" << count
		<< ", trx->id=" << trx->id
		<< ", trx->undo_no=" << trx->undo_no
		<< ", undo_no=" << undo_no
		<< std::endl;
#endif

	ut_ad(offset >= DICT_ANTELOPE_MAX_INDEX_COL_LEN
	      || dict_table_has_atomic_blobs(index->table));

	if (!dict_table_has_atomic_blobs(index->table)) {

		/* For compact and redundant row format, remove the local
		prefix length from the offset. */

		ut_ad(offset >= DICT_ANTELOPE_MAX_INDEX_COL_LEN);
		offset -= DICT_ANTELOPE_MAX_INDEX_COL_LEN;
	}

	DBUG_LOG("lob", "adjusted offset=" << offset << ", len=" << len);

	page_no_t first_page_no = ref.page_no();
	space_id_t space_id = ref.space_id();

	const page_size_t page_size = dict_table_page_size(index->table);
	const page_id_t first_page_id(space_id, first_page_no);

	if (count == 0) {
		/** Repeatedly updating the LOB should increment the
		ref count only once. */
#ifdef LOB_DEBUG
		std::cout << "thread=" << std::this_thread::get_id()
			<< ", lob::replace(): table=" << index->table->name
			<< ", ref=" << ref << std::endl;
#endif
	}

	flst_base_node_t* base_node = first_page.index_list();
	fil_addr_t node_loc = flst_get_first(base_node, mtr);

	ulint page_offset = offset;

	node_loc = find_offset(trx, index, node_loc, page_offset, mtr);
	ulint want = len; /* want to be replaced. */
	const byte* ptr = buf;

	index_entry_t cur_entry(mtr, index);

	if (page_offset > 0) {
		/* Part of the page contents needs to be changed.  So the
		old data must be read. */

		buf_block_t* tmp_block = cur_entry.load_x(node_loc);

		page_no_t cur_page_no = cur_entry.get_page_no();

		buf_block_t* new_block = nullptr;

		if (cur_page_no == first_page_no) {

			/* If current page number is the same as first page
			number, then first page is already loaded. Just update
			the pointer. */
			first_page.set_block(tmp_block);
			new_block = first_page.replace(trx, page_offset, ptr,
						       want, mtr);

		} else {

			/* If current page number is NOT the same as first page
			number, then load first page here. */
			first_page.load_x(first_page_id, page_size);
			data_page_t page(mtr, index);
			page.load_x(cur_page_no);
			new_block = page.replace(trx, page_offset, ptr, want, mtr);

		}
		page_offset = 0;

		data_page_t new_page(new_block, mtr, index);

		/* Allocate a new index entry.  First page is loaded by now. */
		flst_node_t* new_node = first_page.alloc_index_entry(false);

		new_entries++;
		index_entry_t new_entry(new_node, mtr, index);
		new_entry.set_versions_null();
		new_entry.set_trx_id(trx->id);
		new_entry.set_trx_id_modifier(trx->id);
		new_entry.set_trx_undo_no(undo_no);
		new_entry.set_trx_undo_no_modifier(undo_no);
		new_entry.set_page_no(new_page.get_page_no());
		new_entry.set_data_len(new_page.get_data_len());
		new_entry.set_lob_version(lob_version);

		cur_entry.set_trx_undo_no_modifier(undo_no);
		cur_entry.set_trx_id_modifier(trx->id);
		cur_entry.insert_after(base_node, new_entry);
		cur_entry.remove(base_node);
		new_entry.set_old_version(cur_entry);

		node_loc = new_entry.get_next();
	}

	while (!fil_addr_is_null(node_loc) && want > 0) {
		/* One page is updated for each iteration in the loop. */

		buf_block_t* cur_block = cur_entry.load_x(node_loc);
		page_no_t cur_page_no = cur_entry.get_page_no();

		if (cur_page_no == first_page_no) {
			first_page.set_block(cur_block);
		} else {
			first_page.load_x(first_page_id, page_size);
		}

		/* Get the page number */
		page_no_t page_no = cur_entry.get_page_no();
		page_id_t page_id(space_id, page_no);
		ulint data_len = cur_entry.get_data_len();

		if (want < data_len) {
			break;
		}

		/* Full data in data page is replaced.  So no need to
		read old page. */
		data_page_t new_page(mtr, index);
		new_page.alloc(mtr, false);
		new_page.write(trx->id, ptr, want);

		/* Allocate a new index entry */
		flst_node_t* new_node = first_page.alloc_index_entry(false);

		new_entries++;
		index_entry_t new_entry(new_node, mtr, index);
		new_entry.set_lob_version(lob_version);
		new_entry.set_versions_null();
		new_entry.set_trx_id(trx->id);
		new_entry.set_trx_id_modifier(trx->id);
		new_entry.set_trx_undo_no(undo_no);
		new_entry.set_trx_undo_no_modifier(undo_no);
		new_entry.set_page_no(new_page.get_page_no());
		new_entry.set_data_len(new_page.get_data_len());

		cur_entry.set_trx_id_modifier(trx->id);
		cur_entry.set_trx_undo_no_modifier(undo_no);
		cur_entry.insert_after(base_node, new_entry);
		cur_entry.remove(base_node);
		new_entry.set_old_version(cur_entry);

		node_loc = new_entry.get_next();
	}

	if (!fil_addr_is_null(node_loc) && want > 0) {
		/* Part of the page contents needs to be changed.  So the
		old data must be read. */

		cur_entry.load_x(node_loc);
		first_page.load_x(first_page_id, page_size);

		ulint cur_page_no = cur_entry.get_page_no();

		buf_block_t* new_block = nullptr;

		if (cur_page_no == first_page_no) {

			new_block = first_page.replace(trx, 0, ptr,
						       want, mtr);
		} else {

			data_page_t page(mtr, index);
			page.load_x(cur_page_no);
			new_block = page.replace(trx, 0, ptr, want, mtr);

		}

		data_page_t new_page(new_block, mtr, index);

		/* Allocate a new index entry */
		flst_node_t* new_node = first_page.alloc_index_entry(false);

		new_entries++;
		index_entry_t new_entry(new_node, mtr, index);
		new_entry.set_lob_version(lob_version);
		new_entry.set_versions_null();
		new_entry.set_trx_id(trx->id);
		new_entry.set_trx_id_modifier(trx->id);
		new_entry.set_trx_undo_no(undo_no);
		new_entry.set_trx_undo_no_modifier(undo_no);
		new_entry.set_page_no(new_page.get_page_no());
		new_entry.set_data_len(new_page.get_data_len());

		cur_entry.set_trx_undo_no_modifier(undo_no);
		cur_entry.set_trx_id_modifier(trx->id);
		cur_entry.insert_after(base_node, new_entry);
		cur_entry.remove(base_node);
		new_entry.set_old_version(cur_entry);
	}

#ifdef LOB_DEBUG
	first_page.print_index_entries(std::cout);
#endif /* LOB_DEBUG */

	DBUG_RETURN(DB_SUCCESS);
}

};  // namespace lob
