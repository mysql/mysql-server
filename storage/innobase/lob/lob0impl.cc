/*****************************************************************************

Copyright (c) 2016, 2018, Oracle and/or its affiliates. All Rights Reserved.

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

#include "lob0impl.h"
#include "lob0del.h"
#include "lob0index.h"
#include "lob0inf.h"
#include "lob0ins.h"
#include "lob0pages.h"
#include "lob0util.h"
#include "lob0zip.h"
#include "my_dbug.h"
#include "trx0sys.h"
#include "ut0ut.h"
#include "zlob0first.h"
#include "zlob0index.h"
#include "zlob0read.h"

namespace lob {

static void buf_block_set_next_page_no(buf_block_t *block,
                                       page_no_t next_page_no, mtr_t *mtr) {
  mlog_write_ulint(block->frame + FIL_PAGE_NEXT, next_page_no, MLOG_4BYTES,
                   mtr);
}

#ifdef UNIV_DEBUG
/** Validate the page list.
@return true if valid, false otherwise. */
bool plist_base_node_t::validate() const {
  ulint len = 0;
  ulint exp = get_len();

  for (plist_node_t cur = get_first_node(); !cur.is_null();
       cur = cur.get_next_node()) {
    len++;
    ut_ad(len <= exp);
  }

  ut_ad(len == exp);
  return (true);
}
#endif /* UNIV_DEBUG */

/** Allocate one node page. */
buf_block_t *node_page_t::alloc(first_page_t &first_page, bool bulk) {
  page_no_t hint = FIL_NULL;
  m_block = alloc_lob_page(m_index, m_mtr, hint, bulk);

  DBUG_EXECUTE_IF("innodb_lob_alloc_node_page_failed", m_block = nullptr;);

  if (m_block == nullptr) {
    return (nullptr);
  }

  set_page_type();
  set_version_0();
  set_next_page(first_page.get_next_page());
  first_page.set_next_page(get_page_no());

  /* Use fully for the LOB index contents */
  ulint lob_metadata_len = payload();
  ulint node_count = lob_metadata_len / index_entry_t::SIZE;

  flst_base_node_t *free_list = first_page.free_list();

  byte *cur = nodes_begin();

  /* Populate the free list with empty index entry nodes. */
  for (ulint i = 0; i < node_count; ++i) {
    flst_add_last(free_list, cur, m_mtr);
    cur += index_entry_t::SIZE;
  }

  ut_ad(flst_validate(free_list, m_mtr));
  return (m_block);
}

std::ostream &z_frag_entry_t::print(std::ostream &out) const {
  out << "[z_frag_entry_t: prev=" << get_prev() << ", next=" << get_next()
      << ", page_no=" << get_page_no() << ", n_frags=" << get_n_frags()
      << ", used_len=" << get_used_len()
      << ", total_free_len=" << get_total_free_len()
      << ", big_free_len=" << get_big_free_len() << "]";
  return (out);
}

void z_frag_entry_t::purge(flst_base_node_t *used_lst,
                           flst_base_node_t *free_lst) {
  remove(used_lst);
  init();
  push_front(free_lst);
}

/** Update the current fragment entry with information about
the given fragment page.
@param[in]	frag_page	the fragment page whose information
                                will be stored in current fragment entry. */
void z_frag_entry_t::update(const z_frag_page_t &frag_page) {
  ut_ad(m_mtr != nullptr);

  set_page_no(frag_page.get_page_no());
  set_n_frags(frag_page.get_n_frags());
  set_used_len(frag_page.get_total_stored_data());
  set_total_free_len(frag_page.get_total_free_len());
  set_big_free_len(frag_page.get_big_free_len());
}

/** Insert a single zlib stream.
@param[in]	index	the index to which the LOB belongs.
@param[in]	first	the first page of the compressed LOB.
@param[in]	trxid	the id of the current transaction.
@param[in]	blob	in memory copy of the LOB.
@param[in]	len	the length of the LOB.
@param[in]	mtr	the mini transaction context.
@param[in]	bulk	true if bulk operation, false otherwise.
@param[out]	start_page_no	the first page into which zlib stream
                                was written.
@param[out]	frag_id	the fragment id that contains last part of the
                        zlib stream.
@return DB_SUCCESS on success, error code on error. */
dberr_t z_insert_strm(dict_index_t *index, z_first_page_t &first,
                      trx_id_t trxid, byte *blob, ulint len, mtr_t *mtr,
                      bool bulk, page_no_t &start_page_no, frag_id_t &frag_id) {
  ulint remain = len;
  start_page_no = FIL_NULL;
  frag_id = FRAG_ID_NULL;
  page_no_t prev_page_no;
  byte *lob_ptr = blob;
  const page_no_t first_page_no = first.get_page_no();

#ifdef UNIV_DEBUG
  ulint frag_max_payload = z_frag_page_t::max_payload(index);
#endif /* UNIV_DEBUG */

  /* If the first page is empty, then make use of it. */
  if (first.get_data_len() == 0) {
    /* First page is unused. Use it. */
    byte *ptr = first.begin_data_ptr();
    ulint size = first.payload();
    ulint to_copy = (remain > size) ? size : remain;
    mlog_write_string(ptr, lob_ptr, to_copy, mtr);
    remain -= to_copy;
    lob_ptr += to_copy;

    start_page_no = first.get_page_no();
    prev_page_no = start_page_no;

    first.set_data_len(to_copy);
    first.set_trx_id(trxid);
    first.set_next_page_null();

  } else if (!z_frag_page_t::can_data_fit(index, remain)) {
    /* Data cannot fit into a fragment page. Allocate a data
    page. */

    z_data_page_t data_page(mtr, index);
    buf_block_t *tmp_block = data_page.alloc(first_page_no + 1, bulk);

    if (tmp_block == nullptr) {
      return (DB_OUT_OF_FILE_SPACE);
    }

    byte *ptr = data_page.begin_data_ptr();
    ulint size = data_page.payload();
    ulint to_copy = (remain > size) ? size : remain;

    /* Copy data into the page. */
    mlog_write_string(ptr, lob_ptr, to_copy, mtr);

    remain -= to_copy;
    lob_ptr += to_copy;

    start_page_no = data_page.get_page_no();
    prev_page_no = start_page_no;

    data_page.set_data_len(to_copy);
    data_page.set_trx_id(trxid);

  } else {
    /* Data can fit into a fragment page. */
    z_frag_page_t frag_page(mtr, index);

    z_frag_entry_t frag_entry = first.find_frag_page(bulk, remain, frag_page);

    if (frag_entry.is_null()) {
      return (DB_OUT_OF_FILE_SPACE);
    }

#ifdef UNIV_DEBUG
    const ulint big_free_len_1 = frag_page.get_big_free_len();
    const ulint big_free_len_2 = frag_entry.get_big_free_len();
    ut_ad(big_free_len_1 == big_free_len_2);
#endif /* UNIV_DEBUG */

    frag_id = frag_page.alloc_fragment(remain, frag_entry);
    ut_ad(frag_id != FRAG_ID_NULL);

    frag_node_t node = frag_page.get_frag_node(frag_id);
    byte *ptr = node.frag_begin();

    ut_ad(remain == node.payload());

    /* copy data to the page. */
    mlog_write_string(ptr, lob_ptr, remain, mtr);

    remain = 0;
    lob_ptr += remain;
    start_page_no = frag_page.get_page_no();

    /* Update the frag entry. */
    frag_entry.update(frag_page);

    return (DB_SUCCESS);
  }

  /* As long as data cannot fit into a fragment page, use a data page. */
  while (remain > 0 && !z_frag_page_t::can_data_fit(index, remain)) {
    z_data_page_t data_page(mtr, index);
    buf_block_t *new_block = data_page.alloc(first_page_no + 1, bulk);

    if (new_block == nullptr) {
      return (DB_OUT_OF_FILE_SPACE);
    }

    byte *ptr = data_page.begin_data_ptr();
    ulint size = data_page.payload();
    ulint to_copy = (remain > size) ? size : remain;

    mlog_write_string(ptr, lob_ptr, to_copy, mtr);

    remain -= to_copy;
    lob_ptr += to_copy;

    data_page.set_data_len(to_copy);
    data_page.set_trx_id(trxid);

    /* Get the previous page and update its next page. */
    buf_block_t *block =
        buf_page_get(page_id_t(dict_index_get_space(index), prev_page_no),
                     dict_table_page_size(index->table), RW_X_LATCH, mtr);

    buf_block_set_next_page_no(block, data_page.get_page_no(), mtr);

    prev_page_no = data_page.get_page_no();
  }

  if (remain > 0) {
    ut_ad(remain < frag_max_payload);
    ut_ad(frag_id == FRAG_ID_NULL);
    z_frag_page_t frag_page(mtr, index);

    z_frag_entry_t frag_entry = first.find_frag_page(bulk, remain, frag_page);

    if (frag_entry.is_null()) {
      return (DB_OUT_OF_FILE_SPACE);
    }

    ut_ad(frag_entry.get_big_free_len() >= remain);
    ut_ad(frag_page.get_big_free_len() >= remain);

#ifdef UNIV_DEBUG
    const ulint big_free_len_1 = frag_page.get_big_free_len();
    const ulint big_free_len_2 = frag_entry.get_big_free_len();
    ut_ad(big_free_len_1 == big_free_len_2);
#endif /* UNIV_DEBUG */

    frag_id = frag_page.alloc_fragment(remain, frag_entry);
    ut_ad(frag_id != FRAG_ID_NULL);

    frag_node_t node = frag_page.get_frag_node(frag_id);
    byte *ptr = node.frag_begin();

    ut_ad(remain <= node.payload());

    mlog_write_string(ptr, lob_ptr, remain, mtr);
    remain = 0;
    lob_ptr += remain;

    /* Update the frag entry. */
    frag_entry.update(frag_page);

    /* Get the previous page and update its next page. */
    buf_block_t *block =
        buf_page_get(page_id_t(dict_index_get_space(index), prev_page_no),
                     dict_table_page_size(index->table), RW_X_LATCH, mtr);

    buf_block_set_next_page_no(block, frag_page.get_page_no(), mtr);
  }

  return (DB_SUCCESS);
}

/** Insert one chunk of input.  The maximum size of a chunk is Z_CHUNK_SIZE.
@param[in]  index      clustered index in which LOB is inserted.
@param[in]  first      the first page of the LOB.
@param[in]  trx        transaction doing the insertion.
@param[in]  ref        LOB reference in the clust rec.
@param[in]  blob       the uncompressed LOB to be inserted.
@param[in]  len        length of the blob.
@param[out] out_entry  the newly inserted index entry. can be NULL.
@param[in]  mtr        the mini transaction
@param[in]  bulk       true if it is bulk operation, false otherwise.
@return DB_SUCCESS on success, error code on failure. */
dberr_t z_insert_chunk(dict_index_t *index, z_first_page_t &first, trx_t *trx,
                       ref_t ref, byte *blob, ulint len,
                       z_index_entry_t *out_entry, mtr_t *mtr, bool bulk) {
  ut_ad(len <= Z_CHUNK_SIZE);
  ut_ad(first.get_page_type() == FIL_PAGE_TYPE_ZLOB_FIRST);
  dberr_t err(DB_SUCCESS);

  const trx_id_t trxid = (trx == nullptr ? 0 : trx->id);
  const undo_no_t undo_no = (trx == nullptr ? 0 : trx->undo_no - 1);
  z_stream strm;

  strm.zalloc = nullptr;
  strm.zfree = nullptr;
  strm.opaque = nullptr;

  int ret = deflateInit(&strm, page_zip_level);

  ut_a(ret == Z_OK);

  strm.avail_in = len;
  strm.next_in = blob;

  /* It is possible that the compressed stream is actually bigger.  So
  making use of this call to find it out for sure. */
  const ulint max_buf = deflateBound(&strm, len);

  std::unique_ptr<byte[]> tmpbuf(new byte[max_buf]);
  strm.avail_out = max_buf;
  strm.next_out = tmpbuf.get();

  ret = deflate(&strm, Z_FINISH);
  ut_a(ret == Z_STREAM_END);
  ut_a(strm.avail_in == 0);
  ut_a(strm.total_out == (max_buf - strm.avail_out));

  page_no_t z_page_no;
  frag_id_t z_frag_id;
  err = z_insert_strm(index, first, trxid, tmpbuf.get(), strm.total_out, mtr,
                      bulk, z_page_no, z_frag_id);

  if (err != DB_SUCCESS) {
    deflateEnd(&strm);
    return (err);
  }

  z_index_entry_t entry = first.alloc_index_entry(bulk);

  if (entry.is_null()) {
    deflateEnd(&strm);
    return (DB_OUT_OF_FILE_SPACE);
  }

  entry.set_trx_id(trxid);
  entry.set_trx_id_modifier(trxid);
  entry.set_trx_undo_no(undo_no);
  entry.set_trx_undo_no_modifier(undo_no);
  entry.set_z_page_no(z_page_no);
  entry.set_z_frag_id(z_frag_id);
  entry.set_data_len(len);
  entry.set_zdata_len(strm.total_out);

  deflateEnd(&strm);

  if (out_entry != nullptr) {
    out_entry->reset(entry);
  }

  ut_ad(z_validate_strm(index, entry, mtr));
  return (DB_SUCCESS);
}

/** Insert a large object (LOB) into the system.
@param[in]      ctx    the B-tree context for this LOB operation.
@param[in]      trx    transaction doing the insertion.
@param[in,out]  ref    the LOB reference.
@param[in]      field  the LOB field.
@return DB_SUCCESS on success, error code on failure.*/
dberr_t z_insert(InsertContext *ctx, trx_t *trx, ref_t &ref,
                 big_rec_field_t *field, ulint field_j) {
  byte *blob = field->ptr();
  ulint len = field->len;
  ulint remain = len;
  byte *ptr = blob;
  dberr_t err(DB_SUCCESS);
  dict_index_t *index = ctx->index();
  space_id_t space_id = dict_index_get_space(index);
  byte *field_ref;
  mtr_t *mtr = ctx->get_mtr();
  const trx_id_t trxid = (trx == nullptr ? 0 : trx->id);
  const ulint commit_freq = 4;

  ut_ad(remain > 0);

  if (ref.length() > 0) {
    ref.set_length(len, 0);
    if (!ctx->is_bulk()) {
      ctx->zblob_write_blobref(field->field_no, ctx->m_mtr);
    }
  }

  const page_size_t page_size(dict_table_page_size(index->table));

  if (!ref_t::is_big(page_size, len)) {
    /* The LOB is not big enough to build LOB index. Insert the
    LOB without an LOB index. */
    zInserter zblob_writer(ctx);
    err = zblob_writer.prepare();
    if (err == DB_SUCCESS) {
      zblob_writer.write_one_small_blob(field_j);
      err = zblob_writer.finish(false);
    }
    return (err);
  }

  z_first_page_t first(mtr, index);
  buf_block_t *first_block = first.alloc(ctx->is_bulk());

  if (first_block == nullptr) {
    return (DB_OUT_OF_FILE_SPACE);
  }

  first.init_lob_version();
  first.set_last_trx_id(trxid);

  const page_no_t first_page_no = first.get_page_no();
  const page_id_t first_page_id(dict_index_get_space(index), first_page_no);

  if (dict_index_is_online_ddl(index)) {
    row_log_table_blob_alloc(index, first_page_no);
  }

  flst_base_node_t *idx_list = first.index_list();

  ulint nth_chunk = 0;

  while (remain > 0) {
    ut_ad(first.get_page_type() == FIL_PAGE_TYPE_ZLOB_FIRST);

    z_index_entry_t entry(mtr, index);
    ulint size = (remain >= Z_CHUNK_SIZE) ? Z_CHUNK_SIZE : remain;

    err = z_insert_chunk(index, first, trx, ref, ptr, size, &entry, mtr,
                         ctx->is_bulk());

    if (err != DB_SUCCESS) {
      return (err);
    }

    entry.set_lob_version(1);

    ptr += size;
    remain -= size;

    entry.push_back(idx_list);

    if (++nth_chunk % commit_freq == 0) {
      ctx->check_redolog();
      field_ref = ctx->get_field_ref(field->field_no);
      ref.set_ref(field_ref);
      first.load_x(first_page_id, page_size);

      /* The first page could have been re-located.  Reset
      the idx_list to the correct value. */
      idx_list = first.index_list();
    }
  }

  /* Must have inserted atleast one chunk. */
  ut_ad(nth_chunk > 0);

  field_ref = ctx->get_field_ref(field->field_no);
  ref.set_ref(field_ref);

  ref.update(space_id, first_page_no, 1, 0);
  ref.set_length(len, 0);

  ctx->make_nth_extern(field->field_no);

  if (!ctx->is_bulk()) {
    ctx->zblob_write_blobref(field->field_no, ctx->m_mtr);
  }

  /* If the full LOB could not be inserted, then we report error. */
  ut_ad(remain == 0);

#ifdef ZLOB_DEBUG
  std::cout << "thread=" << std::this_thread::get_id()
            << ", lob::z_insert(): table=" << ctx->index()->table->name
            << ", ref=" << ref << std::endl;
  first.print(std::cout);
#endif

  DBUG_EXECUTE_IF("innodb_zlob_print", z_print_info(index, ref, std::cerr););

  return (err);
}

/** Print information about the given compressed lob.
@param[in]  index  the index dictionary object.
@param[in]  ref    the LOB reference
@param[out] out    the output stream where information is printed.
@return DB_SUCCESS on success, or an error code. */
dberr_t z_print_info(const dict_index_t *index, const lob::ref_t &ref,
                     std::ostream &out) {
  mtr_t mtr;
  mtr_start(&mtr);
  z_first_page_t first(&mtr, const_cast<dict_index_t *>(index));
  first.load_x(ref.page_no());
  first.print(out);
  mtr_commit(&mtr);
  return (DB_SUCCESS);
}

/** Allocate the fragment page.
@param[in]	hint	hint page number for allocation.
@param[in]	bulk	true if bulk operation (OPCODE_INSERT_BULK)
                        false otherwise.
@return the allocated buffer block. */
buf_block_t *z_frag_page_t::alloc(page_no_t hint, bool bulk) {
  m_block = alloc_lob_page(m_index, m_mtr, hint, bulk);

  DBUG_EXECUTE_IF("innodb_lob_alloc_z_frag_page_failed", m_block = nullptr;);

  if (m_block == nullptr) {
    return (nullptr);
  }

  /* Set page type to FIL_PAGE_TYPE_ZLOB_FRAG. */
  set_page_type();
  set_version_0();
  set_page_next(FIL_NULL);

  set_frag_entry_null();

  /* Initialize the frag free list. */
  plist_base_node_t fl = free_list();
  fl.init();
  ut_ad(fl.validate());

  /* Initialize the used frag list. */
  plist_base_node_t frag_lst = frag_list();
  frag_lst.init();
  ut_ad(frag_lst.validate());

  byte *page = frame();

  /* Add the available space as free frag to free list. */
  frag_node_t frag(page, page + OFFSET_FRAGS_BEGIN, payload(), m_mtr);
  fl.push_front(frag.m_node);
  frag.set_frag_id_null();

  ut_ad(fl.validate());
  return (m_block);
}

/** Determine if the given length of data can fit into a fragment page.
@param[in]   index   the clust index into which LOB is inserted.
@param[in]   data_size  The length of data to operate.
@return true if data can fit into fragment page, false otherwise. */
bool z_frag_page_t::can_data_fit(dict_index_t *index, ulint data_size) {
  ulint max_size = max_payload(index);

  /* Look for a fragment page only if the data to be stored is less
  than a quarter of the size of the fragment page. */
  return (data_size < (max_size / 4));
}

buf_block_t *z_frag_node_page_t::alloc(z_first_page_t &first, bool bulk) {
  ut_ad(m_block == nullptr);
  page_no_t hint = FIL_NULL;

  m_block = alloc_lob_page(m_index, m_mtr, hint, bulk);

  DBUG_EXECUTE_IF("innodb_lob_alloc_z_frag_node_page_failed",
                  m_block = nullptr;);

  if (m_block == nullptr) {
    return (nullptr);
  }

  set_page_type();
  set_version_0();
  flst_base_node_t *free_lst = first.free_frag_list();
  init(free_lst);

  /* Link the allocated index page to the first page. */
  page_no_t page_no = first.get_frag_node_page_no();
  set_next_page_no(page_no);
  first.set_frag_node_page_no(get_page_no());
  return (m_block);
}

/** Allocate a fragment with the given payload.
@return the frag_id of the allocated fragment. */
frag_id_t z_frag_page_t::alloc_fragment(ulint size, z_frag_entry_t &entry) {
  plist_base_node_t free_lst = free_list();

  ut_ad(free_lst.get_len() > 0);

  const ulint big_free_len = get_big_free_len();
  ut_d(bool visited_big_frag = false;);

  for (plist_node_t cur = free_lst.get_first_node(); !cur.is_null();
       cur = cur.get_next_node()) {
    frag_node_t frag(cur, m_mtr);
    const ulint total_len = frag.get_total_len();
    const ulint payload = frag.payload();
    const ulint overhead = frag_node_t::overhead();

    /* Get the biggest free fragment available. */
    if (total_len != big_free_len) {
      continue;
    }

    ut_d(visited_big_frag = true;);

    bool exact_fit = false;

    if (is_last_frag(frag)) {
      /* This fragment gives space for the directory
      entry. */
      ulint extra = frag_node_t::SIZE_OF_PAGE_DIR_ENTRY;
      if (payload == (size + extra)) {
        exact_fit = true;
      }
    } else {
      /* This fragment does not give space for the
      directory entry. */
      if (payload == size) {
        exact_fit = true;
      }
    }

    if (exact_fit) {
      /* Allocate the fragment id. */
      ulint frag_id = alloc_frag_id();
      ut_ad(frag_id != FRAG_ID_NULL);

      /* this is the requested fragment. */
      free_lst.remove(cur);
      insert_into_frag_list(frag);

      frag.set_frag_id(frag_id);
      set_nth_dir_entry(frag_id, frag.addr());
      entry.update(*this);
      return (frag_id);

    } else if (payload >= (size + overhead + 1)) {
      /* Break the current fragment into two. Atleast 1 byte
      payload must be there in the other node. */

      split_free_frag(frag, size);
      free_lst.remove(frag.m_node);
      insert_into_frag_list(frag);

      /* Allocate the fragment id. */
      ulint frag_id = alloc_frag_id();
      ut_ad(frag_id != FRAG_ID_NULL);

      frag.set_frag_id(frag_id);
      set_nth_dir_entry(frag_id, frag.addr());
      entry.update(*this);
      return (frag_id);
    } else {
      ut_error;
    }
  }

  ut_ad(visited_big_frag);
  ut_error;
}

/** Grow the frag directory by one entry.
@return the fragment identifier that was newly added. */
ulint z_frag_page_t::alloc_dir_entry() {
  plist_base_node_t free_lst = free_list();
  plist_node_t last = free_lst.get_last_node();
  frag_node_t frag(last, m_mtr);
  ulint len = frag.payload();

  /* The last free fragment must be adjacent to the directory.
  Then only it can give space to one slot. */
  if (frag.end_ptr() != slots_end_ptr()) {
    ut_ad(0);
    return (FRAG_ID_NULL);
  }

  if (len <= SIZE_OF_PAGE_DIR_ENTRY) {
    ut_ad(0);
    return (FRAG_ID_NULL);
  }

  incr_n_dir_entries();
  frag.decr_length_by_2();
  return (init_last_dir_entry());
}

z_frag_entry_t z_frag_page_t::get_frag_entry_x() {
  fil_addr_t node_loc = get_frag_entry_addr();
  flst_node_t *node = addr2ptr_x(node_loc);
  z_frag_entry_t entry(node, m_mtr);
  ut_ad(entry.get_page_no() == get_page_no());
  return (entry);
}

z_frag_entry_t z_frag_page_t::get_frag_entry_s() {
  fil_addr_t node_loc = get_frag_entry_addr();
  flst_node_t *node = addr2ptr_s(node_loc);
  z_frag_entry_t entry(node, m_mtr);
  ut_ad(entry.get_page_no() == get_page_no());
  return (entry);
}

void z_frag_page_t::dealloc(z_first_page_t &first, mtr_t *alloc_mtr) {
  ut_ad(get_n_frags() == 0);
  z_frag_entry_t entry = get_frag_entry_x();
  entry.purge(first.frag_list(), first.free_frag_list());
  btr_page_free_low(m_index, m_block, ULINT_UNDEFINED, alloc_mtr);
  m_block = nullptr;
}

std::ostream &z_frag_page_t::print_frags_in_order(std::ostream &out) const {
  if (m_block == nullptr) {
    return (out);
  }

  plist_base_node_t free_lst = free_list();
  plist_base_node_t frag_lst = frag_list();

  out << "[Free List: " << free_lst << "]" << std::endl;
  out << "[Frag List: " << frag_lst << "]" << std::endl;

  frag_node_t cur_free(free_lst.get_first_node(), m_mtr);
  frag_node_t cur_frag(frag_lst.get_first_node(), m_mtr);

  while (!cur_free.is_null() && !cur_frag.is_null()) {
    if (cur_free.is_before(cur_frag)) {
      out << "F: " << cur_free << std::endl;
      cur_free = cur_free.get_next_node();
    } else {
      out << "U: " << cur_frag << std::endl;
      cur_frag = cur_frag.get_next_node();
    }
  }

  if (cur_free.is_null()) {
    while (!cur_frag.is_null()) {
      out << "U: " << cur_frag << std::endl;
      cur_frag = cur_frag.get_next_node();
    }
  }

  if (cur_frag.is_null()) {
    while (!cur_free.is_null()) {
      out << "F: " << cur_free << std::endl;
      cur_free = cur_free.get_next_node();
    }
  }

  return (out);
}

/** Get the total amount of stored data in this page. */
ulint z_frag_page_t::get_total_stored_data() const {
  ulint len = 0;

  ut_ad(m_block != nullptr);

  plist_base_node_t frag_lst = frag_list();

  for (plist_node_t cur = frag_lst.get_first_node(); !cur.is_null();
       cur = cur.get_next_node()) {
    frag_node_t frag(cur, m_mtr);
    len += frag.payload();
  }

  return (len);
}

/** Get the total cumulative free space in this page. */
ulint z_frag_page_t::get_total_free_len() const {
  ulint len = 0;

  ut_ad(m_block != nullptr);

  plist_base_node_t free_lst = free_list();
  for (plist_node_t cur = free_lst.get_first_node(); !cur.is_null();
       cur = cur.get_next_node()) {
    frag_node_t frag(cur, m_mtr);
    len += frag.payload();
  }
  return (len);
}

/** Get the big free space in this page. */
ulint z_frag_page_t::get_big_free_len() const {
  ulint big = 0;

  ut_ad(m_block != nullptr);

  plist_base_node_t free_lst = free_list();
  for (plist_node_t cur = free_lst.get_first_node(); !cur.is_null();
       cur = cur.get_next_node()) {
    frag_node_t frag(cur, m_mtr);

    /* Use the total length (including the meta data overhead) of the
    fragment. */
    ulint total_free = frag.get_total_len();
    if (total_free > big) {
      big = total_free;
    }
  }

  return (big);
}

/** Deallocate all the free slots from the end of the page directory. */
void z_frag_page_t::dealloc_frag_id() {
  plist_base_node_t free_lst = free_list();
  plist_node_t last = free_lst.get_last_node();
  frag_node_t frag(last, m_mtr);
  /* The last free fragment must be adjacent to the directory.
  Then only it can take space from one slot. */
  if (frag.end_ptr() != slots_end_ptr()) {
    return;
  }

  ulint frag_id = get_n_dir_entries() - 1;
  paddr_t addr = frag_id_to_addr(frag_id);
  while (addr == 0) {
    frag.incr_length_by_2();
    decr_n_dir_entries();
    if (frag_id == 0) {
      break;
    }
    frag_id--;
    addr = frag_id_to_addr(frag_id);
  }
}

/** Insert a large object (LOB) into the system.
@param[in]      ctx    the B-tree context for this LOB operation.
@param[in]      trx    transaction doing the insertion.
@param[in,out]  ref    the LOB reference.
@param[in]      field  the LOB field.
@return DB_SUCCESS on success, error code on failure.*/
dberr_t insert(InsertContext *ctx, trx_t *trx, ref_t &ref,
               big_rec_field_t *field, ulint field_j) {
  const trx_id_t trxid = (trx == nullptr ? 0 : trx->id);
  const undo_no_t undo_no = (trx == nullptr ? 0 : trx->undo_no - 1);
  dberr_t ret = DB_SUCCESS;
  ulint total_written = 0;
  const byte *ptr = field->ptr();
  ulint len = field->len;
  mtr_t *mtr = ctx->get_mtr();
  dict_index_t *index = ctx->index();
  space_id_t space_id = dict_index_get_space(index);
  page_size_t page_size(dict_table_page_size(index->table));
  DBUG_ENTER("lob::insert");

  if (ref.length() > 0) {
    ref.set_length(0, mtr);
  }

  if (!ref_t::is_big(page_size, len)) {
    /* The LOB is not big enough to build LOB index. Insert the LOB without an
    LOB index. */
    Inserter blob_writer(ctx);
    DBUG_RETURN(blob_writer.write_one_small_blob(field_j));
  }

  ut_ad(ref_t::is_big(page_size, len));

  DBUG_LOG("lob", PrintBuffer(ptr, len));
  ut_ad(ref.validate(ctx->get_mtr()));

  first_page_t first(mtr, index);
  buf_block_t *first_block = first.alloc(mtr, ctx->is_bulk());

  if (first_block == nullptr) {
    /* Allocation of the first page of LOB failed. */
    DBUG_RETURN(DB_OUT_OF_FILE_SPACE);
  }

  first.set_last_trx_id(trxid);
  first.init_lob_version();

  page_no_t first_page_no = first.get_page_no();

  if (dict_index_is_online_ddl(index)) {
    row_log_table_blob_alloc(index, first_page_no);
  }

  page_id_t first_page_id(space_id, first_page_no);

  flst_base_node_t *index_list = first.index_list();

  ulint to_write = first.write(trxid, ptr, len);
  total_written += to_write;
  ulint remaining = len;

  {
    /* Insert an index entry in LOB index. */
    flst_node_t *node = first.alloc_index_entry(ctx->is_bulk());

    /* Here the first index entry is being allocated.  Since this will be
    allocated in the first page of LOB, it cannot be nullptr. */
    ut_ad(node != nullptr);

    index_entry_t entry(node, mtr, index);
    entry.set_versions_null();
    entry.set_trx_id(trxid);
    entry.set_trx_id_modifier(trxid);
    entry.set_trx_undo_no(undo_no);
    entry.set_trx_undo_no_modifier(undo_no);
    entry.set_page_no(first.get_page_no());
    entry.set_data_len(to_write);
    flst_add_last(index_list, node, mtr);

    first.set_trx_id(trxid);
    first.set_data_len(to_write);
  }

  ulint nth_blob_page = 0;
  const ulint commit_freq = 4;

  while (remaining > 0) {
    data_page_t data_page(mtr, index);
    buf_block_t *block = data_page.alloc(mtr, ctx->is_bulk());

    if (block == nullptr) {
      ret = DB_OUT_OF_FILE_SPACE;
      break;
    }

    to_write = data_page.write(trxid, ptr, remaining);
    total_written += to_write;
    data_page.set_trx_id(trxid);

    /* Allocate a new index entry */
    flst_node_t *node = first.alloc_index_entry(ctx->is_bulk());

    if (node == nullptr) {
      ret = DB_OUT_OF_FILE_SPACE;
      break;
    }

    index_entry_t entry(node, mtr, index);
    entry.set_versions_null();
    entry.set_trx_id(trxid);
    entry.set_trx_id_modifier(trxid);
    entry.set_trx_undo_no(undo_no);
    entry.set_trx_undo_no_modifier(undo_no);
    entry.set_page_no(data_page.get_page_no());
    entry.set_data_len(to_write);
    entry.push_back(first.index_list());

    ut_ad(!entry.get_self().is_equal(entry.get_prev()));
    ut_ad(!entry.get_self().is_equal(entry.get_next()));

    page_type_t type = fil_page_get_type(block->frame);
    ut_a(type == FIL_PAGE_TYPE_LOB_DATA);

    if (++nth_blob_page % commit_freq == 0) {
      ctx->check_redolog();
      ref.set_ref(ctx->get_field_ref(field->field_no));
      first.load_x(first_page_id, page_size);
    }
  }

  if (ret == DB_SUCCESS) {
    ref.update(space_id, first_page_no, 1, mtr);
    ref.set_length(total_written, mtr);
  }

  DBUG_EXECUTE_IF("innodb_lob_print",
                  print(trx, index, std::cerr, ref, false););

  DBUG_EXECUTE_IF("btr_store_big_rec_extern", ret = DB_OUT_OF_FILE_SPACE;);
  DBUG_RETURN(ret);
}

/** Fetch a large object (LOB) from the system.
@param[in]  ctx    the read context information.
@param[in]  ref    the LOB reference identifying the LOB.
@param[in]  offset read the LOB from the given offset.
@param[in]  len    the length of LOB data that needs to be fetched.
@param[out] buf    the output buffer (owned by caller) of minimum len bytes.
@return the amount of data (in bytes) that was actually read. */
ulint read(ReadContext *ctx, ref_t ref, ulint offset, ulint len, byte *buf) {
  DBUG_ENTER("lob::read");
  ut_ad(offset == 0);
  const uint32_t lob_version = ref.version();

  ref_mem_t ref_mem;
  ref.parse(ref_mem);

#ifdef LOB_DEBUG
  std::cout << "thread=" << std::this_thread::get_id()
            << ", lob::read(): table=" << ctx->index()->table->name
            << ", ref=" << ref << std::endl;
#endif /* LOB_DEBUG */

  /* Cache of s-latched blocks of LOB index pages.*/
  BlockCache cached_blocks;

  ut_ad(len > 0);

  /* Obtain length of LOB available in clustered index.*/
  const ulint avail_lob = ref.length();

  if (avail_lob == 0) {
    DBUG_RETURN(0);
  }

  ut_ad(ctx->m_index->is_clustered());

  ulint total_read = 0;
  ulint actual_read = 0;
  page_no_t page_no = ref.page_no();
  const page_id_t page_id(ctx->m_space_id, page_no);
  mtr_t mtr;

  mtr_start(&mtr);

  first_page_t first_page(&mtr, ctx->m_index);
  first_page.load_s(page_id, ctx->m_page_size);

  ulint page_type = first_page.get_page_type();

  if (page_type == FIL_PAGE_TYPE_BLOB || page_type == FIL_PAGE_SDI_BLOB) {
    mtr_commit(&mtr);
    Reader reader(*ctx);
    ulint fetch_len = reader.fetch();
    DBUG_RETURN(fetch_len);
  }

  ut_ad(page_type == FIL_PAGE_TYPE_LOB_FIRST);

  cached_blocks.insert(
      std::pair<page_no_t, buf_block_t *>(page_no, first_page.get_block()));

  ctx->m_lob_version = first_page.get_lob_version();

  page_no_t first_page_no = first_page.get_page_no();

  flst_base_node_t *base_node = first_page.index_list();

  fil_addr_t node_loc = flst_get_first(base_node, &mtr);
  flst_node_t *node = nullptr;

  /* Total bytes that have been skipped in this LOB */
  ulint skipped = 0;

  index_entry_t cur_entry(&mtr, ctx->m_index);
  index_entry_t old_version(&mtr, ctx->m_index);
  index_entry_mem_t entry_mem;

  ut_ad(offset >= skipped);

  ulint page_offset = offset - skipped;
  ulint want = len;
  byte *ptr = buf;

  /* Use a different mtr for data pages. */
  mtr_t data_mtr;
  mtr_start(&data_mtr);
  const ulint commit_freq = 10;
  ulint data_pages_count = 0;

  while (!fil_addr_is_null(node_loc) && want > 0) {
    old_version.reset(nullptr);

    node = first_page.addr2ptr_s_cache(cached_blocks, node_loc);
    cur_entry.reset(node);

    cur_entry.read(entry_mem);

    const uint32_t entry_lob_version = cur_entry.get_lob_version();

    if (entry_lob_version > lob_version) {
      flst_base_node_t *ver_list = cur_entry.get_versions_list();
      /* Look at older versions. */
      fil_addr_t node_versions = flst_get_first(ver_list, &mtr);

      while (!fil_addr_is_null(node_versions)) {
        flst_node_t *node_old_version =
            first_page.addr2ptr_s_cache(cached_blocks, node_versions);

        old_version.reset(node_old_version);

        old_version.read(entry_mem);

        const uint32_t old_lob_version = old_version.get_lob_version();

        if (old_lob_version <= lob_version) {
          /* The current trx can see this
          entry. */
          break;
        }
        node_versions = old_version.get_next();
        old_version.reset(nullptr);
      }
    }

    page_no_t read_from_page_no = FIL_NULL;

    if (old_version.is_null()) {
      read_from_page_no = cur_entry.get_page_no();
    } else {
      read_from_page_no = old_version.get_page_no();
    }

    actual_read = 0;
    if (read_from_page_no != FIL_NULL) {
      if (read_from_page_no == first_page_no) {
        actual_read = first_page.read(page_offset, ptr, want);
        ptr += actual_read;
        want -= actual_read;

      } else {
        buf_block_t *block =
            buf_page_get(page_id_t(ctx->m_space_id, read_from_page_no),
                         ctx->m_page_size, RW_S_LATCH, &data_mtr);

        data_page_t page(block, &data_mtr);
        actual_read = page.read(page_offset, ptr, want);
        ptr += actual_read;
        want -= actual_read;

        page_type_t type = page.get_page_type();
        ut_a(type == FIL_PAGE_TYPE_LOB_DATA);

        if (++data_pages_count % commit_freq == 0) {
          mtr_commit(&data_mtr);
          mtr_start(&data_mtr);
        }
      }
    }

    total_read += actual_read;
    page_offset = 0;
    node_loc = cur_entry.get_next();
  }

  /* Assert that we have read what has been requested or what is
  available. */
  ut_ad(total_read == len || total_read == avail_lob);
  ut_ad(total_read <= avail_lob);

  mtr_commit(&mtr);
  mtr_commit(&data_mtr);
  DBUG_RETURN(total_read);
}

buf_block_t *z_index_page_t::alloc(z_first_page_t &first, bool bulk) {
  ut_ad(m_block == nullptr);
  page_no_t hint = FIL_NULL;
  m_block = alloc_lob_page(m_index, m_mtr, hint, bulk);

  DBUG_EXECUTE_IF("innodb_lob_alloc_z_index_page_failed", m_block = nullptr;);

  if (m_block == nullptr) {
    return (nullptr);
  }

  set_page_type(m_mtr);
  set_version_0();
  flst_base_node_t *free_lst = first.free_list();
  init(free_lst, m_mtr);

  /* Link the allocated index page to the first page. */
  page_no_t page_no = first.get_index_page_no();
  set_next_page_no(page_no);
  first.set_index_page_no(get_page_no());
  return (m_block);
}

/** Allocate one data page.
@param[in]	hint	hint page number for allocation.
@param[in]	bulk	true if bulk operation (OPCODE_INSERT_BULK)
                        false otherwise.
@return the allocated buffer block. */
buf_block_t *z_data_page_t::alloc(page_no_t hint, bool bulk) {
  ut_ad(m_block == nullptr);
  m_block = alloc_lob_page(m_index, m_mtr, hint, bulk);

  DBUG_EXECUTE_IF("innodb_lob_alloc_z_data_page_failed", m_block = nullptr;);

  if (m_block == nullptr) {
    return (nullptr);
  }

  init();
  return (m_block);
}

void z_index_page_t::init(flst_base_node_t *free_lst, mtr_t *mtr) {
  ulint n = get_n_index_entries();
  for (ulint i = 0; i < n; ++i) {
    byte *ptr = frame() + LOB_PAGE_DATA;
    ptr += (i * z_index_entry_t::SIZE);
    z_index_entry_t entry(ptr, mtr);
    entry.init();
    entry.push_back(free_lst);
  }
}

ulint z_index_page_t::get_n_index_entries() const {
  return (payload() / z_index_entry_t::SIZE);
}

ulint node_page_t::node_count() {
  return (max_space_available() / index_entry_t::SIZE);
}

void node_page_t::import(trx_id_t trx_id) {
  ulint nc = node_count();
  byte *cur = nodes_begin();

  /* Update the trx id */
  for (ulint i = 0; i < nc; ++i) {
    index_entry_t entry(cur, m_mtr, m_index);
    entry.set_trx_id_no_redo(trx_id);
    entry.set_trx_id_modifier_no_redo(trx_id);

    cur += index_entry_t::SIZE;
  }
}

/** Print information about the given LOB.
@param[in]  trx  the current transaction.
@param[in]  index  the clust index that contains the LOB.
@param[in]  out    the output stream into which LOB info is printed.
@param[in]  ref    the LOB reference
@param[in]  fatal  if true assert at end of function. */
void print(trx_t *trx, dict_index_t *index, std::ostream &out, ref_t ref,
           bool fatal) {
  trx_id_t trxid = (trx == nullptr ? 0 : trx->id);

  out << "[lob::print: trx_id=" << trxid << ", ";

  mtr_t mtr;

  /* Print the lob reference object. */
  space_id_t space_id = ref.space_id();
  page_no_t page_no = ref.page_no();
  ulint avail_lob = ref.length();

  out << "avail_lob=" << avail_lob << ", ";
  out << ref;

  const page_id_t first_page_id(space_id, page_no);
  const page_size_t page_size = dict_table_page_size(index->table);

  /* Load the first page of LOB */
  mtr_start(&mtr);

  first_page_t first_page(&mtr, index);
  first_page.load_x(first_page_id, page_size);

  first_page.print_index_entries(out);
  mtr_commit(&mtr);
  out << "]";

  if (fatal) {
    ut_error;
  }
}

void z_index_page_t::import(trx_id_t trx_id) {
  ulint n = get_n_index_entries();
  for (ulint i = 0; i < n; ++i) {
    byte *ptr = frame() + LOB_PAGE_DATA;
    ptr += (i * z_index_entry_t::SIZE);
    z_index_entry_t entry(ptr);
    entry.set_trx_id_no_redo(trx_id);
    entry.set_trx_id_modifier_no_redo(trx_id);
  }
}

}  // namespace lob
