/*****************************************************************************

Copyright (c) 2017, 2023, Oracle and/or its affiliates.

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

#ifndef DICT_SDI_H
#define DICT_SDI_H

#include <zconf.h>
#include <zlib.h>

/** Size of sdi_key_t::type */
constexpr const uint32_t SDI_TYPE_LEN = 4;

/** Size of sdi_key_t::id */
constexpr const uint32_t SDI_KEY_LEN = 8;

/** Compress SDI using zlib */
class Sdi_Compressor {
 public:
  Sdi_Compressor(uint32_t src_len, const void *sdi)
      : m_src_len(src_len), m_comp_len(), m_sdi(sdi), m_comp_sdi() {}

  ~Sdi_Compressor() { ut::free(m_comp_sdi); }

  /** Compress the SDI */
  inline void compress() {
    uLongf zlen = compressBound(static_cast<uLong>(m_src_len));
    auto src = reinterpret_cast<const Bytef *>(m_sdi);

    m_comp_sdi =
        static_cast<byte *>(ut::malloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, zlen));
    ut_ad(m_comp_sdi != nullptr);

    switch (
        compress2(m_comp_sdi, &zlen, src, static_cast<uLong>(m_src_len), 6)) {
      case Z_BUF_ERROR:
        ib::fatal(UT_LOCATION_HERE, ER_IB_MSG_FAILED_SDI_Z_BUF_ERROR);
        break;

      case Z_MEM_ERROR:
        ib::fatal(UT_LOCATION_HERE, ER_IB_MSG_FAILED_SDI_Z_MEM_ERROR);
        break;

      case Z_STREAM_ERROR:
        ib::fatal(UT_LOCATION_HERE, ER_IB_MSG_SDI_Z_STREAM_ERROR);
        break;

      case Z_OK:
        m_comp_len = zlen;
        break;

      default:
        ib::fatal(UT_LOCATION_HERE, ER_IB_MSG_SDI_Z_UNKNOWN_ERROR);
        break;
    }
  }

  /** @return compressed SDI record */
  const byte *get_data() const { return (m_comp_sdi); }

  /** @return length of uncompressed SDI */
  uint32_t get_src_len() const { return (m_src_len); }

  /** @return length of compressed SDI */
  uint32_t get_comp_len() const { return (m_comp_len); }

 private:
  /** Length of uncompressed SDI */
  uint32_t m_src_len;
  /** Length of compressed SDI */
  uint32_t m_comp_len;
  /** Uncompressed SDI */
  const void *m_sdi;
  /** Compressed SDI */
  byte *m_comp_sdi;
};

/** Create SDI in a tablespace. This API should be used when
upgrading a tablespace with no SDI.
@param[in,out]  tablespace      tablespace object
@retval         false           success
@retval         true            failure */
bool dict_sdi_create(dd::Tablespace *tablespace);

/** Drop SDI in a tablespace. This API should be used only
when SDI is corrupted.
@param[in,out]  tablespace      tablespace object
@retval         false           success
@retval         true            failure */
bool dict_sdi_drop(dd::Tablespace *tablespace);

/** Get the SDI keys in a tablespace into the vector provided.
@param[in]      tablespace      tablespace object
@param[in,out]  vector          vector to hold SDI keys
@retval         false           success
@retval         true            failure */
bool dict_sdi_get_keys(const dd::Tablespace &tablespace, sdi_vector_t &vector);

/** Retrieve SDI from tablespace.
@param[in]      tablespace      tablespace object
@param[in]      sdi_key         SDI key
@param[in,out]  sdi             SDI retrieved from tablespace
@param[in,out]  sdi_len         in:  size of memory allocated
                                out: actual length of SDI
@retval         false           success
@retval         true            in case of failures like record not found,
                                sdi_len is UINT64MAX_T, else sdi_len is
                                actual length of SDI */
bool dict_sdi_get(const dd::Tablespace &tablespace, const sdi_key_t *sdi_key,
                  void *sdi, uint64_t *sdi_len);

/** Insert/Update SDI in tablespace.
@param[in]      hton            handlerton object
@param[in]      tablespace      tablespace object
@param[in]      table           table object
@param[in]      sdi_key         SDI key to uniquely identify the tablespace
object
@param[in]      sdi             SDI to be stored in tablespace
@param[in]      sdi_len         SDI length
@retval         false           success
@retval         true            failure */
bool dict_sdi_set(handlerton *hton, const dd::Tablespace &tablespace,
                  const dd::Table *table, const sdi_key_t *sdi_key,
                  const void *sdi, uint64_t sdi_len);

/** Delete SDI from tablespace.
@param[in]      tablespace      tablespace object
@param[in]      table           table object
@param[in]      sdi_key         SDI key to uniquely identify the tablespace
                                object
@retval         false           success
@retval         true            failure */
bool dict_sdi_delete(const dd::Tablespace &tablespace, const dd::Table *table,
                     const sdi_key_t *sdi_key);
#endif
