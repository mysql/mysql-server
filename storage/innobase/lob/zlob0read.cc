/*****************************************************************************

Copyright (c) 2016, 2023, Oracle and/or its affiliates.

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

#include "zlob0read.h"
#include "lob0impl.h"
#include "lob0lob.h"
#include "lob0zip.h"
#include "my_dbug.h"
#include "trx0trx.h"
#include "univ.i"
#include "ut0ut.h"
#include "zlob0first.h"

namespace lob {

/** Fetch a compressed large object (ZLOB) from the system.
@param[in] ctx    the read context information.
@param[in] ref    the LOB reference identifying the LOB.
@param[in] offset read the LOB from the given offset.
@param[in] len    the length of LOB data that needs to be fetched.
@param[out] buf   the output buffer (owned by caller) of minimum len bytes.
@return the amount of data (in bytes) that was actually read. */
ulint z_read(ReadContext *ctx, lob::ref_t ref, ulint offset, ulint len,
             byte *buf) {
  ut_ad(offset == 0);
  ut_ad(len > 0);

  const ulint avail_lob = ref.length();

  if (avail_lob == 0) {
    return (0);
  }

  if (ref.is_being_modified()) {
    /* This should happen only for READ UNCOMMITTED transactions. */
    ut_ad(ctx->assert_read_uncommitted());
    return (0);
  }

  const uint32_t lob_version = ref.version();

  fil_addr_t old_node_loc = fil_addr_null;
  fil_addr_t node_loc = fil_addr_null;

  ut_ad(ctx->m_index->is_clustered());
  ut_ad(ctx->m_space_id == ref.space_id());

  const page_no_t first_page_no = ref.page_no();
  const space_id_t space_id = ctx->m_space_id;
  const page_id_t first_page_id(space_id, first_page_no);

  mtr_t mtr;
  mtr_start(&mtr);

  /* The current entry - it is the latest version. */
  z_index_entry_t cur_entry(&mtr, ctx->m_index);

  z_first_page_t first(&mtr, ctx->m_index);
  first.load_x(first_page_no);

  page_type_t page_type = first.get_page_type();

  if (page_type == FIL_PAGE_TYPE_ZBLOB || page_type == FIL_PAGE_SDI_ZBLOB) {
    mtr_commit(&mtr);
    lob::zReader reader(*ctx);
    reader.fetch();
    return (reader.length());
  }

  if (page_type != FIL_PAGE_TYPE_ZLOB_FIRST) {
    /* Assume that the BLOB has been freed and return without taking further
    action.  This condition is hit when there are stale LOB references in the
    clustered index record, especially when there are server crashes during
    updation of delete-marked clustered index record with external fields. */
    mtr_commit(&mtr);
    return (0);
  }

  flst_base_node_t *flst = first.index_list();

#ifdef UNIV_DEBUG
  /* The length of this list cannot be equal to 0 */
  ulint flst_len = flst_get_len(flst);
  ut_a(flst_len != 0);

  std::vector<trx_id_t> trxids;
#endif /* UNIV_DEBUG */

  node_loc = flst_get_first(flst, &mtr);

  byte *ptr = buf;
  ulint remain = len;

  while (remain > 0 && !fil_addr_is_null(node_loc)) {
    flst_node_t *node = first.addr2ptr_x(node_loc);
    cur_entry.reset(node);

#ifdef UNIV_DEBUG
    trxids.push_back(cur_entry.get_trx_id());
#endif /* UNIV_DEBUG */

    /* Obtain the next index entry location. */
    node_loc = cur_entry.get_next();
    const uint32_t entry_lob_version = cur_entry.get_lob_version();

    if (entry_lob_version <= lob_version) {
      /* Can read the entry. */
      ut_ad(entry_lob_version <= lob_version);
      z_read_chunk(ctx->m_index, cur_entry, 0, remain, ptr, &mtr);
    } else {
      ut_ad(entry_lob_version > lob_version);

      /* Cannot read the entry. Look at older versions. */
      flst_base_node_t *ver_lst = cur_entry.get_versions_list();
      old_node_loc = flst_get_first(ver_lst, &mtr);

      bool got_data = false;
      while (!fil_addr_is_null(old_node_loc)) {
        flst_node_t *old_node = first.addr2ptr_x(old_node_loc);

        /* The older version of the current entry. */
        z_index_entry_t old_version(old_node, &mtr, ctx->m_index);
#ifdef UNIV_DEBUG
        trxids.push_back(old_version.get_trx_id());
#endif /* UNIV_DEBUG */
        const uint32_t old_lob_version = old_version.get_lob_version();

        if (old_lob_version <= lob_version) {
          /* Can read the entry. */
          ut_ad(old_version.get_lob_version() <= lob_version);
          z_read_chunk(ctx->m_index, old_version, 0, remain, ptr, &mtr);
          got_data = true;
          break;
        }
        old_node_loc = old_version.get_next();
      }

      /* For this offset, if none of the versions are
      visible, go ahead and read the latest version.  The
      DB_TRX_ID in the clustered index record can be checked
      here to verify that this is correct.*/
      if (!got_data) {
        z_read_chunk(ctx->m_index, cur_entry, 0, remain, ptr, &mtr);
      }
    }

    cur_entry.reset(nullptr);
    mtr_commit(&mtr);
    mtr_start(&mtr);
    first.load_x(first_page_no);
  }

  const ulint total_read = len - remain;
  ut_ad(total_read == len || total_read == avail_lob);
  mtr_commit(&mtr);
  return (total_read);
}

/** Read one data chunk associated with one index entry.
@param[in]      index   The clustered index containing the LOB.
@param[in]      entry   Pointer to the index entry
@param[in]      offset  The offset from which to read the chunk.
@param[in,out]  len     The length of the output buffer. This length can
                        be greater than the chunk size.
@param[in,out]  buf     The output buffer.
@param[in]      mtr     Mini-transaction context.
@return number of bytes copied into the output buffer. */
ulint z_read_chunk(dict_index_t *index, z_index_entry_t &entry, ulint offset,
                   ulint &len, byte *&buf, mtr_t *mtr) {
  const ulint data_len = entry.get_data_len();

  if (entry.get_z_page_no() == FIL_NULL || data_len == 0) {
    return (0);
  }

  const ulint zbuf_size = entry.get_zdata_len();
  std::unique_ptr<byte[]> zbuf(new byte[zbuf_size]);

  ulint zbytes = z_read_strm(index, entry, zbuf.get(), zbuf_size, mtr);
  ut_a(zbytes == zbuf_size);

  z_stream strm;
  strm.zalloc = nullptr;
  strm.zfree = nullptr;
  strm.opaque = nullptr;

  int ret = inflateInit(&strm);
  ut_a(ret == Z_OK);

  strm.avail_in = static_cast<uInt>(zbytes);
  strm.next_in = zbuf.get();

  ulint to_copy = 0;
  if (offset == 0 && len >= data_len) {
    /* The full chunk is needed for output. */
    strm.avail_out = static_cast<uInt>(len);
    strm.next_out = buf;

    ret = inflate(&strm, Z_FINISH);
    ut_a(ret == Z_STREAM_END);

    to_copy = strm.total_out;

  } else {
    /* Only part of the chunk is needed. */
    byte ubuf[Z_CHUNK_SIZE];
    strm.avail_out = Z_CHUNK_SIZE;
    strm.next_out = ubuf;

    ret = inflate(&strm, Z_FINISH);
    ut_a(ret == Z_STREAM_END);

    ulint chunk_size = strm.total_out;
    ut_a(chunk_size == data_len);
    ut_a(offset < chunk_size);

    byte *ptr = ubuf + offset;
    ulint remain = chunk_size - offset;
    to_copy = (len > remain) ? remain : len;
    memcpy(buf, ptr, to_copy);
  }

  len -= to_copy;
  buf += to_copy;
  inflateEnd(&strm);

  return (to_copy);
}

/** Read one zlib stream fully, given its index entry.
@param[in]      index      The index dictionary object.
@param[in]      entry      The index entry (memory copy).
@param[in,out]  zbuf       The output buffer
@param[in]      zbuf_size  The size of the output buffer.
@param[in,out]  mtr        Mini-transaction.
@return the size of the zlib stream.*/
ulint z_read_strm(dict_index_t *index, z_index_entry_t &entry, byte *zbuf,
                  ulint zbuf_size, mtr_t *mtr) {
  page_no_t page_no = entry.get_z_page_no();
  byte *ptr = zbuf;
  ulint remain = zbuf_size;

  while (remain > 0 && page_no != FIL_NULL) {
    buf_block_t *block = buf_page_get(
        page_id_t(dict_index_get_space(index), page_no),
        dict_table_page_size(index->table), RW_X_LATCH, UT_LOCATION_HERE, mtr);

    page_type_t ptype = block->get_page_type();
    byte *data = nullptr;
    ulint data_size = 0;
    if (ptype == FIL_PAGE_TYPE_ZLOB_FRAG) {
      frag_id_t fid = entry.get_z_frag_id();
      z_frag_page_t frag_page(block, mtr, index);
      frag_node_t node = frag_page.get_frag_node(fid);
      data = node.data_begin();
      data_size = node.payload();
    } else if (ptype == FIL_PAGE_TYPE_ZLOB_FIRST) {
      z_first_page_t first(block, mtr, index);
      data = first.begin_data_ptr();
      data_size = first.get_data_len();
    } else {
      ut_a(ptype == FIL_PAGE_TYPE_ZLOB_DATA);
      z_data_page_t data_page(block, mtr, index);
      data = data_page.begin_data_ptr();
      data_size = data_page.get_data_len();
      ut_a(data_size <= data_page.payload());
    }
    memcpy(ptr, data, data_size);
    ptr += data_size;
    ut_a(data_size <= remain);
    remain -= data_size;
    page_no = block->get_next_page_no();
  }

  ulint zbytes = (zbuf_size - remain);
  return (zbytes);
}

#ifdef UNIV_DEBUG
static bool z_validate_strm_low(dict_index_t *index, z_index_entry_t &entry,
                                mtr_t *mtr) {
  /* Expected length of compressed data. */
  const ulint exp_zlen = entry.get_zdata_len();
  page_no_t page_no = entry.get_z_page_no();
  ulint remain = exp_zlen;

  while (remain > 0 && page_no != FIL_NULL) {
    buf_block_t *block = buf_page_get(
        page_id_t(dict_index_get_space(index), page_no),
        dict_table_page_size(index->table), RW_X_LATCH, UT_LOCATION_HERE, mtr);

    page_type_t ptype = block->get_page_type();
    ulint data_size = 0;
    if (ptype == FIL_PAGE_TYPE_ZLOB_FRAG) {
      frag_id_t fid = entry.get_z_frag_id();
      z_frag_page_t frag_page(block, mtr, index);
      frag_node_t node = frag_page.get_frag_node(fid);
      data_size = node.payload();
    } else if (ptype == FIL_PAGE_TYPE_ZLOB_FIRST) {
      z_first_page_t first(block, mtr, index);
      data_size = first.get_data_len();
    } else {
      ut_a(ptype == FIL_PAGE_TYPE_ZLOB_DATA);
      z_data_page_t data_page(block, mtr, index);
      data_size = data_page.get_data_len();
      ut_a(data_size <= data_page.payload());
    }
    ut_a(data_size <= remain);
    remain -= data_size;
    page_no = block->get_next_page_no();
  }

  ut_ad(remain == 0);
  return (true);
}

bool z_validate_strm(dict_index_t *index, z_index_entry_t &entry, mtr_t *mtr) {
  static const uint32_t FREQ = 50;
  static std::atomic<uint32_t> n{0};
  bool ret = true;
  if (++n % FREQ == 0) {
    ret = z_validate_strm_low(index, entry, mtr);
  }
  return (ret);
}
#endif /* UNIV_DEBUG */

}  // namespace lob
