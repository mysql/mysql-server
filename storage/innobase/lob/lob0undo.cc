/*****************************************************************************

Copyright (c) 2017, 2018 Oracle and/or its affiliates. All Rights Reserved.

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

#include "lob0undo.h"
#include "dict0dict.h"
#include "sql/json_binary.h"

namespace lob {

/** Apply the undo information to the given LOB.
@param[in]	index		clustered index containing the LOB.
@param[in]	lob_mem		LOB on which the given undo will be
                                applied.
@param[in]    len             length of LOB.
@param[in]    lob_version     lob version number
@param[in]    first_page_no   the first page number of lob */
void undo_data_t::apply(dict_index_t *index, byte *lob_mem, size_t len,
                        size_t lob_version, page_no_t first_page_no) {
  DBUG_ENTER("undo_data_t::apply");

  DBUG_LOG("undo_data_t", "lob_version=" << lob_version);

#ifdef UNIV_DEBUG
  if (!dict_table_has_atomic_blobs(index->table)) {
    /* For compact and redundant row format, remove the local
    prefix length from the offset. */

    ut_ad(m_offset >= DICT_ANTELOPE_MAX_INDEX_COL_LEN);
  }
#endif /* UNIV_DEBUG */

  /* Ensure that the undo log applied on the LOB is matching. */
  if (first_page_no == m_page_no) {
    byte *ptr = lob_mem + m_offset;
    ut_ad((m_offset + m_length) <= len);
    memcpy(ptr, m_old_data, m_length);
  }

  DBUG_VOID_RETURN;
}

std::ostream &undo_data_t::print(std::ostream &out) const {
  out << "[undo_data_t: m_version=" << m_version << ", m_offset=" << m_offset
      << ", m_length=" << m_length
      << ", m_old_data=" << PrintBuffer(m_old_data, m_length) << "]";
  return (out);
}

/** Copy the old data from the undo page into this object.
@param[in]  undo_ptr  the pointer into the undo log record.
@param[in]  len       length of the old data.
@return pointer past the old data. */
const byte *undo_data_t::copy_old_data(const byte *undo_ptr, ulint len) {
  m_length = len;
  m_old_data = UT_NEW_ARRAY_NOKEY(byte, m_length);
  if (m_old_data == nullptr) {
    return (nullptr);
  }
  memcpy(m_old_data, undo_ptr, m_length);
  return (undo_ptr + m_length);
}

/** Free allocated memory for old data. */
void undo_data_t::destroy() {
  if (m_old_data != nullptr) {
    UT_DELETE_ARRAY(m_old_data);
    m_old_data = nullptr;
  }
}

} /* namespace lob */
