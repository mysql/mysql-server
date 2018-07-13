/*****************************************************************************

Copyright (c) 2015, 2018, Oracle and/or its affiliates. All Rights Reserved.

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

#include <sys/types.h>

#include "btr0pcur.h"
#include "fil0fil.h"
#include "lob0first.h"
#include "lob0inf.h"
#include "lob0lob.h"
#include "lob0zip.h"
#include "row0upd.h"
#include "zlob0first.h"

#include "my_dbug.h"

namespace lob {

/** A BLOB field reference has all the bits set to zero, except the "being
 * modified" bit. */
const byte field_ref_almost_zero[FIELD_REF_SIZE] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x20, 0, 0, 0, 0, 0, 0, 0,
};

#ifdef UNIV_DEBUG
bool ReadContext::assert_read_uncommitted() const {
  ut_ad(m_trx == nullptr || m_trx->is_read_uncommitted());
  return (true);
}
#endif /* UNIV_DEBUG */

/** Gets the offset of the pointer to the externally stored part of a field.
@param[in]	offsets		array returned by rec_get_offsets()
@param[in]	n		index of the external field
@return offset of the pointer to the externally stored part */
ulint btr_rec_get_field_ref_offs(const ulint *offsets, ulint n) {
  ulint field_ref_offs;
  ulint local_len;

  ut_a(rec_offs_nth_extern(offsets, n));
  field_ref_offs = rec_get_nth_field_offs(offsets, n, &local_len);
  ut_a(rec_field_not_null_not_add_col_def(local_len));
  ut_a(local_len >= BTR_EXTERN_FIELD_REF_SIZE);

  return (field_ref_offs + local_len - BTR_EXTERN_FIELD_REF_SIZE);
}

/** Marks non-updated off-page fields as disowned by this record.
The ownership must be transferred to the updated record which is
inserted elsewhere in the index tree. In purge only the owner of
externally stored field is allowed to free the field.
@param[in]	update		update vector. */
void BtrContext::disown_inherited_fields(const upd_t *update) {
  ut_ad(rec_offs_validate());
  ut_ad(!rec_offs_comp(m_offsets) || !rec_get_node_ptr_flag(m_rec));
  ut_ad(rec_offs_any_extern(m_offsets));
  ut_ad(m_mtr);

  for (ulint i = 0; i < rec_offs_n_fields(m_offsets); i++) {
    if (rec_offs_nth_extern(m_offsets, i) &&
        !upd_get_field_by_field_no(update, i, false)) {
      set_ownership_of_extern_field(i, FALSE);
    }
  }
}

/** When bulk load is being done, check if there is enough space in redo
log file. */
void BtrContext::check_redolog_bulk() {
  ut_ad(is_bulk());

  FlushObserver *observer = m_mtr->get_flush_observer();

  rec_block_fix();

  commit_btr_mtr();

  DEBUG_SYNC_C("blob_write_middle");

  log_free_check();

  start_btr_mtr();
  m_mtr->set_flush_observer(observer);

  rec_block_unfix();
  ut_ad(validate());
}

/** Check if there is enough space in log file. Commit and re-start the
mini transaction. */
void BtrContext::check_redolog_normal() {
  ut_ad(!is_bulk());

  FlushObserver *observer = m_mtr->get_flush_observer();
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

/** Print this blob directory into the given output stream.
@param[in]	out	the output stream.
@return the output stream. */
std::ostream &blob_dir_t::print(std::ostream &out) const {
  out << "[blob_dir_t: ";
  for (const blob_page_info_t &info : m_pages) {
    out << info;
  }
  out << "]";
  return (out);
}

/** Print this blob_page_into_t object into the given output stream.
@param[in]	out	the output stream.
@return the output stream. */
std::ostream &blob_page_info_t::print(std::ostream &out) const {
  out << "[blob_page_info_t: m_page_no=" << m_page_no << ", m_bytes=" << m_bytes
      << ", m_zbytes=" << m_zbytes << "]";
  return (out);
}

/** Do setup of the zlib stream.
@return code returned by zlib. */
int zReader::setup_zstream() {
  const ulint local_prefix = m_rctx.m_local_len - BTR_EXTERN_FIELD_REF_SIZE;

  m_stream.next_out = m_rctx.m_buf + local_prefix;
  m_stream.avail_out = static_cast<uInt>(m_rctx.m_len - local_prefix);
  m_stream.next_in = Z_NULL;
  m_stream.avail_in = 0;

  /* Zlib inflate needs 32 kilobytes for the default
  window size, plus a few kilobytes for small objects. */
  m_heap = mem_heap_create(40000);
  page_zip_set_alloc(&m_stream, m_heap);

  int err = inflateInit(&m_stream);
  return (err);
}

/** Fetch the BLOB.
@return DB_SUCCESS on success, DB_FAIL on error. */
dberr_t zReader::fetch() {
  DBUG_ENTER("zReader::fetch");

  dberr_t err = DB_SUCCESS;

  ut_ad(m_rctx.is_valid_blob());
  ut_ad(assert_empty_local_prefix());

  ut_d(m_page_type_ex =
           m_rctx.is_sdi() ? FIL_PAGE_SDI_ZBLOB : FIL_PAGE_TYPE_ZBLOB);

  setup_zstream();

  m_remaining = m_rctx.m_blobref.length();

  while (m_rctx.m_page_no != FIL_NULL) {
    page_no_t curr_page_no = m_rctx.m_page_no;

    err = fetch_page();
    if (err != DB_SUCCESS) {
      break;
    }

    m_stream.next_in = m_bpage->zip.data + m_rctx.m_offset;
    m_stream.avail_in =
        static_cast<uInt>(m_rctx.m_page_size.physical() - m_rctx.m_offset);

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
        ib::error(ER_IB_MSG_630)
            << "inflate() of compressed BLOB page "
            << page_id_t(m_rctx.m_space_id, curr_page_no) << " returned "
            << zlib_err << " (" << m_stream.msg << ")";
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
bool zReader::assert_empty_local_prefix() {
  ut_ad(m_rctx.m_local_len == BTR_EXTERN_FIELD_REF_SIZE);
  return (true);
}
#endif /* UNIV_DEBUG */

dberr_t zReader::fetch_page() {
  dberr_t err(DB_SUCCESS);

  m_bpage = buf_page_get_zip(page_id_t(m_rctx.m_space_id, m_rctx.m_page_no),
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

  return (err);
}

/** This is used to take action when we enter and exit a scope.  When we enter
the scope the constructor will set the "being modified" bit in the lob reference
objects that are either being inserted or updated.  When we exit the scope the
destructor will clear the "being modified" bit in the lob reference objects. */
struct Being_modified {
  /** Constructor.  Set the "being modified" bit in LOB references. */
  Being_modified(const big_rec_t *big_rec_vec, btr_pcur_t *pcur, ulint *offsets,
                 opcode op)
      : m_big_rec_vec(big_rec_vec), m_pcur(pcur), m_offsets(offsets), m_op(op) {
    /* All pointers to externally stored columns in the record
    must either be zero or they must be pointers to inherited
    columns, owned by this record or an earlier record version. */
    rec_t *rec = btr_pcur_get_rec(m_pcur);
#ifdef UNIV_DEBUG
    dict_index_t *index = m_pcur->index();
    rec_offs_make_valid(rec, index, m_offsets);
#endif /* UNIV_DEBUG */
    for (uint i = 0; i < m_big_rec_vec->n_fields; i++) {
      ulint field_no = m_big_rec_vec->fields[i].field_no;
      byte *field_ref = btr_rec_get_field_ref(rec, m_offsets, field_no);
      ref_t blobref(field_ref);

      /* Before we release latches in a subsequent ctx.check_redolog() call,
      mark the blobs as being modified.  This is needed to ensure that READ
      UNCOMMITTED transactions don't read an inconsistent BLOB. */
      blobref.set_being_modified(true);

#if defined UNIV_DEBUG || defined UNIV_BLOB_LIGHT_DEBUG

      /* Make a in-memory copy of the LOB ref. */
      ref_mem_t ref_mem;
      blobref.parse(ref_mem);

      ut_a(blobref.is_owner());
      /* Either this must be an update in place,
      or the BLOB must be inherited, or the BLOB pointer
      must be zero (will be written in this function). */
      ut_a(m_op == OPCODE_UPDATE || m_op == OPCODE_INSERT_UPDATE ||
           blobref.is_inherited() || blobref.is_null_relaxed());
      ut_ad(blobref.is_being_modified());

#endif /* UNIV_DEBUG || UNIV_BLOB_LIGHT_DEBUG */
    }
  }

  /** Destructor.  Clear the "being modified" bit in LOB references. */
  ~Being_modified() {
    rec_t *rec = btr_pcur_get_rec(m_pcur);
#ifdef UNIV_DEBUG
    dict_index_t *index = m_pcur->index();
    rec_offs_make_valid(rec, index, m_offsets);
#endif /* UNIV_DEBUG */
    for (uint i = 0; i < m_big_rec_vec->n_fields; i++) {
      ulint field_no = m_big_rec_vec->fields[i].field_no;
      byte *field_ref = btr_rec_get_field_ref(rec, m_offsets, field_no);
      ref_t blobref(field_ref);

      blobref.set_being_modified(false);
    }
  }

  const big_rec_t *m_big_rec_vec;
  btr_pcur_t *m_pcur;
  ulint *m_offsets;
  opcode m_op;
};

/** Stores the fields in big_rec_vec to the tablespace and puts pointers to
them in rec.  The extern flags in rec will have to be set beforehand. The
fields are stored on pages allocated from leaf node file segment of the index
tree.

TODO: If the allocation extends the tablespace, it will not be redo logged, in
any mini-transaction.  Tablespace extension should be redo-logged, so that
recovery will not fail when the big_rec was written to the extended portion of
the file, in case the file was somehow truncated in the crash.
@param[in]	trx		the trx doing LOB store. If unavailable it
                                could be nullptr.
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
dberr_t btr_store_big_rec_extern_fields(trx_t *trx, btr_pcur_t *pcur,
                                        const upd_t *upd, ulint *offsets,
                                        const big_rec_t *big_rec_vec,
                                        mtr_t *btr_mtr, opcode op) {
  mtr_t mtr;
  mtr_t mtr_bulk;
  page_zip_des_t *page_zip;
  dberr_t error = DB_SUCCESS;
  dict_index_t *index = pcur->index();
  dict_table_t *table = index->table;
  buf_block_t *rec_block = btr_pcur_get_block(pcur);
  rec_t *rec = btr_pcur_get_rec(pcur);

  ut_ad(rec_offs_validate(rec, index, offsets));
  ut_ad(rec_offs_any_extern(offsets));
  ut_ad(btr_mtr);
  ut_ad(mtr_memo_contains_flagged(btr_mtr, dict_index_get_lock(index),
                                  MTR_MEMO_X_LOCK | MTR_MEMO_SX_LOCK) ||
        index->table->is_intrinsic());
  ut_ad(
      mtr_is_block_fix(btr_mtr, rec_block, MTR_MEMO_PAGE_X_FIX, index->table));
  ut_ad(buf_block_get_frame(rec_block) == page_align(rec));
  ut_a(index->is_clustered());

  ut_a(dict_table_page_size(table).equals_to(rec_block->page.size));

  /* Create a blob operation context. */
  BtrContext btr_ctx(btr_mtr, pcur, index, rec, offsets, rec_block, op);
  InsertContext ctx(btr_ctx, big_rec_vec);

  Being_modified bm(big_rec_vec, pcur, offsets, op);

  /* The pcur could be re-positioned.  Commit and restart btr_mtr. */
  ctx.check_redolog();
  rec_block = btr_pcur_get_block(pcur);
  rec = btr_pcur_get_rec(pcur);

  page_zip = buf_block_get_page_zip(rec_block);
  ut_a(fil_page_index_page_check(page_align(rec)) || op == OPCODE_INSERT_BULK);

  if (page_zip != NULL) {
    DBUG_EXECUTE_IF("lob_insert_single_zstream",
                    { goto insert_single_zstream; });

    if (dict_index_is_sdi(index)) {
      goto insert_single_zstream;
    }

  } else {
    /* Uncompressed LOB */

    DBUG_EXECUTE_IF("lob_insert_noindex", { goto insert_noindex; });

    if (dict_index_is_sdi(index)) {
      goto insert_noindex;
    }
  }

  for (uint i = 0; i < big_rec_vec->n_fields; i++) {
    ulint field_no = big_rec_vec->fields[i].field_no;

    /* Cursor could have changed position. */
    rec = btr_pcur_get_rec(pcur);
    rec_offs_make_valid(rec, index, offsets);
    ut_ad(rec_offs_validate(rec, index, offsets));

    byte *field_ref = btr_rec_get_field_ref(rec, offsets, field_no);

    ref_t blobref(field_ref);
    ut_ad(blobref.validate(btr_mtr));

    bool can_do_partial_update = false;

    if (op == lob::OPCODE_UPDATE && upd != nullptr &&
        big_rec_vec->fields[i].ext_in_old) {
      can_do_partial_update = blobref.is_lob_partially_updatable(index);
    }

    if (page_zip != nullptr) {
      bool do_insert = true;

      if (op == lob::OPCODE_UPDATE && upd != nullptr &&
          blobref.is_big(rec_block->page.size) && can_do_partial_update) {
        if (upd->is_partially_updated(field_no)) {
          /* Do partial update. */
          error = lob::z_update(ctx, trx, index, upd, field_no, blobref);
          switch (error) {
            case DB_SUCCESS:
              do_insert = false;
              break;
            case DB_FAIL:
              break;
            default:
              ut_error;
          }
        } else {
          /* This is to inform the purge thread that
          the older version LOB in this update operation
          can be freed. */
          blobref.mark_not_partially_updatable(trx, btr_mtr, index,
                                               dict_table_page_size(table));
        }
      }

      if (do_insert) {
        error = lob::z_insert(&ctx, trx, blobref, &big_rec_vec->fields[i], i);

        if (op == lob::OPCODE_UPDATE && upd != nullptr) {
          /* Get the corresponding upd_field_t
          object.*/
          upd_field_t *uf = upd->get_field_by_field_no(field_no, index);

          if (uf != nullptr) {
            /* Update the LOB reference
            stored in upd_field_t */
            dfield_t *new_val = &uf->new_val;

            if (dfield_is_ext(new_val)) {
              byte *field_ref = new_val->blobref();
              blobref.copy(field_ref);
            }
          }
        }
      }

    } else {
      /* Uncompressed LOB */
      bool do_insert = true;

      if (op == lob::OPCODE_UPDATE && upd != nullptr &&
          blobref.is_big(rec_block->page.size) && can_do_partial_update) {
        if (upd->is_partially_updated(field_no)) {
          /* Do partial update. */
          error = lob::update(ctx, trx, index, upd, field_no, blobref);
          switch (error) {
            case DB_SUCCESS:
              do_insert = false;
              break;
            case DB_FAIL:
              break;
            case DB_OUT_OF_FILE_SPACE:
              break;
            default:
              ut_error;
          }

        } else {
          /* This is to inform the purge thread that
          the older version LOB in this update operation
          can be freed. */
          blobref.mark_not_partially_updatable(trx, btr_mtr, index,
                                               dict_table_page_size(table));
        }
      }

      if (do_insert) {
        error = lob::insert(&ctx, trx, blobref, &big_rec_vec->fields[i], i);

        if (op == lob::OPCODE_UPDATE && upd != nullptr) {
          /* Get the corresponding upd_field_t
          object.*/
          upd_field_t *uf = upd->get_field_by_field_no(field_no, index);

          if (uf != nullptr) {
            /* Update the LOB reference
            stored in upd_field_t */
            dfield_t *new_val = &uf->new_val;
            if (dfield_is_ext(new_val)) {
              byte *field_ref = new_val->blobref();
              blobref.copy(field_ref);
            }
          }
        }
      }
    }

    if (error != DB_SUCCESS) {
      break;
    }

#ifdef UNIV_DEBUG
    /* Ensure that the LOB references are valid now. */
    rec = btr_pcur_get_rec(pcur);
    rec_offs_make_valid(rec, index, offsets);
    field_ref =
        btr_rec_get_field_ref(rec, offsets, big_rec_vec->fields[i].field_no);
    ref_t lobref(field_ref);

    ut_ad(!lobref.is_null());
#endif /* UNIV_DEBUG */
  }
  return (error);

  {
  insert_single_zstream:
    /* Insert the LOB as a single zlib stream spanning multiple
    LOB pages.  This is the old way of storing LOBs. */
    zInserter zblob_writer(&ctx);
    error = zblob_writer.prepare();
    if (error == DB_SUCCESS) {
      zblob_writer.write();
      error = zblob_writer.finish();
    }
    return (error);
  }
  {
  insert_noindex:
    /* Insert the uncompressed LOB without LOB index. */
    Inserter blob_writer(&ctx);
    error = blob_writer.write();
    return (error);
  }
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
byte *btr_rec_copy_externally_stored_field_func(
    trx_t *trx, const dict_index_t *index, const rec_t *rec,
    const ulint *offsets, const page_size_t &page_size, ulint no, ulint *len,
    size_t *lob_version,
#ifdef UNIV_DEBUG
    bool is_sdi,
#endif /* UNIV_DEBUG */
    mem_heap_t *heap) {
  ulint local_len;
  const byte *data;

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
  const byte *field_ref = data + local_len - BTR_EXTERN_FIELD_REF_SIZE;

  lob::ref_t ref(const_cast<byte *>(field_ref));

  ut_a(local_len >= BTR_EXTERN_FIELD_REF_SIZE);

  if (ref.is_null()) {
    /* The externally stored field was not written yet.
    This record should only be seen by
    trx_rollback_or_clean_all_recovered() or any
    TRX_ISO_READ_UNCOMMITTED transactions. */

    lob::ref_mem_t ref_mem;
    ref.parse(ref_mem);
    return (NULL);
  }

  return (btr_copy_externally_stored_field(trx, index, len, lob_version, data,
                                           page_size, local_len, is_sdi, heap));
}

/** Returns the page number where the next BLOB part is stored.
@param[in]	blob_header	the BLOB header.
@return page number or FIL_NULL if no more pages */
static inline page_no_t btr_blob_get_next_page_no(const byte *blob_header) {
  return (mach_read_from_4(blob_header + LOB_HDR_NEXT_PAGE_NO));
}

/** Check the FIL_PAGE_TYPE on an uncompressed BLOB page.
@param[in]	space_id	space identifier.
@param[in]	page_no		page number.
@param[in]	page		the page
@param[in]	read		TRUE=read, FALSE=purge */
static void btr_check_blob_fil_page_type(space_id_t space_id, page_no_t page_no,
                                         const page_t *page, ibool read) {
  ulint type = fil_page_get_type(page);

  ut_a(space_id == page_get_space_id(page));
  ut_a(page_no == page_get_page_no(page));

  switch (type) {
    ulint flags;
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

      ib::fatal(ER_IB_MSG_631)
          << "FIL_PAGE_TYPE=" << type << " on BLOB "
          << (read ? "read" : "purge") << " space " << space_id << " page "
          << page_no << " flags " << flags;
  }
}

/** Returns the length of a BLOB part stored on the header page.
@param[in]	blob_header	the BLOB header.
@return part length */
static inline ulint btr_blob_get_part_len(const byte *blob_header) {
  return (mach_read_from_4(blob_header + LOB_HDR_PART_LEN));
}

/** Fetch one BLOB page. */
void Reader::fetch_page() {
  mtr_t mtr;

  /* Bytes of LOB data available in the current LOB page. */
  ulint part_len;

  /* Bytes of LOB data obtained from the current LOB page. */
  ulint copy_len;

  ut_ad(m_rctx.m_page_no != FIL_NULL);
  ut_ad(m_rctx.m_page_no > 0);

  mtr_start(&mtr);

  m_cur_block = buf_page_get(page_id_t(m_rctx.m_space_id, m_rctx.m_page_no),
                             m_rctx.m_page_size, RW_S_LATCH, &mtr);
  buf_block_dbg_add_level(m_cur_block, SYNC_EXTERN_STORAGE);
  page_t *page = buf_block_get_frame(m_cur_block);

  btr_check_blob_fil_page_type(m_rctx.m_space_id, m_rctx.m_page_no, page, TRUE);

  byte *blob_header = page + m_rctx.m_offset;
  part_len = btr_blob_get_part_len(blob_header);
  copy_len = ut_min(part_len, m_rctx.m_len - m_copied_len);

  memcpy(m_rctx.m_buf + m_copied_len, blob_header + LOB_HDR_SIZE, copy_len);

  m_copied_len += copy_len;
  m_rctx.m_page_no = btr_blob_get_next_page_no(blob_header);
  mtr_commit(&mtr);
  m_rctx.m_offset = FIL_PAGE_DATA;
}

/** Fetch the complete or prefix of the uncompressed LOB data.
@return bytes of LOB data fetched. */
ulint Reader::fetch() {
  if (m_rctx.m_blobref.is_null()) {
    ut_ad(m_copied_len == 0);
    return (m_copied_len);
  }

  while (m_copied_len < m_rctx.m_len) {
    if (m_rctx.m_page_no == FIL_NULL) {
      /* End of LOB has been reached. */
      break;
    }

    fetch_page();
  }

  /* Assure that we have fetched the requested amount or the LOB
  has ended. */
  ut_ad(m_copied_len == m_rctx.m_len || m_rctx.m_page_no == FIL_NULL);

  return (m_copied_len);
}

/** Copies the prefix of an externally stored field of a record.
The clustered index record must be protected by a lock or a page latch.
@param[in]	index		the clust index in which lob is read.
@param[out]	buf		the field, or a prefix of it
@param[in]	len		length of buf, in bytes
@param[in]	page_size	BLOB page size
@param[in]	data		'internally' stored part of the field
                                containing also the reference to the external
                                part; must be protected by a lock or a page
                                latch.
@param[in]	is_sdi		true for SDI indexes
@param[in]	local_len	length of data, in bytes
@return the length of the copied field, or 0 if the column was being
or has been deleted */
ulint btr_copy_externally_stored_field_prefix_func(trx_t *trx,
                                                   const dict_index_t *index,
                                                   byte *buf, ulint len,
                                                   const page_size_t &page_size,
                                                   const byte *data,
#ifdef UNIV_DEBUG
                                                   bool is_sdi,
#endif /* UNIV_DEBUG */
                                                   ulint local_len) {
  ut_a(local_len >= BTR_EXTERN_FIELD_REF_SIZE);

  if (page_size.is_compressed()) {
    ut_a(local_len == BTR_EXTERN_FIELD_REF_SIZE);

    ReadContext rctx(page_size, data, local_len, buf, len
#ifdef UNIV_DEBUG
                     ,
                     is_sdi
#endif /* UNIV_DEBUG */
    );

    rctx.m_index = const_cast<dict_index_t *>(index);
    rctx.m_trx = trx;

    /* Obtain length of LOB available in clustered index.*/
    ulint avail_lob = rctx.m_blobref.length();

    if (avail_lob == 0) {
      /* No LOB data available. */
      return (0);
    }

    /* Read the LOB data. */
    ulint fetch_len = lob::z_read(&rctx, rctx.m_blobref, 0, len, buf);

    /* Either fetch the requested length or fetch the complete
    LOB. If complete LOB is fetched, then it means that requested
    length is bigger than the available length. */
    ut_a(fetch_len == 0 || fetch_len == len ||
         (fetch_len == avail_lob && avail_lob < len));

    return (fetch_len);
  }

  local_len -= BTR_EXTERN_FIELD_REF_SIZE;

  if (UNIV_UNLIKELY(local_len >= len)) {
    memcpy(buf, data, len);
    return (len);
  }

  memcpy(buf, data, local_len);
  data += local_len;

  ut_a(memcmp(data, field_ref_zero, BTR_EXTERN_FIELD_REF_SIZE));

  if (!mach_read_from_4(data + BTR_EXTERN_LEN + 4)) {
    /* The externally stored part of the column has been
    (partially) deleted.  Signal the half-deleted BLOB
    to the caller. */

    return (0);
  }

  ReadContext rctx(page_size, data, local_len + BTR_EXTERN_FIELD_REF_SIZE,
                   buf + local_len, len
#ifdef UNIV_DEBUG
                   ,
                   false
#endif /* UNIV_DEBUG */
  );

  rctx.m_index = (dict_index_t *)index;
  rctx.m_trx = trx;

  ulint fetch_len = lob::read(&rctx, rctx.m_blobref, 0, len, buf + local_len);
  return (local_len + fetch_len);
}

/** Copies an externally stored field of a record to mem heap.
The clustered index record must be protected by a lock or a page latch.
@param[in]	trx		the current trx object or nullptr
@param[in]	index		the clust index in which lob is read.
@param[out]	len		length of the whole field
@param[out]	lob_version	LOB version number.
@param[in]	data		'internally' stored part of the field
                                containing also the reference to the external
                                part; must be protected by a lock or a page
                                latch.
@param[in]	page_size	BLOB page size
@param[in]	local_len	length of data
@param[in]	is_sdi		true for SDI Indexes
@param[in,out]	heap		mem heap
@return the whole field copied to heap */
byte *btr_copy_externally_stored_field_func(
    trx_t *trx, const dict_index_t *index, ulint *len, size_t *lob_version,
    const byte *data, const page_size_t &page_size, ulint local_len,
#ifdef UNIV_DEBUG
    bool is_sdi,
#endif /* UNIV_DEBUG */
    mem_heap_t *heap) {
  uint32_t extern_len;
  byte *buf;

  ut_a(index->is_clustered());

  ut_a(local_len >= BTR_EXTERN_FIELD_REF_SIZE);

  local_len -= BTR_EXTERN_FIELD_REF_SIZE;

  /* Currently a BLOB cannot be bigger than 4 GB; we
  leave the 4 upper bytes in the length field unused */

  extern_len = mach_read_from_4(data + local_len + BTR_EXTERN_LEN + 4);

  buf = (byte *)mem_heap_alloc(heap, local_len + extern_len);

  ReadContext rctx(page_size, data, local_len + BTR_EXTERN_FIELD_REF_SIZE,
                   buf + local_len, extern_len
#ifdef UNIV_DEBUG
                   ,
                   is_sdi
#endif /* UNIV_DEBUG */
  );

  rctx.m_index = (dict_index_t *)index;

  if (ref_t::is_being_modified(data + local_len)) {
    /* This is applicable only for READ UNCOMMITTED transactions because they
    don't take transaction locks. */
    *len = 0;
    return (buf);
  }

  ut_ad(extern_len > 0);

  if (page_size.is_compressed()) {
    ut_ad(local_len == 0);
    *len = 0;

    if (extern_len > 0) {
      *len = lob::z_read(&rctx, rctx.m_blobref, 0, extern_len, buf + local_len);
    }

    return (buf);
  } else {
    if (local_len > 0) {
      memcpy(buf, data, local_len);
    }

    ulint fetch_len =
        lob::read(&rctx, rctx.m_blobref, 0, extern_len, buf + local_len);

    *len = local_len + fetch_len;

    if (lob_version != nullptr) {
      *lob_version = rctx.m_lob_version;
    }

    return (buf);
  }
}

/** Frees the externally stored fields for a record, if the field
is mentioned in the update vector.
@param[in]	trx_id		the transaction identifier.
@param[in]	undo_no		undo number within a transaction whose
                                LOB is being freed.
@param[in]	update		update vector
@param[in]	rollback	performing rollback? */
void BtrContext::free_updated_extern_fields(trx_id_t trx_id, undo_no_t undo_no,
                                            const upd_t *update,
                                            bool rollback) {
  ulint n_fields;
  ulint i;
  ut_ad(rollback);

  ut_ad(rec_offs_validate());
  ut_ad(mtr_is_page_fix(m_mtr, m_rec, MTR_MEMO_PAGE_X_FIX, m_index->table));

  /* Free possible externally stored fields in the record */

  n_fields = upd_get_n_fields(update);

  for (i = 0; i < n_fields; i++) {
    const upd_field_t *ufield = upd_get_nth_field(update, i);

    if (rec_offs_nth_extern(m_offsets, ufield->field_no)) {
      ulint len;
      byte *data = rec_get_nth_field(m_rec, m_offsets, ufield->field_no, &len);
      ut_a(len >= BTR_EXTERN_FIELD_REF_SIZE);

      byte *field_ref = data + len - BTR_EXTERN_FIELD_REF_SIZE;

      DeleteContext ctx(*this, field_ref, ufield->field_no, rollback);

      ref_t lobref(field_ref);
      lob::purge(&ctx, m_index, trx_id, undo_no, lobref, 0, ufield);
    }
  }
}

/** Deallocate a buffer block that was reserved for a BLOB part.
@param[in]	index	index
@param[in]	block	buffer block
@param[in]	all	flag whether remove the compressed page
                        if there is one
@param[in]	mtr	mini-transaction to commit */
void blob_free(dict_index_t *index, buf_block_t *block, bool all, mtr_t *mtr) {
  buf_pool_t *buf_pool = buf_pool_from_block(block);
  page_id_t page_id(block->page.id.space(), block->page.id.page_no());
  bool freed = false;

  ut_ad(mtr_is_block_fix(mtr, block, MTR_MEMO_PAGE_X_FIX, index->table));

  mtr_commit(mtr);

  mutex_enter(&buf_pool->LRU_list_mutex);
  buf_page_mutex_enter(block);

  /* Only free the block if it is still allocated to
  the same file page. */

  if (buf_block_get_state(block) == BUF_BLOCK_FILE_PAGE &&
      page_id.equals_to(block->page.id)) {
    freed = buf_LRU_free_page(&block->page, all);

    if (!freed && all && block->page.zip.data &&
        buf_block_get_state(block) == BUF_BLOCK_FILE_PAGE &&
        page_id.equals_to(block->page.id)) {
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
ulint btr_push_update_extern_fields(dtuple_t *tuple, const upd_t *update,
                                    mem_heap_t *heap) {
  DBUG_ENTER("btr_push_update_extern_fields");

  ulint n_pushed = 0;
  ulint n;
  upd_field_t *uf;

  ut_ad(tuple);
  ut_ad(update);

  uf = update->fields;
  n = upd_get_n_fields(update);

  for (; n--; uf++) {
    if (dfield_is_ext(&uf->new_val)) {
      dfield_t *field = dtuple_get_nth_field(tuple, uf->field_no);

      if (dfield_is_ext(field)) {
        uf->ext_in_old = true;
      } else {
        uf->ext_in_old = false;
        dfield_set_ext(field);
        n_pushed++;
      }

      switch (uf->orig_len) {
        byte *data;
        ulint len;
        byte *buf;
        case 0:
          break;
        case BTR_EXTERN_FIELD_REF_SIZE:
          /* Restore the original locally stored
          part of the column.  In the undo log,
          InnoDB writes a longer prefix of externally
          stored columns, so that column prefixes
          in secondary indexes can be reconstructed. */
          dfield_set_data(field,
                          (byte *)dfield_get_data(field) +
                              dfield_get_len(field) - BTR_EXTERN_FIELD_REF_SIZE,
                          BTR_EXTERN_FIELD_REF_SIZE);
          dfield_set_ext(field);
          break;
        default:
          /* Reconstruct the original locally
          stored part of the column.  The data
          will have to be copied. */
          ut_a(uf->orig_len > BTR_EXTERN_FIELD_REF_SIZE);

          data = (byte *)dfield_get_data(field);
          len = dfield_get_len(field);

          buf = (byte *)mem_heap_alloc(heap, uf->orig_len);
          /* Copy the locally stored prefix. */
          memcpy(buf, data, uf->orig_len - BTR_EXTERN_FIELD_REF_SIZE);
          /* Copy the BLOB pointer. */
          memcpy(buf + uf->orig_len - BTR_EXTERN_FIELD_REF_SIZE,
                 data + len - BTR_EXTERN_FIELD_REF_SIZE,
                 BTR_EXTERN_FIELD_REF_SIZE);

          dfield_set_data(field, buf, uf->orig_len);
          dfield_set_ext(field);
      }
    }
  }

  DBUG_RETURN(n_pushed);
}

/** Gets the externally stored size of a record, in units of a database page.
@param[in]	rec	record
@param[in]	offsets	array returned by rec_get_offsets()
@return externally stored part, in units of a database page */
ulint btr_rec_get_externally_stored_len(const rec_t *rec,
                                        const ulint *offsets) {
  ulint n_fields;
  ulint total_extern_len = 0;
  ulint i;

  ut_ad(!rec_offs_comp(offsets) || !rec_get_node_ptr_flag(rec));

  if (!rec_offs_any_extern(offsets)) {
    return (0);
  }

  n_fields = rec_offs_n_fields(offsets);

  for (i = 0; i < n_fields; i++) {
    if (rec_offs_nth_extern(offsets, i)) {
      ulint extern_len = mach_read_from_4(
          btr_rec_get_field_ref(rec, offsets, i) + BTR_EXTERN_LEN + 4);

      total_extern_len += ut_calc_align(extern_len, UNIV_PAGE_SIZE);
    }
  }

  return (total_extern_len / UNIV_PAGE_SIZE);
}

/** Frees the externally stored fields for a record.
@param[in]	trx_id		transaction identifier whose LOB is
                                being freed.
@param[in]	undo_no		undo number within a transaction whose
                                LOB is being freed.
@param[in]	rollback	performing rollback?
@param[in]	rec_type	undo record type.*/
void BtrContext::free_externally_stored_fields(trx_id_t trx_id,
                                               undo_no_t undo_no, bool rollback,
                                               ulint rec_type) {
  ut_ad(rec_offs_validate());
  ut_ad(mtr_is_page_fix(m_mtr, m_rec, MTR_MEMO_PAGE_X_FIX, m_index->table));

  /* Free possible externally stored fields in the record */
  ut_ad(dict_table_is_comp(m_index->table) == !!rec_offs_comp(m_offsets));
  ulint n_fields = rec_offs_n_fields(m_offsets);

  for (ulint i = 0; i < n_fields; i++) {
    if (rec_offs_nth_extern(m_offsets, i)) {
      byte *field_ref = btr_rec_get_field_ref(m_rec, m_offsets, i);

      DeleteContext ctx(*this, field_ref, i, rollback);
      ref_t lobref(field_ref);

      upd_field_t *uf = nullptr;
      lob::purge(&ctx, m_index, trx_id, undo_no, lobref, rec_type, uf);
    }
  }
}

/** Load the first page of LOB and read its page type.
@param[in]	index			the index object.
@param[in]	page_size		the page size of LOB.
@param[out]	is_partially_updatable	is the LOB partially updatable.
@return the page type of first page of LOB.*/
ulint ref_t::get_lob_page_info(const dict_index_t *index,
                               const page_size_t &page_size,
                               bool &is_partially_updatable) const {
  mtr_t mtr;
  buf_block_t *block;
  ref_mem_t ref_mem;

  parse(ref_mem);

  mtr_start(&mtr);

  block = buf_page_get(page_id_t(ref_mem.m_space_id, ref_mem.m_page_no),
                       page_size, RW_S_LATCH, &mtr);

  page_type_t page_type = block->get_page_type();

  switch (page_type) {
    case FIL_PAGE_TYPE_LOB_FIRST: {
      first_page_t first_page(block, &mtr, (dict_index_t *)index);
      is_partially_updatable = first_page.can_be_partially_updated();
      break;
    }
    case FIL_PAGE_TYPE_ZLOB_FIRST: {
      z_first_page_t z_first_page(block, &mtr, (dict_index_t *)index);
      is_partially_updatable = z_first_page.can_be_partially_updated();
      break;
    }
    default:
      is_partially_updatable = false;
  }

  mtr_commit(&mtr);

  return (page_type);
}

/** Load the first page of the LOB and mark it as not partially
updatable anymore.
@param[in]	trx		the current transaction
@param[in]	mtr		the mini transaction context.
@param[in]	index		the index dictionary object.
@param[in]	page_size	the page size information. */
void ref_t::mark_not_partially_updatable(trx_t *trx, mtr_t *mtr,
                                         dict_index_t *index,
                                         const page_size_t &page_size) {
  buf_block_t *block;
  ref_mem_t ref_mem;

  parse(ref_mem);

  block = buf_page_get(page_id_t(ref_mem.m_space_id, ref_mem.m_page_no),
                       page_size, RW_X_LATCH, mtr);

  page_type_t page_type = block->get_page_type();

  switch (page_type) {
    case FIL_PAGE_TYPE_LOB_FIRST: {
      first_page_t first_page(block, mtr, (dict_index_t *)index);
      first_page.mark_cannot_be_partially_updated(trx);
      break;
    }
    case FIL_PAGE_TYPE_ZLOB_FIRST: {
      z_first_page_t z_first_page(block, mtr, (dict_index_t *)index);
      z_first_page.mark_cannot_be_partially_updated(trx);
      break;
    }
    default:
      /* do nothing */
      break;
  }
}

/** Check if the LOB can be partially updated. This is done by loading
the first page of LOB and looking at the flags.
@param[in]	index	the index to which LOB belongs.
@return true if LOB is partially updatable, false otherwise.*/
bool ref_t::is_lob_partially_updatable(const dict_index_t *index) const {
  if (is_null_relaxed()) {
    return (false);
  }

  const page_size_t page_size = dict_table_page_size(index->table);
  bool can_do_partial_update = false;
  ulint page_type = get_lob_page_info(index, page_size, can_do_partial_update);

  bool page_type_ok = (page_type == FIL_PAGE_TYPE_LOB_FIRST ||
                       page_type == FIL_PAGE_TYPE_ZLOB_FIRST);

  return (page_type_ok && can_do_partial_update);
}

std::ostream &ref_t::print(std::ostream &out) const {
  out << "[ref_t: m_ref=" << (void *)m_ref << ", space_id=" << space_id()
      << ", page_no=" << page_no() << ", offset=" << offset()
      << ", length=" << length() << "]";
  return (out);
}

/** Acquire an x-latch on the index page containing the clustered
index record, in the given mini transaction context.
@param[in]	mtr	the mini-transaction context. */
void DeleteContext::x_latch_rec_page(mtr_t *mtr) {
  bool found;
  page_t *rec_page = m_blobref.page_align();
  page_no_t rec_page_no = page_get_page_no(rec_page);
  space_id_t rec_space_id = page_get_space_id(rec_page);

  const page_size_t &rec_page_size =
      fil_space_get_page_size(rec_space_id, &found);
  ut_ad(found);

#ifdef UNIV_DEBUG
  buf_block_t *block =
#endif /* UNIV_DEBUG */
      buf_page_get(page_id_t(rec_space_id, rec_page_no), rec_page_size,
                   RW_X_LATCH, mtr);

  ut_ad(block != nullptr);
}

}  // namespace lob
