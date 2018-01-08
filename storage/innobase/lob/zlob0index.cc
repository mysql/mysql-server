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

#include "zlob0index.h"
#include "zlob0first.h"
#include "lob0util.h"
#include "trx0purge.h"

namespace lob {

/** Purge one index entry.
@param[in]	index		index to which LOB belongs.
@param[in]	trxid		purging data belonging to trxid.
@param[in]	first		first page of LOB.
@param[in,out]	lst		list from which this entry will be
				removed.
@param[in,out]	free_list	list to which this entry will be
				added.*/
fil_addr_t
z_index_entry_t::purge_version(
	dict_index_t*		index,
	trx_id_t		trxid,
	z_first_page_t&		first,
	flst_base_node_t*	lst,
	flst_base_node_t*	free_list)
{
	/* Save the location of next node. */
	fil_addr_t next_loc = get_next();

	/* Remove the current node from the list it belongs. */
	remove(lst);

	/* Purge the current node. */
	purge(index, first);

	/* Add the current node to the free list. */
	push_front(free_list);

	/* Return the next node location. */
	return(next_loc);
}

/** The current index entry points to a latest LOB page.  It may or may
not have older versions.  If older version is there, bring it back to the
index list from the versions list.  Then remove the current entry from
the index list.  Move the versions list from current entry to older entry.
@param[in]  trxid  The transaction identifier.
@param[in]  first  The first lob page containing index list and free list. */
fil_addr_t
z_index_entry_t::make_old_version_current(
	dict_index_t*	index,
	trx_id_t	trxid,
	z_first_page_t&	first)
{
	flst_base_node_t* idx_flst = first.index_list();
	flst_base_node_t* free_list = first.free_list();
	flst_base_node_t* version_list = get_versions_list();

	if (flst_get_len(version_list) > 0) {
		space_id_t space = dict_index_get_space(index);
		const page_size_t page_size = dict_table_page_size(index->table);

		/* Remove the old version from versions list. */
		fil_addr_t old_node_addr = flst_get_first(version_list, m_mtr);
		flst_node_t* old_node = fut_get_ptr(
			space, page_size, old_node_addr, RW_X_LATCH, m_mtr);
		flst_remove(version_list, old_node, m_mtr);

		/* Copy the version base node from current to old entry. */
		z_index_entry_t old_entry(old_node, m_mtr, index);
		move_version_base_node(old_entry);

		/* Insert the old version after the current node. */
		insert_after(idx_flst, old_entry);
	}

	fil_addr_t loc = purge_version(index, trxid, first, idx_flst,
				       free_list);

	ut_ad(flst_validate(idx_flst, m_mtr));

	return(loc);
}

/** Purge the current index entry. An index entry points to either a FIRST
page or DATA page.  That LOB page will be freed if it is DATA page.  A FIRST
page should not be freed. */
void
z_index_entry_t::purge(
	dict_index_t*	index,
	z_first_page_t&	first)
{
	set_data_len(0);

	const space_id_t  space_id	= dict_index_get_space(index);
	const page_size_t page_size	= dict_table_page_size(index->table);

	while (true) {
		page_no_t page_no = get_z_page_no();

		if (page_no == FIL_NULL) {
			break;
		}

		buf_block_t* block = buf_page_get(
			page_id_t(space_id, page_no),
			page_size, RW_X_LATCH, m_mtr);

		page_type_t type = fil_page_get_type(block->frame);
		page_no_t next = block->get_next_page_no();
		set_z_page_no(next);

		switch (type) {
		case FIL_PAGE_TYPE_ZLOB_FIRST: {
			z_first_page_t first(block, m_mtr, index);
			first.set_data_len(0);
			first.set_trx_id(0);
			first.set_next_page_null();
		} break;
		case FIL_PAGE_TYPE_ZLOB_DATA:
			btr_page_free_low(index, block, ULINT_UNDEFINED,
					  m_mtr);
		break;
		case FIL_PAGE_TYPE_ZLOB_FRAG: {
			z_frag_page_t frag_page(block, m_mtr, index);
			frag_id_t fid = get_z_frag_id();
			ut_ad(fid != FRAG_ID_NULL);
			ut_ad(frag_page.get_n_frags() > 0);

			frag_page.dealloc_fragment(fid);

			if (frag_page.get_n_frags() == 0) {
				frag_page.dealloc(first, m_mtr);
			}
		} break;
		default:
			ut_ad(0);
		}

		if (type == FIL_PAGE_TYPE_ZLOB_FRAG) {
			break;
		}
	}
	init();
}

std::ostream&
z_index_entry_t::print(std::ostream& out) const
{
	if (m_node == nullptr) {
		out << "[z_index_entry_t: m_node=null]";
	} else {
		out << "[z_index_entry_t: m_node=" << (void*) m_node
			<< ", prev=" << get_prev() << ", next=" << get_next()
			<< ", versions=" << flst_bnode_t(get_versions_list(), m_mtr)
			<< ", trx_id=" << get_trx_id()
			<< ", modifier trx_id=" << get_trx_id_modifier()
			<< ", trx_undo_no=" << get_trx_undo_no()
			<< ", trx_undo_no_modifier=" << get_trx_undo_no_modifier()
			<< ", z_page_no=" << get_z_page_no()
			<< ", z_frag_id=" << get_z_frag_id() << ", data_len="
			<< get_data_len() << ", zdata_len=" << get_zdata_len()
			<< "]";
	}
	return(out);
}

std::ostream&
z_index_entry_t::print_pages(std::ostream& out) const
{
	page_no_t page_no = get_z_page_no();

	const space_id_t space_id = dict_index_get_space(m_index);
	const page_size_t page_size = dict_table_page_size(m_index->table);

	out << "[PAGES: ";
	while (page_no != FIL_NULL) {
		buf_block_t* block = buf_page_get(
			page_id_t(space_id, page_no),
			page_size, RW_S_LATCH, m_mtr);

		ulint type = block->get_page_type();
		out << "[page_no=" << page_no << ", type="
			<< block->get_page_type_str() << "]";

		page_no = block->get_next_page_no();
		if (type == FIL_PAGE_TYPE_ZLOB_FRAG) {
			/* Reached the fragment page. Stop. */
			break;
		}
	}

	out << "]";
	return(out);
}

fil_addr_t z_index_entry_t::get_self() const
{
	if (m_node == nullptr) {
		return(fil_addr_null);
	}
	page_t* frame = page_align(m_node);
	page_no_t page_no = mach_read_from_4(frame + FIL_PAGE_OFFSET);
	ulint offset = m_node - frame;
	ut_ad(offset < UNIV_PAGE_SIZE);
	return(fil_addr_t(page_no, offset));
}

void
z_index_entry_t::read(z_index_entry_mem_t& entry_mem) const
{
	entry_mem.m_self = get_self();
	entry_mem.m_prev = get_prev();
	entry_mem.m_next = get_next();
	entry_mem.m_versions = get_versions_mem();
	entry_mem.m_trx_id = get_trx_id();
	entry_mem.m_z_page_no = get_z_page_no();
	entry_mem.m_z_frag_id = get_z_frag_id();
	entry_mem.m_data_len = get_data_len();
	entry_mem.m_z_data_len = get_zdata_len();
}

void
z_index_entry_t::move_version_base_node(z_index_entry_t& entry)
{
	ut_ad(m_mtr != nullptr);

	flst_base_node_t* from_node = get_versions_list();
	flst_base_node_t* to_node = entry.get_versions_list();

	/* Copy with redo logging the version list base node. */
	mlog_write_string(to_node, from_node, FLST_BASE_NODE_SIZE, m_mtr);

	ut_ad(flst_get_len(from_node) == flst_get_len(to_node));
	fil_addr_t addr1 = flst_get_first(from_node, m_mtr);
	fil_addr_t addr2 = flst_get_first(to_node, m_mtr);
	ut_ad(addr1.is_equal(addr2));
	addr1 = flst_get_last(from_node, m_mtr);
	addr2 = flst_get_last(to_node, m_mtr);
	ut_ad(addr1.is_equal(addr2));
	flst_init(from_node, m_mtr);
}

/* The given entry becomes the old version of the current entry.
Move the version base node from old entry to current entry.
@param[in]  entry  the old entry */
void
z_index_entry_t::set_old_version(z_index_entry_t& entry)
{
	flst_base_node_t* version_list = get_versions_list();
	ut_ad(flst_get_len(version_list) == 0);
	entry.move_version_base_node(*this);
	entry.push_front(version_list);
}

std::ostream&
z_index_entry_mem_t::print(std::ostream& out) const
{
	out << "[z_index_entry_mem_t: self=" << m_self << ", prev="
		<< m_prev << ", next=" << m_next << ", versions=" << m_versions
		<< ", m_trx_id=" << m_trx_id
		<< ", m_trx_id_modifier=" << m_trx_id_modifier
		<< ", m_trx_undo_no=" << m_trx_undo_no
		<< ", m_trx_undo_no_modifier=" << m_trx_undo_no_modifier
		<< ", z_page_no=" << m_z_page_no << ", z_frag_id="
		<< m_z_frag_id << ", data_len=" << m_data_len
		<< ", z_data_len=" << m_z_data_len << "]";
		return(out);
}

}; /* namespace lob */
