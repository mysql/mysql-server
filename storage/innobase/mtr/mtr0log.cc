/*****************************************************************************

Copyright (c) 1995, 2024, Oracle and/or its affiliates.

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

/** @file mtr/mtr0log.cc
 Mini-transaction log routines

 Created 12/7/1995 Heikki Tuuri
 *******************************************************/

#include "mtr0log.h"

#ifndef UNIV_HOTBACKUP
#include "buf0buf.h"
#include "buf0dblwr.h"
#include "dict0dict.h"
#include "log0recv.h"
#endif /* !UNIV_HOTBACKUP */
#include "page0page.h"

#ifndef UNIV_HOTBACKUP
#include "dict0boot.h"
#endif /* !UNIV_HOTBACKUP */

/** Parses a log record written by mlog_open_and_write_index.
@param[in]  ptr      buffer
@param[in]  end_ptr  buffer end
@param[out] index    own: dummy index
@return parsed record end, NULL if not a complete record */
[[nodiscard]] static const byte *mlog_parse_index_v1(const byte *ptr,
                                                     const byte *end_ptr,
                                                     dict_index_t **index);

/** Catenates n bytes to the mtr log.
@param[in] mtr Mini-transaction
@param[in] str String to write
@param[in] len String length */
void mlog_catenate_string(mtr_t *mtr, const byte *str, ulint len) {
  if (mtr_get_log_mode(mtr) == MTR_LOG_NONE) {
    return;
  }

  mtr->get_log()->push(str, uint32_t(len));
}

#ifndef UNIV_HOTBACKUP
/** Writes the initial part of a log record consisting of one-byte item
 type and four-byte space and page numbers. Also pushes info
 to the mtr memo that a buffer page has been modified. */
void mlog_write_initial_log_record(
    const byte *ptr, /*!< in: pointer to (inside) a buffer
                     frame holding the file page where
                     modification is made */
    mlog_id_t type,  /*!< in: log item type: MLOG_1BYTE, ... */
    mtr_t *mtr)      /*!< in: mini-transaction handle */
{
  byte *log_ptr = nullptr;

  ut_ad(type <= MLOG_BIGGEST_TYPE);
  ut_ad(type > MLOG_8BYTES);

  /* If no logging is requested, we may return now */
  if (!mlog_open(mtr, REDO_LOG_INITIAL_INFO_SIZE, log_ptr)) {
    return;
  }

  log_ptr = mlog_write_initial_log_record_fast(ptr, type, log_ptr, mtr);

  mlog_close(mtr, log_ptr);
}
#endif /* !UNIV_HOTBACKUP */

/** Parses an initial log record written by mlog_write_initial_dict_log_record.
@param[in]      ptr             buffer
@param[in]      end_ptr         buffer end
@param[out]     type            log record type, should be
                                MLOG_TABLE_DYNAMIC_META
@param[out]     id              table id
@param[out]     version         table dynamic metadata version
@return parsed record end, NULL if not a complete record */
const byte *mlog_parse_initial_dict_log_record(const byte *ptr,
                                               const byte *end_ptr,
                                               mlog_id_t *type, table_id_t *id,
                                               uint64_t *version) {
  if (end_ptr < ptr + 1) {
    return (nullptr);
  }

  *type = (mlog_id_t)((ulint)*ptr & ~MLOG_SINGLE_REC_FLAG);
  ut_ad(*type == MLOG_TABLE_DYNAMIC_META);

  ptr++;

  if (end_ptr < ptr + 1) {
    return (nullptr);
  }

  *id = mach_parse_u64_much_compressed(&ptr, end_ptr);

  if (ptr == nullptr || end_ptr < ptr + 1) {
    return (nullptr);
  }

  *version = mach_parse_u64_much_compressed(&ptr, end_ptr);

  return ptr;
}

/** Parses an initial log record written by mlog_write_initial_log_record.
 @return parsed record end, NULL if not a complete record */
const byte *mlog_parse_initial_log_record(
    const byte *ptr,     /*!< in: buffer */
    const byte *end_ptr, /*!< in: buffer end */
    mlog_id_t *type,     /*!< out: log record type: MLOG_1BYTE, ... */
    space_id_t *space,   /*!< out: space id */
    page_no_t *page_no)  /*!< out: page number */
{
  if (end_ptr < ptr + 1) {
    return (nullptr);
  }

  *type = (mlog_id_t)((ulint)*ptr & ~MLOG_SINGLE_REC_FLAG);
  ut_ad(*type <= MLOG_BIGGEST_TYPE);

  ptr++;

  if (end_ptr < ptr + 2) {
    return (nullptr);
  }

  *space = mach_parse_compressed(&ptr, end_ptr);

  if (ptr != nullptr) {
    *page_no = mach_parse_compressed(&ptr, end_ptr);
  }

  return ptr;
}

/** Parses a log record written by mlog_write_ulint or mlog_write_ull.
 @return parsed record end, NULL if not a complete record or a corrupt record */
const byte *mlog_parse_nbytes(
    mlog_id_t type,      /*!< in: log record type: MLOG_1BYTE, ... */
    const byte *ptr,     /*!< in: buffer */
    const byte *end_ptr, /*!< in: buffer end */
    byte *page,          /*!< in: page where to apply the log
                         record, or NULL */
    void *page_zip)      /*!< in/out: compressed page, or NULL */
{
  ulint offset;
  ulint val;
  uint64_t dval;

  ut_a(type <= MLOG_8BYTES);
  ut_a(!page || !page_zip || !fil_page_index_page_check(page));

  if (end_ptr < ptr + 2) {
    return (nullptr);
  }

  offset = mach_read_from_2(ptr);
  ptr += 2;

  if (offset >= UNIV_PAGE_SIZE) {
    recv_sys->found_corrupt_log = true;

    return (nullptr);
  }

  if (type == MLOG_8BYTES) {
    dval = mach_u64_parse_compressed(&ptr, end_ptr);

    if (ptr == nullptr) {
      return (nullptr);
    }

    if (page) {
      if (page_zip) {
        mach_write_to_8(((page_zip_des_t *)page_zip)->data + offset, dval);
      }
      mach_write_to_8(page + offset, dval);
    }

    return ptr;
  }

  val = mach_parse_compressed(&ptr, end_ptr);

  if (ptr == nullptr) {
    return (nullptr);
  }

  switch (type) {
    case MLOG_1BYTE:
      if (val > 0xFFUL) {
        goto corrupt;
      }
      if (page) {
        if (page_zip) {
          mach_write_to_1(((page_zip_des_t *)page_zip)->data + offset, val);
        }
        mach_write_to_1(page + offset, val);
      }
      break;
    case MLOG_2BYTES:
      if (val > 0xFFFFUL) {
        goto corrupt;
      }
      if (page) {
        if (page_zip) {
          mach_write_to_2(((page_zip_des_t *)page_zip)->data + offset, val);
        }
        mach_write_to_2(page + offset, val);
      }
      break;
    case MLOG_4BYTES:
      if (page) {
        if (page_zip) {
          mach_write_to_4(((page_zip_des_t *)page_zip)->data + offset, val);
        }
        mach_write_to_4(page + offset, val);
      }
      break;
    default:
    corrupt:
      recv_sys->found_corrupt_log = true;
      ptr = nullptr;
  }

  return ptr;
}

/** Writes 1, 2 or 4 bytes to a file page. Writes the corresponding log
 record to the mini-transaction log if mtr is not NULL. */
void mlog_write_ulint(
    byte *ptr,      /*!< in: pointer where to write */
    ulint val,      /*!< in: value to write */
    mlog_id_t type, /*!< in: MLOG_1BYTE, MLOG_2BYTES, MLOG_4BYTES */
    mtr_t *mtr)     /*!< in: mini-transaction handle */
{
  switch (type) {
    case MLOG_1BYTE:
      mach_write_to_1(ptr, val);
      break;
    case MLOG_2BYTES:
      mach_write_to_2(ptr, val);
      break;
    case MLOG_4BYTES:
      mach_write_to_4(ptr, val);
      break;
    default:
      ut_error;
  }

  if (mtr == nullptr) {
    return;
  }

  /* If no logging is requested, we may return now */
  byte *log_ptr = nullptr;
  if (!mlog_open(mtr, REDO_LOG_INITIAL_INFO_SIZE + 2 + 5, log_ptr)) {
    return;
  }

  log_ptr = mlog_write_initial_log_record_fast(ptr, type, log_ptr, mtr);

  mach_write_to_2(log_ptr, page_offset(ptr));
  log_ptr += 2;

  log_ptr += mach_write_compressed(log_ptr, val);

  mlog_close(mtr, log_ptr);
}

/** Writes 8 bytes to a file page. Writes the corresponding log
 record to the mini-transaction log, only if mtr is not NULL */
void mlog_write_ull(byte *ptr,    /*!< in: pointer where to write */
                    uint64_t val, /*!< in: value to write */
                    mtr_t *mtr)   /*!< in: mini-transaction handle */
{
  mach_write_to_8(ptr, val);

  if (mtr == nullptr) {
    return;
  }

  /* If no logging is requested, we may return now */
  byte *log_ptr = nullptr;
  if (!mlog_open(mtr, REDO_LOG_INITIAL_INFO_SIZE + 2 + 9, log_ptr)) {
    return;
  }

  log_ptr = mlog_write_initial_log_record_fast(ptr, MLOG_8BYTES, log_ptr, mtr);

  mach_write_to_2(log_ptr, page_offset(ptr));
  log_ptr += 2;

  log_ptr += mach_u64_write_compressed(log_ptr, val);

  mlog_close(mtr, log_ptr);
}

#ifndef UNIV_HOTBACKUP
/** Writes a string to a file page buffered in the buffer pool. Writes the
 corresponding log record to the mini-transaction log. */
void mlog_write_string(byte *ptr,       /*!< in: pointer where to write */
                       const byte *str, /*!< in: string to write */
                       ulint len,       /*!< in: string length */
                       mtr_t *mtr)      /*!< in: mini-transaction handle */
{
  ut_ad(ptr);
  ut_ad(mtr != nullptr || buf_page_t::is_memory(ptr));
  ut_a(len < UNIV_PAGE_SIZE);

  memcpy(ptr, str, len);

  if (mtr != nullptr) {
    mlog_log_string(ptr, len, mtr);
  }
}

/** Logs a write of a string to a file page buffered in the buffer pool.
 Writes the corresponding log record to the mini-transaction log. */
void mlog_log_string(byte *ptr,  /*!< in: pointer written to */
                     ulint len,  /*!< in: string length */
                     mtr_t *mtr) /*!< in: mini-transaction handle */
{
  byte *log_ptr = nullptr;

  ut_ad(ptr && mtr);
  ut_ad(len <= UNIV_PAGE_SIZE);

  /* If no logging is requested, we may return now */
  if (!mlog_open(mtr, 30, log_ptr)) {
    return;
  }

  log_ptr =
      mlog_write_initial_log_record_fast(ptr, MLOG_WRITE_STRING, log_ptr, mtr);
  mach_write_to_2(log_ptr, page_offset(ptr));
  log_ptr += 2;

  mach_write_to_2(log_ptr, len);
  log_ptr += 2;

  mlog_close(mtr, log_ptr);

  mlog_catenate_string(mtr, ptr, len);
}
#endif /* !UNIV_HOTBACKUP */

/** Parses a log record written by mlog_write_string.
 @return parsed record end, NULL if not a complete record */
const byte *mlog_parse_string(
    const byte *ptr,     /*!< in: buffer */
    const byte *end_ptr, /*!< in: buffer end */
    byte *page,          /*!< in: page where to apply the log record, or NULL */
    void *page_zip)      /*!< in/out: compressed page, or NULL */
{
  ulint offset;
  ulint len;

  ut_a(!page || !page_zip ||
       (fil_page_get_type(page) != FIL_PAGE_INDEX &&
        fil_page_get_type(page) != FIL_PAGE_RTREE));

  if (end_ptr < ptr + 4) {
    return (nullptr);
  }

  offset = mach_read_from_2(ptr);
  ptr += 2;
  len = mach_read_from_2(ptr);
  ptr += 2;

  if (offset >= UNIV_PAGE_SIZE || len + offset > UNIV_PAGE_SIZE) {
    recv_sys->found_corrupt_log = true;

    return (nullptr);
  }

  if (end_ptr < ptr + len) {
    return (nullptr);
  }

  if (page) {
    if (page_zip) {
      memcpy(((page_zip_des_t *)page_zip)->data + offset, ptr, len);
    }
    memcpy(page + offset, ptr, len);
  }

  return (ptr + len);
}

const byte *mlog_parse_index_8027(const byte *ptr, const byte *end_ptr,
                                  bool comp, dict_index_t **index) {
  ulint i;
  dict_table_t *table;
  dict_index_t *ind;
  bool instant = false;
  uint16_t n, n_uniq;
  uint16_t n_inst_cols = 0;

  if (comp) {
    if (end_ptr < ptr + 4) {
      return (nullptr);
    }
    n = mach_read_from_2(ptr);
    ptr += 2;
    if ((n & 0x8000) != 0) {
      /* This is instant fields,
      see also mlog_open_and_write_index() */
      instant = true;
      n_inst_cols = n & ~0x8000;
      n = mach_read_from_2(ptr);
      ptr += 2;
      ut_ad((n & 0x8000) == 0);
      ut_ad(n_inst_cols <= n);

      if (end_ptr < ptr + 2) {
        return (nullptr);
      }
    }
    n_uniq = mach_read_from_2(ptr);
    ptr += 2;
    ut_ad(n_uniq <= n);
    if (end_ptr < ptr + n * 2) {
      return (nullptr);
    }
  } else {
    n = n_uniq = 1;
  }
  table = dict_mem_table_create("LOG_DUMMY", DICT_HDR_SPACE, n, 0, 0,
                                comp ? DICT_TF_COMPACT : 0, 0);
  if (instant) {
    table->set_instant_cols(n_inst_cols);
  }

  ind = dict_mem_index_create("LOG_DUMMY", "LOG_DUMMY", DICT_HDR_SPACE, 0, n);
  ind->table = table;
  ind->n_uniq = (unsigned int)n_uniq;
  if (n_uniq != n) {
    ut_a(n_uniq + DATA_ROLL_PTR <= n);
    ind->type = DICT_CLUSTERED;
  }
  if (comp) {
    for (i = 0; i < n; i++) {
      ulint len = mach_read_from_2(ptr);
      ptr += 2;
      /* The high-order bit of len is the NOT NULL flag;
      the rest is 0 or 0x7fff for variable-length fields,
      and 1..0x7ffe for fixed-length fields. */
      dict_mem_table_add_col(
          table, nullptr, nullptr,
          ((len + 1) & 0x7fff) <= 1 ? DATA_BINARY : DATA_FIXBINARY,
          len & 0x8000 ? DATA_NOT_NULL : 0, len & 0x7fff, true);

      /* The is_ascending flag does not matter during
      redo log apply, because we do not compare for
      "less than" or "greater than". */
      dict_index_add_col(ind, table, table->get_col(i), 0, true);
    }
    dict_table_add_system_columns(table, table->heap);
    if (n_uniq != n) {
      /* Identify DB_TRX_ID and DB_ROLL_PTR in the index. */
      ut_a(DATA_TRX_ID_LEN == ind->get_col(DATA_TRX_ID - 1 + n_uniq)->len);
      ut_a(DATA_ROLL_PTR_LEN == ind->get_col(DATA_ROLL_PTR - 1 + n_uniq)->len);
      ind->fields[DATA_TRX_ID - 1 + n_uniq].col = &table->cols[n + DATA_TRX_ID];
      ind->fields[DATA_ROLL_PTR - 1 + n_uniq].col =
          &table->cols[n + DATA_ROLL_PTR];
    }

    if (ind->is_clustered() && ind->table->has_instant_cols()) {
      ind->instant_cols = true;
      ind->n_instant_nullable =
          ind->get_n_nullable_before(ind->get_instant_fields());
    } else {
      ind->instant_cols = false;
      ind->n_instant_nullable = ind->n_nullable;
    }
  }
  /* avoid ut_ad(index->cached) in dict_index_get_n_unique_in_tree */
  ind->cached = true;
  *index = ind;
  return (ptr);
}

#ifndef UNIV_HOTBACKUP
/* logical_pos 2 bytes, phy_pos 2 bytes, v_added 1 byte, v_dropped 1 byte */
constexpr size_t inst_col_info_size = 6;

/** Calculate total size needed to log index information.
@param[in]   index         index
@param[in]   size          size passed from caller
@param[in]   n             number of fields in index
@param[in]   is_comp       true if COMP
@param[in]   is_versioned  if table has row versions
@param[in]   is_instant    true if table has INSTANT cols
@param[out]  size_needed   total size needed on REDO LOG */
static void log_index_get_size_needed(const dict_index_t *index, size_t size,
                                      uint16_t n, bool is_comp,
                                      bool is_versioned, bool is_instant,
                                      size_t &size_needed) {
  auto size_for_versioned_fields = [](const dict_index_t *ind) {
    size_t _size = 0;
    /* 2 bytes for number of columns with version */
    _size += 2;

    size_t n_versioned_fields = ind->table->get_n_instant_add_cols() +
                                ind->table->get_n_instant_drop_cols();
    ut_ad(n_versioned_fields != 0);

    _size += n_versioned_fields * inst_col_info_size;
    return (_size);
  };

  ut_ad(size_needed == 0);

  size_needed += REDO_LOG_INITIAL_INFO_SIZE + size;

  /* 1 byte to log INDEX_LOG_VERSION */
  size_needed += 1;

  /* 1 byte to log flag */
  size_needed += 1;

  if (!is_versioned && !is_comp) {
    return;
  }

  /* 2 bytes to log n */
  size_needed += 2;

  if (!is_comp) {
    ut_ad(is_versioned);
    size_needed += size_for_versioned_fields(index);
    return;
  }

  if (is_instant) {
    /* 2 bytes to log n_instant_cols */
    size_needed += 2;
  }

  /* 2 bytes to log n_uniq */
  size_needed += 2;

  /* 2 bytes needed for each (n) field to store it's len */
  size_needed += n * 2;

  if (is_versioned) {
    size_needed += size_for_versioned_fields(index);
  }
}

/** Log the index log version.
@param[in]     version  1 byte index log version
@param[in,out] log_ptr  REDO LOG buffer pointer */
static void log_index_log_version(uint8_t version, byte *&log_ptr) {
  mach_write_to_1(log_ptr, version);
  log_ptr += 1;
}

/** Log the flag.
@param[in]     flag     1 byte flag to be logged
@param[in,out] log_ptr  REDO LOG buffer pointer */
static void log_index_flag(uint8_t flag, byte *&log_ptr) {
  mach_write_to_1(log_ptr, flag);
  log_ptr += 1;
}

/** Log the number of fields in index.
@param[in]      index         index
@param[in]      n             number of fields
@param[in]      rec           index record
@param[in]      is_comp       true if COMP
@param[in]      is_versioned  true if table has row versions
@param[in]      is_instant    true if table has INSTANT cols
@param[in,out]  log_ptr       REDO LOG buffer pointer */
static void log_index_column_counts(const dict_index_t *index, uint16_t n,
                                    const byte *rec, bool is_comp,
                                    bool is_versioned, bool is_instant,
                                    byte *&log_ptr) {
  /* Only clustered index can have versions */
  ut_ad(!is_versioned || index->is_clustered());

  if (!is_versioned && !is_comp) {
    return;
  }

  /* log n */
  mach_write_to_2(log_ptr, n);
  log_ptr += 2;

  if (!is_comp) {
    ut_ad(is_versioned);
    return;
  }

  if (is_instant) {
    mach_write_to_2(log_ptr, index->get_instant_fields());
    log_ptr += 2;
  }

  /* log n_uniq */
  uint16_t n_uniq;
  if (page_is_leaf(page_align(rec))) {
    n_uniq = dict_index_get_n_unique_in_tree(index);
  } else {
    n_uniq = dict_index_get_n_unique_in_tree_nonleaf(index);
  }
  ut_ad(n_uniq <= n);
  mach_write_to_2(log_ptr, n_uniq);
  log_ptr += 2;
}

/** Close, allocate and reopen LOG pointer buffer.
@param[in]      log_ptr   pointer to log buffer
@param[in,out]  log_start start of currently allocated buffer
@param[in,out]  log_end   end of currently allocated buffer
@param[in]      mtr       mini transaction
@param[in,out]  alloc     size allocated as of now on log
@param[in,out]  total     total size needed on log */
static bool close_and_reopen_log(byte *&log_ptr, const byte *&log_start,
                                 const byte *&log_end, mtr_t *&mtr,
                                 size_t &alloc, size_t &total) {
  mlog_close(mtr, log_ptr);
  ut_a(total > (ulint)(log_ptr - log_start));
  total -= log_ptr - log_start;
  alloc = total;

  if (alloc > mtr_buf_t::MAX_DATA_SIZE) {
    alloc = mtr_buf_t::MAX_DATA_SIZE;
  }

  if (!mlog_open(mtr, alloc, log_ptr)) {
    /* logging is disabled */
    return false;
  }
  log_start = log_ptr;
  log_end = log_ptr + alloc;

  return true;
}

template <typename F>
/** Log index field len info.
@param[in]  index         index
@param[in]  n             number of fields
@param[in]  is_versioned  true if table has row versions
@param[in,out]  f         vector of fields with versions
@param[in]  changed_order array indicating fields changed position
@param[in]  log_ptr       log buffer pointer
@param[in]  func          callback to check size reopen log buffer */
static bool log_index_fields(const dict_index_t *index, uint16_t n,
                             bool is_versioned, std::vector<dict_field_t *> &f,
                             bool *changed_order, byte *&log_ptr, F &func) {
  /* Write metadata for each field. Log the fields in their logical order. */
  for (size_t i = 0; i < n; i++) {
    dict_field_t *field = index->get_field(i);
    const dict_col_t *col = field->col;
    ulint len = field->fixed_len;
    ut_ad(len < 0x7fff);

    if (len == 0 && (DATA_BIG_COL(col))) {
      /* variable-length field with maximum length > 255 */
      len = 0x7fff;
    }

    if (col->prtype & DATA_NOT_NULL) {
      len |= 0x8000;
    }

    if (!func(2)) {
      return false;
    }

    mach_write_to_2(log_ptr, len);
    log_ptr += 2;

    if (is_versioned) {
      if (col->is_instant_added() || col->is_instant_dropped() ||
          changed_order[i]) {
        f.push_back(field);
      }
    }
  }

  return true;
}

template <typename F>
/** Log fields with version.
@param[in]  f             vector of fields with versions
@param[in]  log_ptr       log buffer pointer
@param[in]  func          callback to check size reopen log buffer
@param[in]  index         index to fetch field's logical position */
static bool log_index_versioned_fields(const std::vector<dict_field_t *> &f,
                                       byte *&log_ptr, F &func,
                                       const dict_index_t *index) {
  uint16_t n_inst = f.size();
  ut_ad(n_inst > 0);

  if (!func(2)) {
    return false;
  }
  mach_write_to_2(log_ptr, n_inst);
  log_ptr += 2;

  for (auto field : f) {
    uint16_t logical_pos = index->get_logical_pos(field->get_phy_pos());
    ut_a(logical_pos != UINT16_UNDEFINED);

    /* Maximum columns could be 1017. Which needs maximum 10 bits. So we can
    use MSB to indicate if version info follows.
           - - - - - -[----------]
           ^ ^           ^
           | |           |
           | |           | 10 bits phy pos
           | |
           | | 15th bit indicates drop version info follows.
           | 16th bit indicates add version info follows. */
    uint16_t phy_pos = field->get_phy_pos();

    /* It also might be accompanying column order change (!added&&!dropped) */

    if (field->col->is_instant_added()) {
      /* Set 16th bit in phy_pos to indicate presence of version added */
      phy_pos |= 0x8000;
    }

    if (field->col->is_instant_dropped()) {
      /* Set 15th bit in phy_pos to indicate presence of version added */
      phy_pos |= 0x4000;
    }

    if (!func(6)) {
      return false;
    }

    mach_write_to_2(log_ptr, logical_pos);
    log_ptr += 2;

    mach_write_to_2(log_ptr, phy_pos);
    log_ptr += 2;

    if (field->col->is_instant_added()) {
      uint8_t v = field->col->get_version_added();
      mach_write_to_1(log_ptr, v);
      log_ptr += 1;
    }

    if (field->col->is_instant_dropped()) {
      uint8_t v = field->col->get_version_dropped();
      mach_write_to_1(log_ptr, v);
      log_ptr += 1;
    }
  }
  return true;
}
#endif

bool mlog_open_and_write_index(mtr_t *mtr, const byte *rec,
                               const dict_index_t *index, mlog_id_t type,
                               size_t size, byte *&log_ptr) {
#ifndef UNIV_HOTBACKUP
  ut_ad(page_rec_is_comp(rec) == dict_table_is_comp(index->table));
  const bool is_instant = index->has_instant_cols();
  const bool is_versioned = index->has_row_versions();
  const bool is_comp = dict_table_is_comp(index->table);

  const byte *log_start;
  const byte *log_end;

  uint16_t n = is_versioned ? index->get_n_total_fields()
                            : dict_index_get_n_fields(index);
  /* For spatial index, on non-leaf page, we just keep
  2 fields, MBR and page no. */
  if (dict_index_is_spatial(index) && !page_is_leaf(page_align(rec))) {
    n = DICT_INDEX_SPATIAL_NODEPTR_SIZE;
  }

  size_t size_needed = 0;
  log_index_get_size_needed(index, size, n, is_comp, is_versioned, is_instant,
                            size_needed);
  size_t total = size_needed;
  size_t alloc = total;
  if (alloc > mtr_buf_t::MAX_DATA_SIZE) {
    alloc = mtr_buf_t::MAX_DATA_SIZE;
  }

  if (!mlog_open(mtr, alloc, log_ptr)) {
    /* logging is disabled */
    return (false);
  }

  log_start = log_ptr;
  log_end = log_ptr + alloc;

  log_ptr = mlog_write_initial_log_record_fast(rec, type, log_ptr, mtr);

  uint8_t index_log_version = INDEX_LOG_VERSION_CURRENT;
  DBUG_EXECUTE_IF("invalid_index_log_version",
                  index_log_version = INDEX_LOG_VERSION_MAX + 1;);
  log_index_log_version(index_log_version, log_ptr);

  uint8_t flag = 0;
  if (is_instant) SET_INSTANT(flag);
  if (is_versioned) SET_VERSIONED(flag);
  if (is_comp) SET_COMPACT(flag);
  log_index_flag(flag, log_ptr);

  log_index_column_counts(index, n, rec, is_comp, is_versioned, is_instant,
                          log_ptr);

  /* List of INSTANT fields to be logged */
  std::vector<dict_field_t *> instant_fields_to_log;

  /* To check _size is available on buffer. If not, close and reopen buffer */
  auto f = [&](const size_t _size) {
    if (log_ptr + _size > log_end) {
      if (!close_and_reopen_log(log_ptr, log_start, log_end, mtr, alloc,
                                total)) {
        return false;
      }
    }
    return true;
  };

  /* Ordinal position of an existing field can't be changed with INSTANT
  algorithm. But when it is combined with ADD/DROP COLUMN, ordinal position
  of a filed can be changed. This bool array of size #fields in index,
  represents if ordinal position of an existing filed is changed. */
  bool *fields_with_changed_order = nullptr;
  if (is_versioned) {
    fields_with_changed_order = new bool[n];
    memset(fields_with_changed_order, false, (sizeof(bool) * n));

    uint16_t phy_pos = 0;
    for (size_t i = 0; i < n; i++) {
      dict_field_t *field = index->get_field(i);
      const dict_col_t *col = field->col;

      if (col->is_instant_added() || col->is_instant_dropped()) {
        continue;
      } else if (col->get_phy_pos() >= phy_pos) {
        phy_pos = col->get_phy_pos();
      } else {
        fields_with_changed_order[i] = true;
      }
    }
  }

  if (is_comp) {
    /* Write fields info. */
    if (!log_index_fields(index, n, is_versioned, instant_fields_to_log,
                          fields_with_changed_order, log_ptr, f)) {
      if (is_versioned) {
        delete[] fields_with_changed_order;
      }
      return false;
    }
  } else if (is_versioned) {
    for (size_t i = 0; i < n; i++) {
      dict_field_t *field = index->get_field(i);
      const dict_col_t *col = field->col;
      if (col->is_instant_added() || col->is_instant_dropped() ||
          fields_with_changed_order[i]) {
        instant_fields_to_log.push_back(field);
      }
    }
  }

  if (is_versioned) {
    delete[] fields_with_changed_order;
  }

  if (!instant_fields_to_log.empty()) {
    ut_ad(is_versioned);
    /* Log INSTANT ADD/DROP fields */
    if (!log_index_versioned_fields(instant_fields_to_log, log_ptr, f, index)) {
      return false;
    }
  }

  if (size == 0) {
    mlog_close(mtr, log_ptr);
    log_ptr = nullptr;
  } else if (log_ptr + size > log_end) {
    mlog_close(mtr, log_ptr);
    bool success = mlog_open(mtr, size, log_ptr);
    ut_a(success);
  }

  return (log_ptr != nullptr);
#else  /* !UNIV_HOTBACKUP */
  return (false);
#endif /* !UNIV_HOTBACKUP */
}

/** Read 2 bytes from log buffer.
@param[in]   ptr      pointer to buffer
@param[in]   end_ptr  pointer to end of buffer
@param[out]  val      read 2 bytes value */
static const byte *read_2_bytes(const byte *ptr, const byte *end_ptr,
                                uint16_t &val) {
  if (end_ptr < ptr + 2) {
    return (nullptr);
  }
  val = mach_read_from_2(ptr);
  ptr += 2;
  return ptr;
}

/** Read 1 bytes from log buffer.
@param[in]   ptr      pointer to buffer
@param[in]   end_ptr  pointer to end of buffer
@param[out]  val      read 2 bytes value */
static const byte *read_1_bytes(const byte *ptr, const byte *end_ptr,
                                uint8_t &val) {
  if (end_ptr < ptr + 1) {
    return (nullptr);
  }
  val = mach_read_from_1(ptr);
  ptr += 1;
  return ptr;
}

/** Read number of columns for index.
@param[in]   ptr           pointer to buffer
@param[in]   end_ptr       pointer to end of buffer
@param[in]   is_comp       true if COMP
@param[in]   is_versioned  true if table has row versions
@param[in]   is_instant    true if table has INSTANT cols
@param[out]  n             number of index fields
@param[out]  n_uniq        n_uniq for index
@param[out]  inst_cols     number of column before first instant add was done.
@return pointer to buffer. */
static const byte *parse_index_column_counts(const byte *ptr,
                                             const byte *end_ptr, bool is_comp,
                                             bool is_versioned, bool is_instant,
                                             uint16_t &n, uint16_t &n_uniq,
                                             uint16_t &inst_cols) {
  if (!is_versioned && !is_comp) {
    n = n_uniq = 1;
    inst_cols = 0;
    return ptr;
  }

  /* Parse n */
  ptr = read_2_bytes(ptr, end_ptr, n);
  if (ptr == nullptr) {
    return ptr;
  }

  if (!is_comp) {
    ut_ad(is_versioned);
    return ptr;
  }

  if (is_instant) {
    ptr = read_2_bytes(ptr, end_ptr, inst_cols);
    if (ptr == nullptr) {
      return ptr;
    }
  }

  ptr = read_2_bytes(ptr, end_ptr, n_uniq);
  if (ptr == nullptr) {
    return ptr;
  }
  ut_ad(n_uniq <= n);

  return ptr;
}

/** Parse index fields.
@param[in]       ptr      pointer to buffer
@param[in]       end_ptr  pointer to end of buffer
@param[in]       n        number of fields
@param[in]       n_uniq   n_uniq
@param[in]       is_versioned  true if table has row versions
@param[in,out]   ind      dummy index
@param[in,out]   table    dummy table
@return pointer to log buffer */
static const byte *parse_index_fields(const byte *ptr, const byte *end_ptr,
                                      uint16_t n, uint16_t n_uniq,
                                      bool is_versioned, dict_index_t *&ind,
                                      dict_table_t *&table) {
  for (size_t i = 0; i < n; i++) {
    /* For redundant, col len metadata isn't needed for recovery as it is
    part of record itself. */
    uint16_t len = 0;
    ptr = read_2_bytes(ptr, end_ptr, len);
    if (ptr == nullptr) {
      return (nullptr);
    }

    uint32_t phy_pos = UINT32_UNDEFINED;
    uint8_t v_added = UINT8_UNDEFINED;
    uint8_t v_dropped = UINT8_UNDEFINED;

    /* The high-order bit of len is the NOT NULL flag;
    the rest is 0 or 0x7fff for variable-length fields,
    and 1..0x7ffe for fixed-length fields. */
    dict_mem_table_add_col(
        table, nullptr, nullptr,
        ((len + 1) & 0x7fff) <= 1 ? DATA_BINARY : DATA_FIXBINARY,
        len & 0x8000 ? DATA_NOT_NULL : 0, len & 0x7fff, true, phy_pos, v_added,
        v_dropped);

    /* The is_ascending flag does not matter during
    redo log apply, because we do not compare for
    "less than" or "greater than". */
    dict_index_add_col(ind, table, table->get_col(i), 0, true);
  }

  dict_table_add_system_columns(table, table->heap);

  /* Identify DB_TRX_ID and DB_ROLL_PTR in the index. */
  if (is_versioned || (n_uniq != n)) {
    size_t i = 0;
    i = DATA_TRX_ID - 1 + n_uniq;
    ut_a(DATA_TRX_ID_LEN == ind->get_col(i)->len);
    ind->fields[i].col = &table->cols[n + DATA_TRX_ID];
    ind->fields[i].col->set_phy_pos(table->cols[i].get_phy_pos());

    i = DATA_ROLL_PTR - 1 + n_uniq;
    ut_a(DATA_ROLL_PTR_LEN == ind->get_col(i)->len);
    ind->fields[i].col = &table->cols[n + DATA_ROLL_PTR];
    ind->fields[i].col->set_phy_pos(table->cols[i].get_phy_pos());
  }

  table->initial_col_count = table->current_col_count = table->total_col_count =
      n;
  return ptr;
}

struct Field_instant_info {
  uint16_t logical_pos{UINT16_UNDEFINED};
  uint16_t phy_pos{UINT16_UNDEFINED};
  uint8_t v_added{UINT8_UNDEFINED};
  uint8_t v_dropped{UINT8_UNDEFINED};
};

using instant_fields_list_t = std::vector<Field_instant_info>;

/** Parse the fields with versions.
@param[in]   ptr       pointer to buffer
@param[in]   end_ptr   pointer to end of buffer
@param[out]  f         vector of fields with versions
@param[out]  crv       current row version
@param[out]  n_dropped number of dropped columns */
static const byte *parse_index_versioned_fields(const byte *ptr,
                                                const byte *end_ptr,
                                                instant_fields_list_t &f,
                                                uint16_t &crv,
                                                size_t &n_dropped) {
  uint16_t n_inst = 0;
  ptr = read_2_bytes(ptr, end_ptr, n_inst);
  if (ptr == nullptr) return (nullptr);
  ut_ad(n_inst > 0);

  for (auto i = n_inst; i > 0; --i) {
    Field_instant_info info;

    ptr = read_2_bytes(ptr, end_ptr, info.logical_pos);
    if (ptr == nullptr) return (nullptr);

    ptr = read_2_bytes(ptr, end_ptr, info.phy_pos);
    if (ptr == nullptr) return (nullptr);

    if ((info.phy_pos & 0x8000) != 0) {
      info.phy_pos &= ~0x8000;

      /* Read v_added */
      ptr = read_1_bytes(ptr, end_ptr, info.v_added);
      if (ptr == nullptr) return (nullptr);
      ut_ad(info.v_added != UINT8_UNDEFINED);
      crv = std::max(crv, (uint16_t)info.v_added);
    }

    if ((info.phy_pos & 0x4000) != 0) {
      info.phy_pos &= ~0x4000;

      /* Read v_dropped */
      ptr = read_1_bytes(ptr, end_ptr, info.v_dropped);
      if (ptr == nullptr) return (nullptr);
      ut_ad(info.v_dropped != UINT8_UNDEFINED);
      crv = std::max(crv, (uint16_t)info.v_dropped);
      n_dropped++;
    }

    ut_ad((info.phy_pos & 0xC000) == 0);

    f.push_back(info);
  }

  return (ptr);
}

/** Update the version info for the columns.
NOTE : fields are logged in their physical order so with the help of phy_pos,
it's easy to locate them.
@param[in]      f      fields with versions
@param[in,out]  index  dummy index */
static void update_instant_info(instant_fields_list_t f, dict_index_t *index) {
  if (f.empty()) {
    return;
  }

  size_t n_added = 0;
  size_t n_dropped = 0;

  for (auto field : f) {
    bool is_added = field.v_added != UINT8_UNDEFINED;
    bool is_dropped = field.v_dropped != UINT8_UNDEFINED;

    dict_col_t *col = index->fields[field.logical_pos].col;

    if (is_dropped) {
      col->set_version_dropped(field.v_dropped);
      n_dropped++;
      if (col->is_nullable()) {
        ut_a(index->n_nullable > 0);
        --index->n_nullable;
      }
    }

    if (is_added) {
      col->set_version_added(field.v_added);
      n_added++;
    }

    col->set_phy_pos(field.phy_pos);
  }

  index->table->initial_col_count -= n_added;
  index->table->current_col_count -= n_dropped;
  index->table->n_cols -= n_dropped;
}

/** To populate dummy fields. Used only in case of REDUNDANT row format.
@param[in,out]  index    dummy index
@param[in,out]  table    dummy table
@param[in]      n        number of fields
@param[in]     is_comp  true if COMP
*/
static void populate_dummy_fields(dict_index_t *index, dict_table_t *table,
                                  size_t n IF_DEBUG(, bool is_comp)) {
  ut_ad(!is_comp);

  uint32_t phy_pos = UINT32_UNDEFINED;
  uint8_t v_added = UINT8_UNDEFINED;
  uint8_t v_dropped = UINT8_UNDEFINED;
  size_t dummy_len = 10;

  for (size_t i = 0; i < n; i++) {
    dict_mem_table_add_col(table, nullptr, nullptr, DATA_BINARY, DATA_NOT_NULL,
                           dummy_len, true, phy_pos, v_added, v_dropped);

    dict_index_add_col(index, table, table->get_col(i), 0, true);
  }
  table->initial_col_count = table->current_col_count = table->total_col_count =
      n;
}

static const byte *parse_index_log_version(const byte *ptr, const byte *end_ptr,
                                           uint8_t &version) {
  ptr = read_1_bytes(ptr, end_ptr, version);
  if (ptr == nullptr) return nullptr;

  return ptr;
}

static const byte *parse_index_flag(const byte *ptr, const byte *end_ptr,
                                    uint8_t &flag) {
  ptr = read_1_bytes(ptr, end_ptr, flag);
  if (ptr == nullptr) return nullptr;

  return ptr;
}

const byte *mlog_parse_index(const byte *ptr, const byte *end_ptr,
                             dict_index_t **index) {
  /* Read the 1 byte for index log version */
  uint8_t index_log_version = 0;
  ptr = parse_index_log_version(ptr, end_ptr, index_log_version);
  if (ptr == nullptr) {
    return nullptr;
  }

  switch (index_log_version) {
    case INDEX_LOG_VERSION_CURRENT:
      ptr = mlog_parse_index_v1(ptr, end_ptr, index);
      break;
    case INDEX_LOG_VERSION_0:
      /* INDEX_LOG_VERSION_0 is used in 8.0.29 and in 8.0.30 REDO log format
      has changed which expects REDOs from < 8.0.30 to be logically empty. Thus
      we shall never reach here. */
      ut_error;
    default:
      ib::fatal(UT_LOCATION_HERE, ER_IB_INDEX_LOG_VERSION_MISMATCH,
                (unsigned int)index_log_version,
                (unsigned int)INDEX_LOG_VERSION_MAX);
  }

  return ptr;
}

static const byte *mlog_parse_index_v1(const byte *ptr, const byte *end_ptr,
                                       dict_index_t **index) {
  /* Read the 1 byte flag */
  uint8_t flag = 0;
  ptr = parse_index_flag(ptr, end_ptr, flag);
  if (ptr == nullptr) {
    return nullptr;
  }

  const bool is_comp = IS_COMPACT(flag);
  const bool is_versioned = IS_VERSIONED(flag);
  const bool is_instant = IS_INSTANT(flag);

  /* Read n and n_uniq */
  uint16_t n = 0;
  uint16_t n_uniq = 0;
  uint16_t inst_cols = 0;
  ptr = parse_index_column_counts(ptr, end_ptr, is_comp, is_versioned,
                                  is_instant, n, n_uniq, inst_cols);
  if (ptr == nullptr) {
    return ptr;
  }
  ut_ad(inst_cols == 0 || is_instant);

  /* Create a dummy dict_table_t */
  dict_table_t *table =
      dict_mem_table_create(RECOVERY_INDEX_TABLE_NAME, DICT_HDR_SPACE, n, 0, 0,
                            is_comp ? DICT_TF_COMPACT : 0, 0);

  if (inst_cols > 0) {
    table->set_instant_cols(inst_cols);
  }

  /* Create a dummy dict_index_t */
  dict_index_t *ind =
      dict_mem_index_create(RECOVERY_INDEX_TABLE_NAME,
                            RECOVERY_INDEX_TABLE_NAME, DICT_HDR_SPACE, 0, n);
  ind->table = table;
  ind->n_uniq = (unsigned int)n_uniq;
  if (n_uniq != n) {
    ut_a(n_uniq + DATA_ROLL_PTR <= n);
    ind->type = DICT_CLUSTERED;
  }

  if (is_comp) {
    /* Read each index field info */
    ptr = parse_index_fields(ptr, end_ptr, n, n_uniq, is_versioned, ind, table);
    if (ptr == nullptr) {
      *index = ind;
      return ptr;
    }
  } else if (is_versioned) {
    /* Populate dummy cols/fields and link them */
    populate_dummy_fields(ind, table, n IF_DEBUG(, is_comp));
  }

  size_t n_dropped = 0;
  if (is_versioned) {
    /* Read the fields with version added/dropped */
    instant_fields_list_t f;
    uint16_t current_row_version = 0;
    ptr = parse_index_versioned_fields(ptr, end_ptr, f, current_row_version,
                                       n_dropped);
    if (ptr == nullptr) {
      *index = ind;
      return (ptr);
    }
    ind->table->current_row_version = current_row_version;

    /* Update fields INSTANT info */
    update_instant_info(f, ind);

    bool *phy_pos_bitmap = new bool[ind->n_def];
    memset(phy_pos_bitmap, false, (sizeof(bool) * ind->n_def));
    for (auto field : f) {
      phy_pos_bitmap[field.phy_pos] = true;
    }
    f.clear();

    /* For the remaining columns, update physical pos */
    int shift_count = 0;
    for (size_t i = 0; i < ind->n_def; i++) {
      dict_field_t *field = ind->get_field(i);
      if (field->col->get_phy_pos() == UINT32_UNDEFINED) {
        uint16_t phy_pos = i + shift_count;
        ut_ad(phy_pos < ind->n_def);
        while (phy_pos_bitmap[phy_pos]) {
          phy_pos++;
        }
        field->col->set_phy_pos(phy_pos);
        phy_pos_bitmap[phy_pos] = true;
      } else {
        if (field->col->is_instant_added() &&
            !field->col->is_instant_dropped()) {
          shift_count--;
        }
      }
    }

    delete[] phy_pos_bitmap;
    ind->row_versions = true;
  }

  ind->n_fields = n - n_dropped;
  ind->n_total_fields = n;

  /* For upgraded table from v1, set following */
  if (inst_cols > 0) {
    ind->instant_cols = true;
    const size_t n_instant_fields = ind->get_instant_fields();
    size_t new_n_nullable = ind->calculate_n_instant_nullable(n_instant_fields);
    ind->set_instant_nullable(new_n_nullable);
  }

  table->is_system_table = false;

  if (is_instant || is_versioned) {
    if (is_versioned) {
      ut_ad(ind->has_row_versions());
      ind->create_fields_array();
    }
    if (is_instant) {
      ind->table->set_upgraded_instant();
    }
    ind->type = DICT_CLUSTERED;
    ind->create_nullables(table->current_row_version);
  }

  /* avoid ut_ad(index->cached) in dict_index_get_n_unique_in_tree */
  ind->cached = true;
  *index = ind;
  return (ptr);
}
