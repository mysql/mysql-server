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
#ifndef lob0zip_h
#define lob0zip_h

#include "lob0ins.h"

namespace lob {

/** Insert or write the compressed BLOB as a single zlib stream. */
class zInserter : private BaseInserter {
public:
  /** Constructor.
  @param[in]	ctx	blob operation context. */
  zInserter(InsertContext *ctx) : BaseInserter(ctx), m_heap(NULL) {}

  /** Destructor. */
  ~zInserter();

  /** Prepare to write a compressed BLOB. Setup the zlib
  compression stream.
  @return DB_SUCCESS on success, error code on failure. */
  dberr_t prepare();

  /** Write all the BLOBs of the clustered index record.
  @return DB_SUCCESS on success, error code on failure. */
  dberr_t write();

	/** Cleanup after completing the write of compressed BLOB.
	@param[in]	validate	if true, validate all the
					lob references. if false,
					skip this validation.
	@return DB_SUCCESS on success, error code on failure. */
	dberr_t finish(bool validate = true)
	{
		int ret = deflateEnd(&m_stream);
		ut_ad(ret == Z_OK);
		ut_ad(!validate || validate_blobrefs());

		if (ret != Z_OK) {
			m_err = DB_FAIL;
		}
		return (m_err);
	}

  /** Write the page type of the BLOB page and also generate the
  redo log record.
  @param[in]	blob_page	the BLOB page
  @param[in]	nth_blob_page	the count of BLOB page from
                                  the beginning of the BLOB. */
  void log_page_type(page_t *blob_page, ulint nth_blob_page) {
    page_type_t page_type;

    if (is_index_sdi()) {
      page_type = FIL_PAGE_SDI_ZBLOB;
    } else if (nth_blob_page == 0) {
      page_type = FIL_PAGE_TYPE_ZBLOB;
    } else {
      page_type = FIL_PAGE_TYPE_ZBLOB2;
    }

    mlog_write_ulint(blob_page + FIL_PAGE_TYPE, page_type, MLOG_2BYTES,
                     &m_blob_mtr);
  }

  /** Calculate the total number of pages needed to store
  the given blobs */
  ulint calc_total_pages() {
    const page_size_t page_size = m_ctx->page_size();

    /* Space available in compressed page to carry blob data */
    const ulint payload_size_zip = page_size.physical() - FIL_PAGE_DATA;

    const big_rec_t *vec = m_ctx->get_big_rec_vec();

    ulint total_blob_pages = 0;
    for (ulint i = 0; i < vec->n_fields; i++) {
      total_blob_pages +=
          static_cast<ulint>(
              deflateBound(&m_stream, static_cast<uLong>(vec->fields[i].len)) +
              payload_size_zip - 1) /
          payload_size_zip;
    }

    return (total_blob_pages);
  }

  /** Write contents into a single BLOB page.
  @return code as returned by zlib. */
  int write_into_single_page();

  /** Commit the BLOB mtr. */
  void commit_blob_mtr() { mtr_commit(&m_blob_mtr); }

  /** Write one blob page.  This function will be repeatedly called
  with an increasing nth_blob_page to completely write a BLOB.
  @param[in]	blob_j		the jth blob object of the record.
  @param[in]	field		the big record field.
  @param[in]	nth_blob_page	count of the BLOB page (starting from 1).
  @return code as returned by the zlib. */
  int write_single_blob_page(size_t blob_j, big_rec_field_t &field,
                             ulint nth_blob_page);

  /** Write first blob page.
  @param[in]	blob_j		the jth blob object of the record.
  @param[in]	field		the big record field.
  @return code as returned by the zlib. */
  int write_first_page(size_t blob_j, big_rec_field_t &field);

  /** Verify that all pointers to externally stored columns in the record
  is be valid.  If validation fails, this function doesn't return.
  @return true if valid. */
  bool validate_blobrefs() const {
    const ulint *offsets = m_ctx->get_offsets();

    for (ulint i = 0; i < rec_offs_n_fields(offsets); i++) {
      if (!rec_offs_nth_extern(offsets, i)) {
        continue;
      }

      byte *field_ref = btr_rec_get_field_ref(m_ctx->rec(), offsets, i);

      ref_t blobref(field_ref);

      /* The pointer must not be zero if the operation
      succeeded. */
      ut_a(!blobref.is_null() || m_err != DB_SUCCESS);

      /* The column must not be disowned by this record. */
      ut_a(blobref.is_owner());
    }
    return (true);
  }

  /** For the given blob field, update its length in the blob reference
  which is available in the clustered index record.
  @param[in]	field	the concerned blob field. */
  void update_length_in_blobref(big_rec_field_t &field);

  /** Make the current page as next page of previous page.  In other
  words, make the page m_cur_blob_page_no as the next page
  (FIL_PAGE_NEXT) of page m_prev_page_no.
  @return DB_SUCCESS on success, or error code on failure. */
  dberr_t set_page_next();

	/** Write one small blob field data. Refer to ref_t to determine
	the definition of small blob.
	@param[in]	blob_j	the blob field number
	@return DB_SUCCESS on success, error code on failure. */
	dberr_t write_one_small_blob(size_t blob_j);

private:
  /** Add the BLOB page information to the directory
  @param[in]	page_info	BLOB page information. */
  void add_to_blob_dir(const blob_page_info_t &page_info) {
    m_dir.add(page_info);
  }

  /** Write one blob field data.
  @param[in]	blob_j	the blob field number
  @return DB_SUCCESS on success, error code on failure. */
  dberr_t write_one_blob(size_t blob_j);

  mem_heap_t *m_heap;
  z_stream m_stream;

  /** The BLOB directory information. */
  blob_dir_t m_dir;
};

inline zInserter::~zInserter() {
  if (m_heap != NULL) {
    mem_heap_free(m_heap);
  }
}

} // namespace lob

#endif // lob0zip_h
