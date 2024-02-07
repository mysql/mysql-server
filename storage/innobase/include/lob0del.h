/*****************************************************************************

Copyright (c) 2015, 2024, Oracle and/or its affiliates.

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
#ifndef lob0del_h
#define lob0del_h

#include "lob0lob.h"

namespace lob {

/* Delete a LOB */
class Deleter {
 public:
  /** Constructor */
  Deleter(DeleteContext &ctx) : m_ctx(ctx) {
    ut_ad(ctx.index()->is_clustered());
    ut_ad(mtr_memo_contains_flagged(ctx.get_mtr(),
                                    dict_index_get_lock(ctx.index()),
                                    MTR_MEMO_X_LOCK | MTR_MEMO_SX_LOCK) ||
          ctx.table()->is_intrinsic());
    ut_ad(mtr_is_page_fix(ctx.get_mtr(), ctx.m_blobref.page_align(),
                          MTR_MEMO_PAGE_X_FIX, ctx.table()));
    ut_ad(ctx.rec_offs_validate());
    ut_ad(ctx.validate_blobref());
  }

  /** Free the LOB object.
  @return DB_SUCCESS on success. */
  dberr_t destroy();

  /** Free the first page of the BLOB and update the BLOB reference
  in the clustered index.
  @return DB_SUCCESS on pass, error code on failure. */
  dberr_t free_first_page();

 private:
  /** Obtain an x-latch on the clustered index record page.*/
  void x_latch_rec_page();

  /** Validate the page type of the given page frame.
  @param[in]    page    the page frame.
  @return true if valid, false otherwise. */
  bool validate_page_type(const page_t *page) const {
    return (m_ctx.is_compressed() ? validate_zblob_page_type(page)
                                  : validate_blob_page_type(page));
  }

  /** Check if the page type is set correctly.
  @param[in]    page    the page frame.
  @return true if page type is correct. */
  bool validate_zblob_page_type(const page_t *page) const {
    const page_type_t pt = fil_page_get_type(page);
    switch (pt) {
      case FIL_PAGE_TYPE_ZBLOB:
      case FIL_PAGE_TYPE_ZBLOB2:
      case FIL_PAGE_SDI_ZBLOB:
        break;
      default:
        ut_error;
    }
    return (true);
  }

  /** Check if the page type is set correctly.
  @param[in]    page    the page frame.
  @return true if page type is correct. */
  bool validate_blob_page_type(const page_t *page) const {
    const page_type_t type = fil_page_get_type(page);

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
          return (true);
        }
#endif /* !UNIV_DEBUG */
        ut_error;
    }
    return (true);
  }

  /** Check if the BLOB can be freed.  If the clustered index record
  is not the owner of the LOB, then it cannot be freed.  Also, during
  rollback, if inherited flag is set, then LOB will not be freed.
  @return true if the BLOB can be freed, false otherwise. */
  bool can_free() const;

  DeleteContext &m_ctx;
  mtr_t m_mtr;
};

}  // namespace lob

#endif  // lob0del_h
