/*****************************************************************************

Copyright (c) 2017, 2023, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is also distributed with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have included with MySQL.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file include/lob0undo.h
 Undo logging small changes to BLOBs. */

#ifndef lob0undo_h
#define lob0undo_h

#include <list>
#include "dict0mem.h"
#include "mem0mem.h"
#include "univ.i"

namespace lob {

/** Undo information about LOB data alone without including LOB index. */
struct undo_data_t {
  /** Apply the undo information to the given LOB.
  @param[in]    index           clustered index containing the LOB.
  @param[in]    lob_mem         LOB on which the given undo will be
                                  applied.
  @param[in]    len             length of LOB.
  @param[in]    lob_version     lob version number
  @param[in]    first_page_no   the first page number of lob */
  void apply(dict_index_t *index, byte *lob_mem, size_t len, size_t lob_version,
             page_no_t first_page_no);

  /** The LOB first page number. */
  page_no_t m_page_no = FIL_NULL;

  /** The LOB version number on which this undo should be applied. */
  ulint m_version = 0;

  /** The offset within LOB where partial update happened. */
  ulint m_offset = 0;

  /** The length of the modification. */
  ulint m_length = 0;

  /** Changes to the LOB data. */
  byte *m_old_data = nullptr;

  /** Copy the old data from the undo page into this object.
  @param[in]  undo_ptr  the pointer into the undo log record.
  @param[in]  len       length of the old data.
  @return pointer past the old data. */
  const byte *copy_old_data(const byte *undo_ptr, ulint len);

  /** Free allocated memory for old data. */
  void destroy();

  std::ostream &print(std::ostream &out) const;
};

inline std::ostream &operator<<(std::ostream &out, const undo_data_t &obj) {
  return (obj.print(out));
}

/** Container to hold a sequence of undo log records containing modification
of BLOBs. */
struct undo_seq_t {
  /** Constructor.
  @param[in]    field_no        the field number of LOB.*/
  undo_seq_t(ulint field_no) : m_field_no(field_no), m_undo_list(nullptr) {}

  /** Apply the undo log records on the given LOB in memory.
  @param[in]    index   the clustered index to which LOB belongs.
  @param[in]    lob     the BLOB in memory.
  @param[in]    len     the length of BLOB in memory.
  @param[in]    lob_version     the LOB version number.
  @param[in]    first_page_no   the first page number of BLOB.*/
  void apply(dict_index_t *index, byte *lob, size_t len, size_t lob_version,
             page_no_t first_page_no) {
    if (m_undo_list != nullptr) {
      for (auto iter = m_undo_list->begin(); iter != m_undo_list->end();
           ++iter) {
        iter->apply(index, lob, len, lob_version, first_page_no);
      }
    }
  }

  /** Get the field number of BLOB.
  @return the field number of BLOB. */
  ulint get_field_no() const { return (m_field_no); }

  /** Append the given undo log record to the end of container.
  @param[in]    u1      the undo log record information. */
  void push_back(undo_data_t &u1) {
    if (m_undo_list == nullptr) {
      m_undo_list =
          ut::new_withkey<std::list<undo_data_t>>(UT_NEW_THIS_FILE_PSI_KEY);
    }
    m_undo_list->push_back(u1);
  }

  /** Destroy the contents of this undo sequence list. */
  void destroy() {
    if (m_undo_list != nullptr) {
      std::for_each(m_undo_list->begin(), m_undo_list->end(),
                    [](undo_data_t &obj) { obj.destroy(); });
      m_undo_list->clear();
      ut::delete_(m_undo_list);
      m_undo_list = nullptr;
    }
  }

  /** Check if any undo log exists to apply. */
  bool exists() const {
    return (m_undo_list == nullptr ? false : !m_undo_list->empty());
  }

  ulint m_field_no;

 private:
  std::list<undo_data_t> *m_undo_list = nullptr;
};

/** The list of modifications to be applied on LOBs to get older versions.
Given a field number, it should be able to obtain the list of undo
information. */
struct undo_vers_t {
 public:
  /** Get the undo log sequence object for the given field number, which
  represents one blob.
  @param[in]    field_no   the field number of the blob.
  @return the undo sequence object or nullptr. */
  undo_seq_t *get_undo_sequence_if_exists(ulint field_no) {
    if (m_versions == nullptr) {
      return (nullptr);
    } else {
      for (auto iter = m_versions->begin(); iter != m_versions->end(); ++iter) {
        if ((*iter)->get_field_no() == field_no) {
          return (*iter);
        }
      }
    }
    return (nullptr);
  }

  /** Get the undo log sequence object for the given field number, which
  represents one blob.  The undo sequence object is allocated if it does
  not exist.
  @param[in]    field_no   the field number of the blob.
  @return the undo sequence object. */
  undo_seq_t *get_undo_sequence(ulint field_no) {
    if (m_versions == nullptr) {
      m_versions =
          ut::new_withkey<std::list<undo_seq_t *>>(UT_NEW_THIS_FILE_PSI_KEY);
    } else {
      for (auto iter = m_versions->begin(); iter != m_versions->end(); ++iter) {
        if ((*iter)->get_field_no() == field_no) {
          return (*iter);
        }
      }
    }

    undo_seq_t *seq =
        ut::new_withkey<undo_seq_t>(UT_NEW_THIS_FILE_PSI_KEY, field_no);
    m_versions->push_back(seq);

    return (seq);
  }

  /** Empty the collected LOB undo information from cache. */
  void reset() {
    if (m_versions != nullptr) {
      for (auto iter = m_versions->begin(); iter != m_versions->end(); ++iter) {
        (*iter)->destroy();
        ut::delete_(*iter);
      }
      m_versions->clear();
    }
  }

  /** Apply the undo log record on the given LOB in memory.
  @param[in]    clust_index     the clust index to which LOB belongs.
  @param[in]    field_no        the field number of the LOB.
  @param[in]    lob             the LOB data.
  @param[in]    len             the length of LOB.
  @param[in]    lob_version     LOB version number.
  @param[in]    first_page      the first page number of LOB.*/
  void apply(dict_index_t *clust_index, ulint field_no, byte *lob, size_t len,
             size_t lob_version, page_no_t first_page) {
    undo_seq_t *seq = get_undo_sequence_if_exists(field_no);

    if (seq != nullptr) {
      seq->apply(clust_index, lob, len, lob_version, first_page);
    }
  }

  /** Destroy the accumulated undo_seq_t objects. */
  void destroy() {
    if (m_versions != nullptr) {
      reset();
      ut::delete_(m_versions);
      m_versions = nullptr;
    }
  }

  /** Check if the undo contained older versions.
  @return true if there are no older versions, false otherwise. */
  bool is_empty() const { return (m_versions->empty()); }

  /** Destructor to free the resources. */
  ~undo_vers_t() { destroy(); }

 private:
  /** Maintain a list of undo_seq_t objects. */
  std::list<undo_seq_t *> *m_versions = nullptr;
};

} /* namespace lob */

#endif /* lob0undo_h */
