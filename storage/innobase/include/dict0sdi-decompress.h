/*****************************************************************************

Copyright (c) 2017, 2024, Oracle and/or its affiliates.

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

#ifndef DICT_SDI_DECOMPRESS_H
#define DICT_SDI_DECOMPRESS_H

#include <zlib.h>

/** Decompress SDI record */
class Sdi_Decompressor {
 public:
  Sdi_Decompressor(byte *uncomp_sdi, uint32_t uncomp_len, byte *comp_sdi,
                   uint32_t comp_len)
      : m_uncomp_sdi(uncomp_sdi),
        m_uncomp_len(uncomp_len),
        m_comp_sdi(comp_sdi),
        m_comp_len(comp_len) {
    ut_ad(m_uncomp_sdi != nullptr);
    ut_ad(m_comp_sdi != nullptr);
  }

  ~Sdi_Decompressor() = default;

  /** Decompress the SDI and store in the buffer passed. */
  inline void decompress() {
    int ret;
    uLongf dest_len = m_uncomp_len;
    ret = uncompress(m_uncomp_sdi, &dest_len, m_comp_sdi, m_comp_len);

    if (ret != Z_OK) {
#ifdef UNIV_NO_ERR_MSGS
      ib::error()
#else
      ib::error(ER_IB_ERR_ZLIB_UNCOMPRESS_FAILED)
#endif
          << "ZLIB uncompress() failed:"
          << " compressed len: " << m_comp_len
          << ", original_len: " << m_uncomp_len;

      switch (ret) {
        case Z_BUF_ERROR:
#ifdef UNIV_NO_ERR_MSGS
          ib::fatal(UT_LOCATION_HERE)
#else
          ib::fatal(UT_LOCATION_HERE, ER_IB_ERR_ZLIB_BUF_ERROR)
#endif

              << "retval = Z_BUF_ERROR";
          break;

        case Z_MEM_ERROR:
#ifdef UNIV_NO_ERR_MSGS
          ib::fatal(UT_LOCATION_HERE)
#else
          ib::fatal(UT_LOCATION_HERE, ER_IB_ERR_ZLIB_MEM_ERROR)
#endif
              << "retval = Z_MEM_ERROR";
          break;

        case Z_DATA_ERROR:
#ifdef UNIV_NO_ERR_MSGS
          ib::fatal(UT_LOCATION_HERE)
#else
          ib::fatal(UT_LOCATION_HERE, ER_IB_ERR_ZLIB_DATA_ERROR)
#endif
              << "retval = Z_DATA_ERROR";
          break;

        default:
#ifdef UNIV_NO_ERR_MSGS
          ib::fatal(UT_LOCATION_HERE)
#else
          ib::fatal(UT_LOCATION_HERE, ER_IB_ERR_ZLIB_UNKNOWN_ERROR)
#endif
              << "retval = UNKNOWN_ERROR";
          break;
      }
    }
  }

  /** @return the uncompressed sdi */
  byte *get_data() const { return (m_uncomp_sdi); }

 private:
  /** Buffer to hold uncompressed SDI. memory allocated by caller */
  byte *m_uncomp_sdi;
  /** Length of Outbuf Buffer */
  uint32_t m_uncomp_len;
  /** Input Compressed SDI */
  byte *m_comp_sdi;
  /** Length of Compressed SDI */
  uint32_t m_comp_len;
};
#endif
