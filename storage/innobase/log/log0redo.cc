/*****************************************************************************

Copyright (c) 2023, 2026, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file log/log0redo.cc
 Redo log parsing and applying.

 ****************************************************************************/

#include "log0redo.h"

#include <list>
#include <map>
#include <memory>
#include <string_view>
#include <unordered_map>

#include "btr0cur.h"
#include "buf0buf.h"
#include "dict0mem.h"
#include "fsp0fsp.h"
#include "ibuf0ibuf.h"
#include "log0parsed_index_lru_cache.h"
#include "log0test.h"
#include "mtr0log.h"
#include "mtr0mtr.h"
#include "mtr0types.h"
#include "page0cur.h"
#include "page0zip.h"
#include "sync0rw.h"
#include "trx0rec.h"
#include "trx0undo.h"
#include "ut0dbg.h"
#include "ut0expected.h"

/* MEB related stuff.
TODO: Should be moved somewhere else. */
#ifdef UNIV_HOTBACKUP
/** Print important values from a page header.
@param[in]      page    page */
void meb_print_page_header(const page_t *page) {
  ib::trace_1() << "space_id " << mach_read_from_4(page + FIL_PAGE_SPACE_ID)
                << " page_nr " << mach_read_from_4(page + FIL_PAGE_OFFSET)
                << " lsn " << mach_read_from_8(page + FIL_PAGE_LSN) << " type "
                << mach_read_from_2(page + FIL_PAGE_TYPE);
}
#endif /* UNIV_HOTBACKUP */

namespace ib::redo {

class Redo_applier::Impl {
 public:
  Impl() : m_persisters(std::make_unique<Persisters>()) {
    m_persisters->add(PM_INDEX_CORRUPTED);
    m_persisters->add(PM_TABLE_AUTO_INC);
  }

  [[nodiscard]] Parse_result<Mtr_view> parse_mtr(
      std::span<const uint8_t> buffer) {
    Mtr_view mtr;
    size_t pos = 0;

    while (!buffer.empty()) {
      bool single_rec = buffer[0] & MLOG_SINGLE_REC_FLAG;

      if (single_rec && !mtr.empty()) {
        return ut::Unexpected(
            Parse_error{Parse_error::Corrupted}.with_pos(pos));
      }

      auto record = parse_record(buffer);
      if (!record) {
        /* Here we override record.error().pos() as:
        1. currently, parse_record does not call with_pos(..) at all
        2. error at "position 7th" within something which can't be parsed as
           any record in the end, might be actually caused by corruption at
           earlier position, perhaps even 0th
        3. currently, the error reporting shows only bytes starting at pos */
        return ut::Unexpected(record.error().with_pos(pos));
      }

      const int rec_type = record->type();
      const size_t rec_size = record->size();

      mtr.add_record(std::move(*record));

      if (single_rec || rec_type == MLOG_MULTI_REC_END) {
        return mtr;
      }

      buffer = buffer.subspan(rec_size);
      pos += rec_size;
    }

    return ut::Unexpected(Parse_error{Parse_error::Incomplete}.with_pos(pos));
  }

  [[nodiscard]] bool apply(const Record_handle &record_handle,
                           Page_handle &page_handle) {
    /** We don't want to ever apply MLOG_FILE_CREATE to a page, even to page 0,
    even though for legacy reasons the MLOG_FILE_CREATE starts with a header
    which looks as if it was a record targeting page_no=0 of a given space. */
    ut_a(!is_space_record(record_handle.type));

    const uint32_t space_id = page_handle.space_id;
    const uint32_t page_no = page_handle.page_no;

    /* TODO: This is a temporary hack to fit into
    parse_or_apply_page_record_body() interface. */
    alignas(alignof(buf_block_t)) byte block_data[sizeof(buf_block_t)]{};
    buf_block_t &block = *reinterpret_cast<buf_block_t *>(&block_data);
    const auto &page_buffer = page_handle.frame;
    block.frame = page_buffer.data();
    block.page.state = BUF_BLOCK_MEMORY;
    block.page.id = page_id_t(space_id, page_no);

    if (!page_handle.zipped.has_value()) {
      block.page.size =
          page_size_t(page_buffer.size(), page_buffer.size(), false);
    } else {
      /* Set the compressed page buffer  */
      const auto &zipped = page_handle.zipped.value();
      block.page.zip.data = zipped.frame.data();
      /* Set the compressed page metadata */
      const auto &metadata = zipped.metadata;
      page_zip_set_size(&block.page.zip, zipped.frame.size());
      block.page.zip.m_start = metadata.start_offset;
      block.page.zip.m_end = metadata.end_offset;
      block.page.zip.n_blobs = metadata.n_blobs;
      block.page.size =
          page_size_t(zipped.frame.size(), page_buffer.size(), true);
    }

#ifndef UNIV_HOTBACKUP
    new (&block.lock) rw_lock_t;
#ifdef UNIV_DEBUG
    block.lock.m_id = LATCH_ID_BUF_BLOCK_LOCK;
    block.lock.m_rw_lock = true;
    UT_LIST_INIT(block.lock.debug_list);
    rw_lock_add_debug_info(&block.lock, 0, RW_LOCK_X, UT_LOCATION_HERE);
#endif /* UNIV_DEBUG */
#endif /* !UNIV_HOTBACKUP */

    mtr_t mtr;
    mtr.start();
    mtr_set_log_mode(&mtr, MTR_LOG_NONE);
    mtr_memo_push(&mtr, &block, MTR_MEMO_PAGE_X_FIX);

    /** This assert is to distinguish if a given parser's subfunction returned
    nullptr because it had trouble with parsing, or because the buffer was
    nullptr to begin with */
    ut_a(record_handle.body.data() != nullptr);

    const auto result =
        parse_or_apply_page_record_body(record_handle.type, space_id, page_no,
                                        record_handle.body, &block, &mtr)
            .has_value();

#ifndef UNIV_HOTBACKUP
#ifdef UNIV_DEBUG
    rw_lock_remove_debug_info(&block.lock, 0, RW_LOCK_X);
#endif /* UNIV_DEBUG */
#endif /* !UNIV_HOTBACKUP */

    if (page_handle.zipped.has_value()) {
      /* Update the compressed page metadata in the page_handle */
      auto &metadata = page_handle.zipped.value().metadata;
      metadata.start_offset = block.page.zip.m_start;
      metadata.end_offset = block.page.zip.m_end;
      metadata.n_blobs = block.page.zip.n_blobs;
    }

    ut_d(mtr.get_memo()->erase());

    return result;
  }

 private:
  /** Parse redo record.
  @param buffer buffer to parse
  @return Parsed record or error. */
  [[nodiscard]] Parse_result<Record_view> parse_record(
      std::span<const uint8_t> buffer) {
    if (buffer.empty()) {
      return ut::Unexpected(Parse_error::Incomplete);
    }

    const int type = buffer[0] & ~MLOG_SINGLE_REC_FLAG;
    if (type == 0 || type > MLOG_BIGGEST_TYPE) {
      return ut::Unexpected(Parse_error::Corrupted);
    }

    switch (type) {
      case MLOG_MULTI_REC_END:
      case MLOG_DUMMY_RECORD:
        return Record_view(type, 1, {}, Record_view::Aux_tag{});

      case MLOG_TABLE_DYNAMIC_META: {
        mlog_id_t mlog_type;
        uint64_t table_id;
        uint64_t version;

        const auto *ptr = mlog_parse_initial_dict_log_record(
            buffer.data(), buffer.data() + buffer.size(), &mlog_type, &table_id,
            &version);

        if (ptr == nullptr) {
          return ut::Unexpected(Parse_error::Incomplete);
        }

        ut_a(mlog_type == type);

        const size_t hdr_size = ptr - buffer.data();

        auto result =
            parse_table_record_body(table_id, buffer.subspan(hdr_size));
        if (!result) {
          return ut::Unexpected(result.error());
        }

        const size_t body_size = *result;
        const size_t rec_size = hdr_size + body_size;

        return Record_view(type, rec_size, buffer.subspan(hdr_size, body_size),
                           Record_view::Table_tag{table_id, version});
      }

      default: {
        mlog_id_t mlog_type;
        uint32_t space_id;
        uint32_t page_no;

        const auto *ptr = mlog_parse_initial_log_record(
            buffer.data(), buffer.data() + buffer.size(), &mlog_type, &space_id,
            &page_no);

        if (ptr == nullptr) {
          return ut::Unexpected(Parse_error::Incomplete);
        }

        ut_a(mlog_type == type);

        const size_t hdr_size = ptr - buffer.data();

        auto result = parse_page_record_body(type, space_id, page_no,
                                             buffer.subspan(hdr_size));
        if (!result) {
          return ut::Unexpected(result.error());
        }

        const size_t body_size = *result;
        const size_t rec_size = hdr_size + body_size;

        if (is_space_record(type)) {
          return Record_view(type, rec_size,
                             buffer.subspan(hdr_size, body_size),
                             Record_view::Space_tag{space_id});
        }

        return Record_view(type, rec_size, buffer.subspan(hdr_size, body_size),
                           Record_view::Page_tag{space_id, page_no});
      }
    }
  }

  /** Parse page data record body.
  @param type     record type
  @param space_id tablespace id
  @param page_no  page number
  @param buffer   buffer to parse
  @return Record body size or error. */
  [[nodiscard]] Parse_result<size_t> parse_page_record_body(
      int type, uint32_t space_id, uint32_t page_no,
      std::span<const uint8_t> buffer) {
    return parse_or_apply_page_record_body(type, space_id, page_no, buffer,
                                           nullptr, nullptr);
  }

  /** Parse table metadata record body.
  @param table_id table id
  @param buffer   buffer to parse
  @return Record body size or error. */
  [[nodiscard]] Parse_result<size_t> parse_table_record_body(
      uint64_t table_id, std::span<const uint8_t> buffer) {
    /* At least we should get type byte and another one byte
    for data, if not, it's an incomplete record. */
    if (buffer.size() < 2) {
      return ut::Unexpected(Parse_error::Incomplete);
    }

    const auto ptype = static_cast<persistent_type_t>(buffer[0]);
    if (ptype <= PM_SMALLEST_TYPE || ptype >= PM_BIGGEST_TYPE) {
      return ut::Unexpected(Parse_error::Corrupted);
    }

    auto *persister = m_persisters->get(ptype);
    ut_a(persister);

    PersistentTableMetadata metadata(table_id, 0);
    bool corrupted = false;
    constexpr auto ptype_len = 1;
    const auto body = buffer.subspan(ptype_len);
    const size_t consumed =
        persister->read(metadata, body.data(), body.size(), &corrupted);

    if (corrupted) {
      return ut::Unexpected(Parse_error::Corrupted);
    }
    if (consumed == 0) {
      return ut::Unexpected(Parse_error::Incomplete);
    }

    return ptype_len + consumed;
  }

  /** Parse or apply page data record body.
  @param rec_type record type
  @param space_id tablespace id
  @param page_no  page number
  @param buffer   buffer to parse or apply
  @param block    block containing a page to apply (nullptr for parsing)
  @param mtr      mtr for apply operation (nullptr for parsing)
  @return Record body size or error.

  @note This function was moved from log0recv.cc mostly untouched and needs
  to be refactored:
    - Consider to get rid of the block parameter.
    - Consider to get rid of the mtr parameter.
    - Get rid of recv_sys global usage.
    - Get rid of other possible innodb globals.
    - Decide how to handle table space flags modification.
    - Create unit tests for all possible record types. */
  [[nodiscard]] Parse_result<size_t> parse_or_apply_page_record_body(
      int rec_type, uint32_t space_id, uint32_t page_no,
      std::span<const uint8_t> buffer, buf_block_t *block, mtr_t *mtr) {
    const auto type = static_cast<mlog_id_t>(rec_type);

    const auto *ptr = buffer.data();
    const auto *end_ptr = buffer.data() + buffer.size();

    bool applying_redo = (block != nullptr);

    /**  A few lines later fil_tablespace_redo_delete(..,true) is called where
    the last argument means parse_only. Passing hardcoded pasre_only=true
    is only justified if we assume that true return from
    is_space_record(type) implies applying_redo variable is false */
    ut_a(!(is_space_record(type) && applying_redo));

    const auto maybe_error =
        [buffer](const uint8_t *ptr) -> Parse_result<size_t> {
      if (ptr == nullptr) {
        /* TODO: Get rid of recv_sys->found_corrupt_log. */
        if (recv_sys && recv_sys->found_corrupt_log) {
          return ut::Unexpected(Parse_error::Corrupted);
        }
        return ut::Unexpected(Parse_error::Incomplete);
      }

      return ptr - buffer.data();
    };

    switch (type) {
      case MLOG_FILE_DELETE:
        return maybe_error(
            fil_tablespace_redo_delete(ptr, end_ptr, space_id, true));

      case MLOG_FILE_CREATE:
        return maybe_error(
            fil_tablespace_redo_create(ptr, end_ptr, space_id, true));

      case MLOG_FILE_RENAME:
        return maybe_error(
            fil_tablespace_redo_rename(ptr, end_ptr, space_id, true));

      case MLOG_FILE_EXTEND:
        return maybe_error(
            fil_tablespace_redo_extend(ptr, end_ptr, space_id, true));

      case MLOG_INDEX_LOAD:
        if (end_ptr < ptr + 8) {
          return ut::Unexpected(Parse_error::Incomplete);
        }
        return 8;
      default:
        break;
    }

    page_t *page;
    page_zip_des_t *page_zip;
    dict_index_t *index = nullptr;

#ifdef UNIV_DEBUG
    ulint page_type;
#endif /* UNIV_DEBUG */

#if defined(UNIV_HOTBACKUP) && defined(UNIV_DEBUG)
    ib::trace_3() << "recv_parse_or_apply_log_rec_body: type "
                  << get_mlog_string(type) << " space_id " << space_id
                  << " page_nr " << page_no << " ptr "
                  << static_cast<const void *>(ptr) << " end_ptr "
                  << static_cast<const void *>(end_ptr) << " block "
                  << static_cast<const void *>(block) << " mtr "
                  << static_cast<const void *>(mtr);
#endif /* UNIV_HOTBACKUP && UNIV_DEBUG */

    if (applying_redo) {
      /* Applying a page log record. */
      ut_ad(mtr != nullptr);

      page = block->frame;
      page_zip = buf_block_get_page_zip(block);

      ut_d(page_type = fil_page_get_type(page));
#if defined(UNIV_HOTBACKUP) && defined(UNIV_DEBUG)
      if (page_type == 0) {
        meb_print_page_header(page);
      }
#endif /* UNIV_HOTBACKUP && UNIV_DEBUG */

    } else {
      /* Parsing a page log record. */
      ut_ad(mtr == nullptr);
      page = nullptr;
      page_zip = nullptr;

      ut_d(page_type = FIL_PAGE_TYPE_ALLOCATED);
    }

    switch (type) {
      case MLOG_1BYTE:
      case MLOG_2BYTES:
      case MLOG_4BYTES:
      case MLOG_8BYTES:
#ifdef UNIV_DEBUG
        ut_ad(page == nullptr || end_ptr > ptr + 2);
        if (page && page_type == FIL_PAGE_TYPE_ALLOCATED) {
          /* It is OK to set FIL_PAGE_TYPE and certain list node fields on an
          empty page. Any other write is not OK. */

          const uint16_t offs = mach_read_from_2(ptr);

          switch (type) {
            default:
              ut_error;
            case MLOG_2BYTES:
              ut_ad(offs == FIL_PAGE_TYPE ||
                    offs ==
                        IBUF_HEADER + IBUF_TREE_SEG_HEADER + FSEG_HDR_OFFSET ||
                    offs == PAGE_HEADER + PAGE_BTR_SEG_LEAF + FSEG_HDR_OFFSET ||
                    offs == PAGE_HEADER + PAGE_BTR_SEG_TOP + FSEG_HDR_OFFSET ||
                    offs == PAGE_HEADER + PAGE_BTR_IBUF_FREE_LIST + FLST_FIRST +
                                FIL_ADDR_BYTE ||
                    offs == PAGE_HEADER + PAGE_BTR_IBUF_FREE_LIST + FLST_LAST +
                                FIL_ADDR_BYTE ||
                    offs == PAGE_HEADER + PAGE_BTR_IBUF_FREE_LIST_NODE +
                                FLST_PREV + FIL_ADDR_BYTE ||
                    offs == PAGE_HEADER + PAGE_BTR_IBUF_FREE_LIST_NODE +
                                FLST_NEXT + FIL_ADDR_BYTE);
              break;
            case MLOG_4BYTES:
              ut_ad(
                  offs ==
                      IBUF_HEADER + IBUF_TREE_SEG_HEADER + FSEG_HDR_PAGE_NO ||
                  offs == IBUF_HEADER + IBUF_TREE_SEG_HEADER + FSEG_HDR_SPACE ||
                  offs == PAGE_HEADER + PAGE_BTR_SEG_LEAF + FSEG_HDR_PAGE_NO ||
                  offs == PAGE_HEADER + PAGE_BTR_SEG_LEAF + FSEG_HDR_SPACE ||
                  offs == PAGE_HEADER + PAGE_BTR_SEG_TOP + FSEG_HDR_PAGE_NO ||
                  offs == PAGE_HEADER + PAGE_BTR_SEG_TOP + FSEG_HDR_SPACE ||
                  offs == PAGE_HEADER + PAGE_BTR_IBUF_FREE_LIST + FLST_LEN ||
                  offs == PAGE_HEADER + PAGE_BTR_IBUF_FREE_LIST + FLST_FIRST +
                              FIL_ADDR_PAGE ||
                  offs == PAGE_HEADER + PAGE_BTR_IBUF_FREE_LIST + FLST_LAST +
                              FIL_ADDR_PAGE ||
                  offs == PAGE_HEADER + PAGE_BTR_IBUF_FREE_LIST_NODE +
                              FLST_PREV + FIL_ADDR_PAGE ||
                  offs == PAGE_HEADER + PAGE_BTR_IBUF_FREE_LIST_NODE +
                              FLST_NEXT + FIL_ADDR_PAGE);
              break;
          }
        }
#endif /* UNIV_DEBUG */

        ptr = mlog_parse_nbytes(type, ptr, end_ptr, page, page_zip);
        break;

      case MLOG_REC_INSERT:

        ut_ad(!page || fil_page_type_is_index(page_type));

        if (nullptr != (ptr = parse_index(ptr, end_ptr, &index, page))) {
          ut_a(!page || page_is_comp(page) == dict_table_is_comp(index->table));

          ptr =
              page_cur_parse_insert_rec(false, ptr, end_ptr, block, index, mtr);
        }
        break;

      case MLOG_REC_CLUST_DELETE_MARK:

        ut_ad(!page || fil_page_type_is_index(page_type));

        if (nullptr != (ptr = parse_index(ptr, end_ptr, &index, page))) {
          ut_a(!page || page_is_comp(page) == dict_table_is_comp(index->table));

          ptr = btr_cur_parse_del_mark_set_clust_rec(ptr, end_ptr, page,
                                                     page_zip, index);
        }

        break;

      case MLOG_REC_SEC_DELETE_MARK:

        ut_ad(!page || fil_page_type_is_index(page_type));

        ptr = btr_cur_parse_del_mark_set_sec_rec(ptr, end_ptr, page, page_zip);
        break;

      case MLOG_REC_UPDATE_IN_PLACE:

        ut_ad(!page || fil_page_type_is_index(page_type));

        if (nullptr != (ptr = parse_index(ptr, end_ptr, &index, page))) {
          ut_a(!page || page_is_comp(page) == dict_table_is_comp(index->table));

          ptr = btr_cur_parse_update_in_place(ptr, end_ptr, page, page_zip,
                                              index);
        }

        break;

      case MLOG_LIST_END_DELETE:
      case MLOG_LIST_START_DELETE:

        ut_ad(!page || fil_page_type_is_index(page_type));

        if (nullptr != (ptr = parse_index(ptr, end_ptr, &index, page))) {
          ut_a(!page || page_is_comp(page) == dict_table_is_comp(index->table));

          ptr =
              page_parse_delete_rec_list(type, ptr, end_ptr, block, index, mtr);
        }

        break;

      case MLOG_LIST_END_COPY_CREATED:

        ut_ad(!page || fil_page_type_is_index(page_type));

        if (nullptr != (ptr = parse_index(ptr, end_ptr, &index, page))) {
          ut_a(!page || page_is_comp(page) == dict_table_is_comp(index->table));

          ptr = page_parse_copy_rec_list_to_created_page(ptr, end_ptr, block,
                                                         index, mtr);
        }

        break;

      case MLOG_PAGE_REORGANIZE:

        ut_ad(!page || fil_page_type_is_index(page_type));

        if (nullptr != (ptr = parse_index(ptr, end_ptr, &index, page))) {
          ut_a(!page || page_is_comp(page) == dict_table_is_comp(index->table));

          ptr =
              btr_parse_page_reorganize(ptr, end_ptr, index, false, block, mtr);
        }

        break;

      case MLOG_ZIP_PAGE_REORGANIZE:

        ut_ad(!page || fil_page_type_is_index(page_type));

        if (nullptr != (ptr = parse_index(ptr, end_ptr, &index, page))) {
          ut_a(!page || page_is_comp(page) == dict_table_is_comp(index->table));

          ptr =
              btr_parse_page_reorganize(ptr, end_ptr, index, true, block, mtr);
        }

        break;

      case MLOG_PAGE_CREATE:
      case MLOG_COMP_PAGE_CREATE:

        /* Allow anything in page_type when creating a page. */
        ut_a(!page_zip);

        page_parse_create(block, type == MLOG_COMP_PAGE_CREATE, FIL_PAGE_INDEX);

        break;

      case MLOG_PAGE_CREATE_RTREE:
      case MLOG_COMP_PAGE_CREATE_RTREE:

        page_parse_create(block, type == MLOG_COMP_PAGE_CREATE_RTREE,
                          FIL_PAGE_RTREE);

        break;

      case MLOG_PAGE_CREATE_SDI:
      case MLOG_COMP_PAGE_CREATE_SDI:

        page_parse_create(block, type == MLOG_COMP_PAGE_CREATE_SDI,
                          FIL_PAGE_SDI);

        break;

      case MLOG_UNDO_INSERT:

        ut_ad(!page || page_type == FIL_PAGE_UNDO_LOG);

        ptr = trx_undo_parse_add_undo_rec(ptr, end_ptr, page);

        break;

      case MLOG_UNDO_ERASE_END:

        ut_ad(!page || page_type == FIL_PAGE_UNDO_LOG);

        ptr = trx_undo_parse_erase_page_end(ptr, end_ptr, page, mtr);

        break;

      case MLOG_UNDO_INIT:

        /* Allow anything in page_type when creating a page. */

        ptr = trx_undo_parse_page_init(ptr, end_ptr, page, mtr);

        break;
      case MLOG_UNDO_HDR_CREATE:
      case MLOG_UNDO_HDR_REUSE:

        ut_ad(!page || page_type == FIL_PAGE_UNDO_LOG);

        ptr = trx_undo_parse_page_header(type, ptr, end_ptr, page, mtr);

        break;

      case MLOG_REC_MIN_MARK:
      case MLOG_COMP_REC_MIN_MARK:

        ut_ad(!page || fil_page_type_is_index(page_type));

        /* On a compressed page, MLOG_COMP_REC_MIN_MARK
        will be followed by MLOG_COMP_REC_DELETE
        or MLOG_ZIP_WRITE_HEADER(FIL_PAGE_PREV, FIL_nullptr)
        in the same mini-transaction. */

        ut_a(type == MLOG_COMP_REC_MIN_MARK || !page_zip);

        ptr = btr_parse_set_min_rec_mark(
            ptr, end_ptr, type == MLOG_COMP_REC_MIN_MARK, page, mtr);

        break;

      case MLOG_REC_DELETE:

        ut_ad(!page || fil_page_type_is_index(page_type));

        if (nullptr != (ptr = parse_index(ptr, end_ptr, &index, page))) {
          ut_a(!page || page_is_comp(page) == dict_table_is_comp(index->table));

          ptr = page_cur_parse_delete_rec(ptr, end_ptr, block, index, mtr);
        }

        break;

      case MLOG_IBUF_BITMAP_INIT:

        /* Allow anything in page_type when creating a page. */

        ptr = ibuf_parse_bitmap_init(ptr, end_ptr, block, mtr);

        break;

      case MLOG_INIT_FILE_PAGE:
      case MLOG_INIT_FILE_PAGE2: {
        /* Allow anything in page_type when creating a page. */
        ptr = fsp_parse_init_file_page(ptr, end_ptr, block);
        break;
      }

      case MLOG_WRITE_STRING: {
        ut_ad(!page || page_type != FIL_PAGE_TYPE_ALLOCATED || page_no == 0);

#ifndef UNIV_HOTBACKUP
        if (applying_redo) {
          const bool is_encryption =
              check_encryption(page_no, space_id, ptr, end_ptr);

          /* Reset in-mem encryption information for the tablespace here if this
          is "resetting encryption info" log. */
          if (is_encryption && ut::is_zeros(ptr + 4, Encryption::INFO_SIZE)) {
            const auto status = fil_reset_encryption(space_id);
            ut_a(DB_SUCCESS == status);
          }
        }
#endif /* !UNIV_HOTBACKUP */

        ptr = mlog_parse_string(ptr, end_ptr, page, page_zip);
        break;
      }

      case MLOG_ZIP_WRITE_NODE_PTR:

        ut_ad(!page || fil_page_type_is_index(page_type));

        ptr = page_zip_parse_write_node_ptr(ptr, end_ptr, page, page_zip);

        break;

      case MLOG_ZIP_WRITE_BLOB_PTR:

        ut_ad(!page || fil_page_type_is_index(page_type));

        ptr = page_zip_parse_write_blob_ptr(ptr, end_ptr, page, page_zip);

        break;

      case MLOG_ZIP_WRITE_HEADER:

        ut_ad(!page || fil_page_type_is_index(page_type));

        ptr = page_zip_parse_write_header(ptr, end_ptr, page, page_zip);

        break;

      case MLOG_ZIP_PAGE_COMPRESS:

        /* Allow anything in page_type when creating a page. */
        ptr = page_zip_parse_compress(ptr, end_ptr, page, page_zip);
        break;

      case MLOG_ZIP_PAGE_COMPRESS_NO_DATA:

        if (nullptr != (ptr = parse_index(ptr, end_ptr, &index, page))) {
          ut_a(!page ||
               (page_is_comp(page) == dict_table_is_comp(index->table)));

          ptr = page_zip_parse_compress_no_data(ptr, end_ptr, page, page_zip,
                                                index);
        }

        break;

      case MLOG_TEST:
#ifndef UNIV_HOTBACKUP
        if (log_test != nullptr) {
          ptr = log_test->parse_mlog_rec(ptr, end_ptr);
        } else {
          /* Just parse and ignore record to pass it and go forward. Note that
          this record is also used in the innodb.log_first_rec_group mtr test.
          The record is written in the buf0flu.cc when flushing page in that
          case. */
          Log_test::Key key;
          Log_test::Value value;
          lsn_t start_lsn, end_lsn;

          ptr = Log_test::parse_mlog_rec(ptr, end_ptr, key, value, start_lsn,
                                         end_lsn);
        }
        break;
#endif /* !UNIV_HOTBACKUP */
        /* Fall through. */

      default:
        /* Should not happen, any invalid record type was already handled by
        get_record_type(). */
        ut_error;
    }

    return maybe_error(ptr);
  }

  /** Check if redo record has encryption information.
  @param page_no  page number
  @param space_id tablespace id
  @param start    record body start
  @param end      record body end
  @return True if encryption information presents. */
  [[nodiscard]] bool check_encryption(uint32_t page_no, uint32_t space_id,
                                      const byte *start, const byte *end) {
    /* Only page zero contains encryption metadata. */
    if (page_no != 0 || fsp_is_system_or_temp_tablespace(space_id) ||
        end < start + 4) {
      return false;
    }

    bool found = false;

    const page_size_t &page_size = fil_space_get_page_size(space_id, &found);

    if (!found) {
      return false;
    }

    auto encryption_offset = fsp_header_get_encryption_offset(page_size);
    auto offset = mach_read_from_2(start);

    /* Encryption offset at page 0 is the only way we can identify encryption
    information as of today. Ideally we should have a separate redo type. */
    if (offset == encryption_offset) {
      auto len = mach_read_from_2(start + 2);
      ut_ad(len == Encryption::INFO_SIZE);

      if (len != Encryption::INFO_SIZE) {
        /* purecov: begin inspected */
        ib::warn(ER_IB_WRN_ENCRYPTION_INFO_SIZE_MISMATCH, size_t{len},
                 Encryption::INFO_SIZE);
        return false;
        /* purecov: end */
      }
      return true;
    }

    return false;
  }

  /** Check if the record is of table space kind.
  @param type record type
  @return true if the record is of table space kind, false otherwise. */
  [[nodiscard]] static bool is_space_record(int type) {
    switch (type) {
      case MLOG_FILE_CREATE:
      case MLOG_FILE_RENAME:
      case MLOG_FILE_DELETE:
      case MLOG_FILE_EXTEND:
      case MLOG_INDEX_LOAD:
        return true;
      default:
        return false;
    }
  }

  /** Provides a result of mlog_parse_index() which creates a dummy index based
  on redo log information in [ptr, end_ptr).
  The index->id is set to the one extracted from the page if it is provided.
  This id is required when applying a record to a page.
  The caller should not attempt to free the index, and can only dereference it
  until another call to parse_index(..).
  @param[in]  ptr     buffer containing index description (i.e. what typically
                      follows after the type of the redo log record and page id)
  @param[in]  end_ptr buffer end
  @param[out] index   this will be set by this function to point to an instance
                      of dict_index_t corresponding to the result of parsing the
                      prefix of [ptr, end_ptr). The ownership is not transferred
                      to the caller. It is owned by m_parsed_index_cache and so
                      can be evicted or overwritten by another call to
                      parse_index(..) or the destructor of this Redo_applier
  @param[in]  page    page to apply the record or null
  @return parsed record end or null if record is incomplete. */
  [[nodiscard]] const byte *parse_index(const byte *ptr, const byte *end_ptr,
                                        dict_index_t **index,
                                        const page_t *page) {
    Parsed_index_lru_cache::sequence_view buffer(
        ptr, static_cast<size_t>(end_ptr - ptr));
    if (dict_index_t *cached = m_parsed_index_cache.lookup(buffer);
        cached != nullptr) {
      *index = cached;
      if (page) {
        (*index)->id = mach_read_from_8(page + PAGE_HEADER + PAGE_INDEX_ID);
      }
      return end_ptr - buffer.size();
    }

    dict_index_t *parsed_index = nullptr;
    const byte *parsed_end = mlog_parse_index(ptr, end_ptr, &parsed_index);
    if (parsed_end == nullptr) {
      /* Note: for some incomplete buffers mlog_parse_index() might have
      allocated dummy objects and returned nullptr. Keep the old behavior and
      free them here (the cache only owns successful parses). */
      if (parsed_index != nullptr) {
        dict_table_t *table = parsed_index->table;
        dict_mem_index_free(parsed_index);
        dict_mem_table_free(table);
      }
      return nullptr;
    }

    if (page) {
      parsed_index->id = mach_read_from_8(page + PAGE_HEADER + PAGE_INDEX_ID);
    }
    const bool inserted = m_parsed_index_cache.insert_mru(
        Parsed_index_lru_cache::sequence_view(
            ptr, static_cast<size_t>(parsed_end - ptr)),
        parsed_index);
    ut_a(inserted);
    *index = parsed_index;
    return parsed_end;
  }

 private:
  std::unique_ptr<Persisters> m_persisters;
  /** Maps a buffer prefix to parsed dict_index_t. */
  Parsed_index_lru_cache m_parsed_index_cache;
};

Redo_applier::Redo_applier() : m_impl(std::make_unique<Impl>()) {}

Redo_applier::~Redo_applier() = default;

Parse_result<Mtr_view> Redo_applier::parse_mtr(
    std::span<const uint8_t> buffer) {
  return m_impl->parse_mtr(buffer);
}

bool Redo_applier::apply(const Record_handle &record_handle,
                         Page_handle &page_handle) {
  return m_impl->apply(record_handle, page_handle);
}

Page_handle_wrapper::Page_handle_wrapper(buf_block_t &block)
    : m_block(block),
      m_page_handle{.space_id = block.get_page_id().space(),
                    .page_no = block.get_page_no(),
                    .frame = {block.frame, block.page.size.logical()},
                    .zipped = {}} {
  const auto &comp_page_desc = m_block.get_page_zip();
  if (comp_page_desc) {
    m_page_handle.zipped.emplace(
        std::span<uint8_t>(comp_page_desc->data, m_block.page.size.physical()),
        Page_handle::Zipped::Metadata{comp_page_desc->m_start,
                                      comp_page_desc->m_end,
                                      comp_page_desc->n_blobs});
  }
}

void Page_handle_wrapper::update_block() {
  if (m_page_handle.zipped.has_value()) {
    const auto &metadata = m_page_handle.zipped.value().metadata;
    m_block.page.zip.m_start = metadata.start_offset;
    m_block.page.zip.m_end = metadata.end_offset;
    m_block.page.zip.n_blobs = metadata.n_blobs;
  }
#ifdef UNIV_DEBUG
  m_need_to_update_block = false;
#endif /* UNIV_DEBUG */
}

}  // namespace ib::redo
