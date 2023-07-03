/* Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "util/ndbxfrm_file.h"
#include "portlib/ndb_file.h"
#include "util/ndb_az31.h"
#include "util/ndb_math.h"
#include "util/ndb_ndbxfrm1.h"
#include "util/ndb_openssl_evp.h"
#include "util/ndbxfrm_iterator.h"
#include <algorithm>
#include <math.h>

// clang-format off
#ifndef REQUIRE
#define REQUIRE(r) do { if (unlikely(!(r))) { fprintf(stderr, "\nYYY: %s: %u: %s: r = %d\n", __FILE__, __LINE__, __func__, (r)); require((r)); } } while (0)
#endif
//#define RETURN(r) do { REQUIRE((r) >= 0); return (r); } while (0)
#define RETURN(r) return (r)
// clang-format on

/*
 * Made for multi threaded concurrent read and write of whole pages (assume not
 * two concurrent ops on the same page).
 *
 * No internal buffering.
 *
 * Files need fixed size from start to be able to write trailer (if encrypted).
 *
 * If encrypted: data_pos zero is second file page, first file page is header
 * If not encrypted data_pos and file_pos are same.
 *
 * No file checksum since that would need updating trailer which would be a
 * synchronization point (maybe keep composable checksum in memory and update
 * checksum at file close? )
 *
 * Since class has no buffering, all i/o is transformed, and transformation
 * must be explicit not transparent as for ndbxfrm_{write, read}file.
 *
 * Also to allow writing multiple consecutive pages, buffer is outside class
 * and transformation must be done buffer to buffer rather when transparent
 * doing i/o.
 */

ndbxfrm_file::ndbxfrm_file()
    : m_file(nullptr),
      m_file_block_size(0),
      m_payload_start(-1),
      m_encrypted(false),
      m_file_format(FF_UNKNOWN),
      m_payload_end(INDEFINITE_OFFSET),
      m_file_pos(INDEFINITE_OFFSET),
      m_data_size(0),
      openssl_evp_op(&openssl_evp)
{
  openssl_evp.set_memory(m_encryption_keys, sizeof(m_encryption_keys));
}

void ndbxfrm_file::reset()
{
  m_file = nullptr;
  m_file_block_size = 0;
  m_payload_start = 0;
  m_append = false;
  m_encrypted = false;
  m_compressed = false;
  m_have_data_crc32 = false;
  openssl_evp.reset();
  m_file_format = FF_UNKNOWN;
  memset(m_encryption_keys, 0, sizeof(m_encryption_keys));
  m_data_block_size = 0;
  m_data_crc32 = 0;
  m_payload_end = INDEFINITE_OFFSET;
  m_file_pos = -1;
  m_data_size = 0;
  m_file_size = 0;
  openssl_evp_op.reset();
  zlib.reset();
  m_crc32 = 0;
  m_decrypted_buffer.init();
  m_file_buffer.init();
  m_data_pos = 0;
}

bool ndbxfrm_file::is_open() const { return m_file_format != FF_UNKNOWN; }

int ndbxfrm_file::open(ndb_file &file, const byte *pwd_key, size_t pwd_key_len)
{
   ndb_ndbxfrm1::header header;
   ndb_ndbxfrm1::trailer trailer;
   int ret = open(file, pwd_key, pwd_key_len, header, trailer);
   return (ret<0) ? -1 : ret;
}


int ndbxfrm_file::read_header_and_trailer(ndb_file &file,
                                          ndb_ndbxfrm1::header& header,
                                          ndb_ndbxfrm1::trailer& trailer)
{
  int ret = open(file, nullptr, 0, header, trailer);
  if(ret==0)
  {
    close(true);
  }
  /*
   * if ret==-2 header and trailer were read successfully so return 0.
   */
  return (ret==-1) ? -1 : 0;
}

int ndbxfrm_file::open(ndb_file &file, const byte *pwd_key,
                       size_t pwd_key_len, ndb_ndbxfrm1::header& header,
                       ndb_ndbxfrm1::trailer& trailer)
{
  reset();
  // file fixed properties
  m_file = &file;
  // TO BE READ
  m_file_block_size = 0;
  m_payload_start = 0;
  m_encrypted = false;
  m_compressed = false;
  m_have_data_crc32 = false;
  m_file_format = FF_UNKNOWN;
  // m_encryption_keys
  m_data_block_size = 0;
  m_data_crc32 = 0;

  // file status
  m_payload_end = INDEFINITE_OFFSET;
  m_file_pos = 0;
  m_data_size = 0;
  m_file_size = 0;

  // operation per file properties
  m_file_op = OP_NONE;
  m_crc32 = 0;
  // zlib
  m_decrypted_buffer.init();
  m_file_buffer.init();
  m_data_pos = 0;

  int rv;

  // Read file header
  {
    ndbxfrm_output_iterator out = m_file_buffer.get_output_iterator();
    rv = m_file->read_forward(out.begin(), out.size());
    if (rv == -1)
    {
      RETURN(-1);
    }
    m_file_pos = rv;
    if (size_t(rv) < out.size()) out.set_last();
    out.advance(rv);
    m_file_buffer.update_write(out);
  }
  size_t trailer_max_size;
  int rh;
  {
    ndbxfrm_input_iterator in = m_file_buffer.get_input_iterator();
    rh = read_header(&in, pwd_key, pwd_key_len, &trailer_max_size, 
                         header);
    if(rh==-1) return rh;
    //if rh == -2 does nothing for now. It could be ok if
    //we just wont to get header and trailer without decrypting the file
    
    m_file_buffer.update_read(in);
    m_file_buffer.rebase(m_file_block_size);
  }

  // Read file trailer, which will provide file size and data size.
  {
    size_t trailer_need =
        (m_file_block_size > 0
             ? (ndb_ceil_div(trailer_max_size, m_file_block_size) *
                m_file_block_size)
             : trailer_max_size);
    ndb_off_t file_size = m_file->get_size();
    if (file_size == -1) return -1;
    if ((ndb_off_t)trailer_need > file_size) trailer_need = file_size;

    ndb_off_t old_pos = m_file->get_pos();
    if (old_pos == -1) RETURN(-1);
    if (m_file->set_pos(0) == -1) RETURN(-1);
    require(m_file->set_pos(file_size - trailer_need) != -1);
    {
      m_file_size = file_size;
      require(trailer_need <= ndbxfrm_buffer::size());
      byte page[ndbxfrm_buffer::size()];
      ndbxfrm_output_iterator out = {
          page, page + ndbxfrm_buffer::size(), false};
      int rv =
          m_file->read_pos(out.begin(), out.size(), m_file_size - trailer_need);
      if (rv == -1)
      {
        RETURN(-1);
      }
      require(size_t(rv) == trailer_need);
      if (size_t(rv) < out.size()) out.set_last();
      out.advance(rv);

      ndbxfrm_input_reverse_iterator rin = {out.begin(), page, out.last()};

      int rt = read_trailer(&rin, trailer);
      if (rt == -1) return rt;
      if (rt != 0)
      {
        m_file_format = FF_RAW;
        m_compressed = m_encrypted = false;
        m_file_block_size = 0;
        ndb_off_t file_size = m_file->get_size();
        if (file_size == -1) return -1;
        m_data_size = m_payload_end = m_file_size = file_size;
        m_payload_start = 0;
        m_data_pos = 0;
      }
    }
    require(m_file->set_pos(old_pos) != -1);
    require(is_definite_offset(m_payload_end));
    require(m_payload_end >= m_payload_start);
    m_file_pos = old_pos;
    if (m_file_pos > m_payload_end)
    {
      ndbxfrm_input_iterator in = m_file_buffer.get_input_iterator();
      in.reduce(m_file_pos - m_payload_end);
      in.set_last();
      m_file_buffer.update_read(in);
      m_file_buffer.rebase(m_file_block_size);
      m_file_pos = m_payload_end;
    }
  }

  require(is_open());
  m_data_pos = 0;
  if(rh==-2)
  {
    close(true);
  }
  return rh;
}


int ndbxfrm_file::create(
    ndb_file &file,
    bool compress,
    const byte *pwd_key,
    size_t pwd_key_len,
    int kdf_iter_count,  // 0 - pwd_key is a key, using AESKW,
                         // >0 - pwd_key is password, using PBKDF2
                         // -1 - using PBKDF2 and default iteration count
    int key_cipher,      // 0 - none, 1 - cbc, 2 - xts (always no padding)
    int key_count,
    size_t key_data_unit_size,  //
    size_t file_block_size,   // typ. 32KiB phys (or logical?)
    Uint64 data_size,         // file size excluding file header and trailer
    bool is_data_size_estimated)
{
  reset();

  m_data_block_size = 0;
  m_data_size = 0;
  m_file_op = OP_NONE;
  m_append = false;
  m_is_estimated_data_size = is_data_size_estimated;
  m_estimated_data_size = data_size;

  m_file = &file;

  m_file_buffer.init();
  m_decrypted_buffer.init();  // Only needed for encrypted

  m_compressed = compress;
  m_encrypted = (pwd_key != nullptr);

  size_t data_page_size = key_data_unit_size ? file_block_size : 0;
  if (m_encrypted)
  {
    m_file_format = FF_NDBXFRM1;
  }
  else if (m_compressed)
  {
    m_file_format = FF_AZ31;
  }
  else
  {
    m_file_format = FF_RAW;
  }

  m_file_block_size = file_block_size;
  if (is_definite_size(data_size) && !is_data_size_estimated) 
    m_data_size = data_size;
  if (pwd_key)
  {
    if (kdf_iter_count == -1)
      kdf_iter_count = ndb_openssl_evp::DEFAULT_KDF_ITER_COUNT;
  }
  else
  {
    // no encryption - clear all encryption specifics?
  }
  ndbxfrm_output_iterator out = m_file_buffer.get_output_iterator();
  const byte *out_begin = out.begin();
  int r = write_header(&out,
                       data_page_size,
                       pwd_key,
                       pwd_key_len,
                       kdf_iter_count,
                       key_cipher,
                       key_count,
                       key_data_unit_size);
  if (r != 0) return r;
  m_payload_start = out.begin() - out_begin;
  m_file_buffer.update_write(out);

  if (!is_definite_size(data_size) || m_is_estimated_data_size)
  {
    // File created with no data, and later appended until closed.
    m_file_size = INDEFINITE_SIZE;
    m_payload_end = INDEFINITE_OFFSET;
  }
  else if (m_file_format == FF_RAW)
  {
    m_payload_end = m_file_size = data_size;
  }
  else
  {
    /*
     * Files created with a fixed size are also implied to use block access
     * mode.  Since neither compression nor CBC-mode encryption support
     * that file is encrypted using XTS.
     */
    require(data_page_size == m_file_block_size);

    ndbxfrm_input_iterator in = m_file_buffer.get_input_iterator();
    require((ndb_off_t)in.size() == m_payload_start);
    int n = m_file->write_pos(in.cbegin(), in.size(), 0);
    if (n != (ndb_off_t)in.size()) return -1;
    in.advance(n);
    m_file_buffer.update_read(in);
    m_file_size = m_payload_start + data_size + data_page_size;
    require(m_file->set_pos(m_payload_start) == 0);

    if (m_file_block_size > 0)
    {
      require(m_payload_start % m_file_block_size == 0);
      require(m_file_size % m_file_block_size == 0);
    }
    if (data_page_size > 0)
    {
      require(m_payload_start % data_page_size == 0);
      require(m_file_size % data_page_size == 0);
    }
  }

  if (has_definite_file_size())
  {
    m_file->extend(m_file_size, ndb_file::NO_FILL);
    m_payload_end = m_payload_start + data_size;
  }

  if (m_file_format != FF_RAW && is_definite_offset(m_payload_end))
  {
    byte page_[BUFFER_SIZE + NDB_O_DIRECT_WRITE_ALIGNMENT];
    byte *page = page_ + (NDB_O_DIRECT_WRITE_ALIGNMENT -
                          ((uintptr_t)page_) % NDB_O_DIRECT_WRITE_ALIGNMENT);
    ndbxfrm_output_iterator out = {page, page + BUFFER_SIZE, false};
    const byte *out_begin = out.begin();
    /*
     * File is implied to use block mode access. The last file block will
     * always contain the full trailer and no extra buffer is needed.
     */
    int r = write_trailer(&out, nullptr);
    if (r != 0) RETURN(r);

    int n = m_file->write_pos(page, out.begin() - out_begin, m_payload_end);
    require(n == (out.begin() - out_begin));
    m_file->set_pos(m_payload_start);
    ndb_off_t file_size = m_file->get_size();
    if (file_size == -1) return -1;
    require(ndb_off_t(m_data_size) < file_size);
  }
  require(r == 0);
  return r;
}

int ndbxfrm_file::flush_payload()
{
  if (m_file_buffer.last())
  {  // All should already be compressed and encrypted
     // as needed.
    require(m_decrypted_buffer.read_size() == 0);
    if (m_file_buffer.read_size() == 0)
      return 0;  // Nothing more to write to file.
  }
  else if (!m_encrypted || !m_decrypted_buffer.last())
  {
    // Mark that there will be no more payload.
    ndbxfrm_input_iterator in(nullptr, nullptr, true);
    int r = write_forward(&in);
    if (r == -1) return -1;
    require(m_decrypted_buffer.read_size() == 0);
  }

  m_file_op = OP_WRITE_FORW;
  return 0;
}

int ndbxfrm_file::close(bool abort)
{
  /*
   * If abort flag is set pending data need not be written since file likely
   * will be discarded.
   */

  if (!is_open())
  {
    RETURN(-1);
  }
  if (m_file_op == OP_WRITE_FORW)
    if (!abort)
    {
      if (flush_payload() == -1)
      {
        RETURN(-1);
      }
    }

  bool was_compressed = m_compressed;
  if (m_encrypted)
  {
    if (m_file_op == OP_WRITE_FORW && m_append)
      require(openssl_evp_op.encrypt_end() == 0 || abort);
    else if (m_file_op == OP_READ_FORW || m_file_op == OP_READ_BACKW)
      require(openssl_evp_op.decrypt_end() == 0 || abort);
    else
      require(m_file_op == OP_NONE);

    openssl_evp.reset();
  }

  if (m_compressed)
  {
    if (m_file_op == OP_WRITE_FORW && m_append)
      require(zlib.deflate_end() == 0 || abort);
    else if (m_file_op == OP_READ_FORW)
    {
      require(zlib.inflate_end() == 0 || abort);
    }
    else
      require(m_file_op == OP_NONE);
  }

  if (m_file_op == OP_WRITE_FORW)
  {
    // Extra buffer for write trailer that may touch two blocks
    byte extra_page_[ndbxfrm_buffer::size() + NDB_O_DIRECT_WRITE_ALIGNMENT];
    byte *extra_page =
        extra_page_ + (NDB_O_DIRECT_WRITE_ALIGNMENT -
                       ((uintptr_t)extra_page_) % NDB_O_DIRECT_WRITE_ALIGNMENT);
    ndbxfrm_output_iterator extra = {
        extra_page, extra_page + ndbxfrm_buffer::size(), false};

    if (!abort && m_file_format != FF_RAW)
    {
      // Allow trailer to be written into buffer.
      m_file_buffer.clear_last();

      ndbxfrm_output_iterator out = m_file_buffer.get_output_iterator();
      int r = write_trailer(&out, &extra);
      if (!was_compressed)
      {
        ndb_off_t file_size = m_file->get_size();
        require(ndb_off_t(m_data_size) <= file_size + ndb_off_t{BUFFER_SIZE});
      }
      if (r == -1)
      {
        RETURN(-1);
      }
      m_file_buffer.update_write(out);
      m_file_format = FF_RAW;
    }

    if (!abort)
    {
      ndbxfrm_input_iterator in = m_file_buffer.get_input_iterator();
      while (in.size() > 0)
      {
        int n = m_file->append(in.cbegin(), in.size());
        if (n == -1)
        {
          RETURN(-1);
        }
        if (n == 0)
        {
          RETURN(-1);
        }
        in.advance(n);
      }
      require(in.empty());
      m_file_buffer.update_read(in);
      m_file_buffer.rebase(m_file_block_size);

      if (extra_page != extra.begin())
      {
        size_t len = extra.begin() - extra_page;
        int n = m_file->append(extra_page, len);
        if (n == -1)
        {
          RETURN(-1);
        }
        if (n < 0 || size_t(n) != len)
        {
          RETURN(-1);
        }
      }
    }
  }
  else if (!abort && m_file_op == OP_READ_FORW)
  {
    if (m_data_pos != m_data_size)
    {
      // Whole file was not consumed
      return -1;
    }
    if (m_have_data_crc32 && m_data_crc32 != m_crc32)
    {
      return -1;
    }
  }
  m_encrypted = false;
  m_compressed = false;
  m_file_op = OP_NONE;
  m_file_format = FF_UNKNOWN;
  return 0;
}

int ndbxfrm_file::transform_pages(ndb_openssl_evp::operation *op,
                                  ndb_off_t data_pos,
                                  ndbxfrm_output_iterator *out,
                                  ndbxfrm_input_iterator *in)
{
  if (!m_encrypted && !m_compressed)
  {
    return out->copy_from(in);
  }

  require(m_encrypted);
  require(!m_compressed);

  if (op == nullptr)
    op = &openssl_evp_op;
  else if (op->set_context(&openssl_evp) == -1)
  {
    return -1;
  }

  if (op->encrypt_init(data_pos, data_pos) == -1)
  {
    return -1;
  }
  if (op->encrypt(out, in) == -1)
  {
    return -1;
  }
  if (op->encrypt_end() == -1)
  {
    return -1;
  }
  if (!in->empty())
  {
    // All input is not transformed, function must be called again.
    return 1;
  }
  return 0;
}

int ndbxfrm_file::untransform_pages(ndb_openssl_evp::operation *op,
                                    ndb_off_t data_pos,
                                    ndbxfrm_output_iterator *out,
                                    ndbxfrm_input_iterator *in)
{
  if (!m_encrypted && !m_compressed)
  {
    return out->copy_from(in);
  }

  require(m_encrypted);
  require(!m_compressed);

  if (op == nullptr)
    op = &openssl_evp_op;
  else if (op->set_context(&openssl_evp) == -1)
  {
    return -1;
  }

  if (op->decrypt_init(data_pos, data_pos) == -1)
  {
    return -1;
  }
  if (op->decrypt(out, in) == -1)
  {
    return -1;
  }
  if (op->decrypt_end() == -1)
  {
    return -1;
  }
  if (!in->empty())
  {
    return 1;
  }
  return 0;
}

int ndbxfrm_file::read_header(ndbxfrm_input_iterator *in,
                              const byte *pwd_key,
                              size_t pwd_key_len,
                              size_t *trailer_max_size,
                              ndb_ndbxfrm1::header& ndbxfrm_header)
{
  bool unwrap_keys_failed = false;
  const byte *in_begin = in->cbegin();
  size_t header_size = 0;
  if (ndb_az31::detect_header(in) == 0)
  {
    int rv = ndb_az31::read_header(in);
    if (rv == -1)
    {
      RETURN(-1);
    }
    if (rv == 1)
    {
      RETURN(-1);
    }
    header_size = 512;
    m_file_block_size = 512;
    m_file_format = FF_AZ31;
    m_compressed = true;

    /*
     * Set compression flag in ndbxfrm_header object.
     * From ndxrfrm tool we need to open the file just to read header and
     * trailer. In that case we have to propagate the compression flag
     * to the caller. Since, currently, the only way caller have to access
     * that info is through the ndb_ndbxfrm1::header reference
     * we are writing there the compression flag.
     */
    ndbxfrm_header.set_compression_method(ndb_ndbxfrm1::compression_deflate);
    m_encrypted = false;
    *trailer_max_size = 12 + 511;
  }
  else if (int ret = ndb_ndbxfrm1::header::detect_header(in, &header_size);
           (ret == -1) || (ret == 0))
  {
    m_file_format = FF_NDBXFRM1;
    if (ret == -1)
    {
      // File magic was found, but other parts of header was bad.
      RETURN(-1);
    }
    if (header_size > in->size())
    {
      RETURN(-1);
    }
    int rv = ndbxfrm_header.read_header(in);
    if (rv == -1)
    {
      RETURN(-1);
    }
    if (rv == 1)
    {
      RETURN(-1);
    }
    ndbxfrm_header.get_file_block_size(&m_file_block_size);
    ndbxfrm_header.get_trailer_max_size(trailer_max_size);
    m_compressed = (ndbxfrm_header.get_compression_method() != 0);
    int compress_padding = 0;
    if (m_compressed)
    {
      compress_padding = ndbxfrm_header.get_compression_padding();
      switch (compress_padding)
      {
        case 0: /* no padding */
          break;
        case ndb_ndbxfrm1::padding_pkcs:
          require(zlib.set_pkcs_padding() == 0);
          break;
        default:
          RETURN(-1);
      }
    }

    Uint32 cipher = 0;
    ndbxfrm_header.get_encryption_cipher(&cipher);
    m_encrypted = (cipher != 0);
    Uint32 enc_data_unit_size = 0;
    if (m_encrypted)
    {
      Uint32 padding = 0;
      Uint32 krm = 0;
      Uint32 kdf_iter_count = 0;
      Uint32 key_selection_mode = 0;
      byte
          keying_material[ndb_ndbxfrm1::header::get_max_keying_material_size()];
      size_t keying_material_size = 0;
      size_t keying_material_count = 0;

      require(ndbxfrm_header.get_encryption_padding(&padding) == 0);
      require(ndbxfrm_header.get_encryption_krm(&krm) == 0);
      require(ndbxfrm_header.get_encryption_krm_kdf_iter_count(&kdf_iter_count) == 0);
      require(ndbxfrm_header.get_encryption_key_selection_mode(
                  &key_selection_mode, &enc_data_unit_size) == 0);
      require(ndbxfrm_header.get_encryption_keying_material(keying_material,
                                                     sizeof(keying_material),
                                                     &keying_material_size,
                                                     &keying_material_count) ==
              0);
      if (cipher != ndb_ndbxfrm1::cipher_cbc && cipher != ndb_ndbxfrm1::cipher_xts)
        RETURN(-1);
      if (!(padding == 0 || padding == ndb_ndbxfrm1::padding_pkcs)) RETURN(-1);
      if (krm != ndb_ndbxfrm1::krm_pbkdf2_sha256 &&
          krm != ndb_ndbxfrm1::krm_aeskw_256)
        RETURN(-1);
      if (key_selection_mode > 2) RETURN(-1);
      if (krm == ndb_ndbxfrm1::krm_pbkdf2_sha256)
      {
        if (keying_material_size != ndb_openssl_evp::SALT_LEN ||
            keying_material_count == 0)
        {
          RETURN(-1);
        }
      }
      else if (krm == ndb_ndbxfrm1::krm_aeskw_256)
      {
        if (keying_material_count != 1)
        {
          RETURN(-1);
        }
      }
      if (krm == ndb_ndbxfrm1::krm_pbkdf2_sha256)
      {
        if (kdf_iter_count == 0) RETURN(-1);
      }
      else if (krm == ndb_ndbxfrm1::krm_aeskw_256)
      {
        if (kdf_iter_count != 0) RETURN(-1);
      }

      openssl_evp.reset();
      switch (cipher)
      {
        case ndb_ndbxfrm1::cipher_cbc:
          require(openssl_evp.set_aes_256_cbc((padding == ndb_ndbxfrm1::padding_pkcs),
                                              enc_data_unit_size) == 0);
          break;
        case ndb_ndbxfrm1::cipher_xts:
          require(openssl_evp.set_aes_256_xts((padding == ndb_ndbxfrm1::padding_pkcs),
                                              enc_data_unit_size) == 0);
          if (m_compressed)
          {
            /*
             * XTS requires block at least 16 bytes long, uses pkcs padding on
             * compressed data to ensure that.
             */
            require(compress_padding == ndb_ndbxfrm1::padding_pkcs);
          }
          break;
        default:
          RETURN(-1);
      }
      if (pwd_key != nullptr)
      {
        if (krm == ndb_ndbxfrm1::krm_pbkdf2_sha256)
        {
          for (unsigned i = 0; i < keying_material_count; i++)
          {
            openssl_evp.derive_and_add_key_iv_pair(
                pwd_key,
                pwd_key_len,
                kdf_iter_count,
                keying_material + keying_material_size * i);
          }
        }
        else if (krm == ndb_ndbxfrm1::krm_aeskw_256)
        {
          require(keying_material_count == 1);
          require(keying_material_size <=
                  ndb_ndbxfrm1::header::get_max_keying_material_size());
          byte keys[ndb_ndbxfrm1::header::get_max_keying_material_size() -
                     ndb_openssl_evp::AESKW_EXTRA];
          size_t keys_size = sizeof(keys);
          if (openssl_evp.unwrap_keys_aeskw256(keys,
                                               &keys_size,
                                               keying_material,
                                               keying_material_size,
                                               pwd_key,
                                               pwd_key_len) != 0)
            unwrap_keys_failed = true;
          else
          {
            int key_count = (keys_size / (ndb_openssl_evp::KEY_LEN +
                                          ndb_openssl_evp::IV_LEN));
            openssl_evp.add_key_iv_pairs(
                keys,
                key_count,
                ndb_openssl_evp::KEY_LEN + ndb_openssl_evp::IV_LEN);
          }
        }
      }
    }
    if (!m_compressed && m_encrypted && enc_data_unit_size > 0)
    {
      m_data_block_size = enc_data_unit_size;
    }
  }
  else
  {
    m_file_format = FF_RAW;
    m_compressed = m_encrypted = false;
    m_file_block_size = 0;
    m_data_size = m_payload_end = m_file_size = m_file->get_size();
    m_payload_start = 0;
    m_data_pos = 0;
    *trailer_max_size = 0;
  }
  m_payload_start = in->cbegin() - in_begin;
  if (m_encrypted && pwd_key == nullptr)
  {
    // Encrypted file but no password or key given
    return -2;
  }
  return (unwrap_keys_failed ? -2 : 0);
}

int ndbxfrm_file::read_trailer(ndbxfrm_input_reverse_iterator *rin, 
                               ndb_ndbxfrm1::trailer& ndbxfrm_trailer)
{
  if (m_file_format == FF_AZ31)
  {
    const byte *in_begin = rin->cbegin();

    ndb_az31 az31;
    int r = az31.read_trailer(rin);
    if (r == -1)
    {
      RETURN(-1);
    }
    if (r == 1)
    {
      RETURN(-1);
    }
    if (az31.get_data_size(&m_data_size) != 0 ) RETURN(-1);
    if (az31.get_data_crc32(&m_data_crc32) != 0) RETURN(-1);
    m_have_data_crc32 = true;
    {
      size_t trailer_size = in_begin - rin->cbegin();
      require(trailer_size > 0);

      m_payload_end = m_file_size - trailer_size;
    }
  }
  else if (m_file_format == FF_NDBXFRM1)
  {
    const byte *in_begin = rin->cbegin();
    int rv = ndbxfrm_trailer.read_trailer(rin);
    size_t trailer_size = in_begin - rin->cbegin();
    size_t tsz;
    ndbxfrm_trailer.get_trailer_size(&tsz);
    {
      require(trailer_size > 0);

      ndb_off_t file_size = m_file->get_size();
      require((uintmax_t)file_size == m_file_size);
      m_payload_end = file_size - tsz;
    }
    if (rv == -1)
    {
      RETURN(-1);
    }
    if (rv == 1)
    {
      RETURN(-1);
    }

    Uint64 data_size = 0;
    require(ndbxfrm_trailer.get_data_size(&data_size) == 0);
    m_data_size = data_size;

    Uint32 data_crc32 = 0;
    if(ndbxfrm_trailer.get_data_crc32(&data_crc32) == 0)
    {
      m_have_data_crc32 = true;
    }
    else
    {
      m_have_data_crc32 = false;
    }
    m_data_crc32 = data_crc32;
  }
  else
  {
    require(m_file_format == FF_RAW);
    m_payload_end = m_data_size = m_file_size = m_file->get_size();
  }
  return 0;
}

int ndbxfrm_file::read_transformed_pages(ndb_off_t data_pos,
                                         ndbxfrm_output_iterator *out)
{
  if (!is_definite_offset(m_payload_end))
  {
    /*
     * This is a hack to allow reading from created zero-sized file using same
     * instance of ndbxfrm_file.
     *
     * When opening an existing file m_payload_end will always be set unlike
     * when you creates a file in append mode when m_payload_end will be
     * determined at close.
     *
     * When the Backup block read the LCP control file it will create it if it
     * does not exist and read will always succeed.
     * Instead it should handle reading failed due to no file exists and let
     * the writing logic handle the creation of file as it already do.
     */
    m_payload_end = m_data_size;
  }
  require(m_file_op == OP_NONE || m_file_op == OP_READ_FORW);
  // todo verify against payload size
  ndb_off_t file_pos = m_payload_start + data_pos;
  if (file_pos >= (ndb_off_t)m_payload_end)
  {
    require(m_payload_end >= m_payload_start);
    require(m_payload_start >= 0);
    out->set_last();
    return 0;
  }
  ndb_off_t read_end = file_pos + out->size();
  if (read_end > (ndb_off_t)m_payload_end) read_end = m_payload_end;
  size_t read_size = read_end - file_pos;
  int nb = m_file->read_pos(out->begin(), read_size, file_pos);
  if (nb == -1)
  {
    return -1;
  }
  if (nb == 0 && !out->empty())
  {
    out->set_last();
    return 0;
  }
  out->advance(nb);
  if ((size_t)nb == read_size && read_end != m_payload_end && !out->empty())
  {
    return 1;
  }
  if ((size_t)nb == read_size && read_end == m_payload_end) out->set_last();
  return 0;
}

int ndbxfrm_file::write_transformed_pages(ndb_off_t data_pos,
                                          ndbxfrm_input_iterator *in)
{
  require(m_file_op == OP_NONE || m_file_op == OP_READ_FORW);

  ndb_off_t file_pos = m_payload_start + data_pos;
  int nb = m_file->write_pos(in->cbegin(), in->size(), file_pos);
  if (nb == -1)
  {
    return -1;
  }
  in->advance(nb);
  if (!in->empty())
  {
    return -1;
  }
  return 0;
}

int ndbxfrm_file::generate_keying_material(ndb_ndbxfrm1::header *ndbxfrm1,
                                           const byte *pwd_key,
                                           size_t pwd_key_len,
                                           const int key_cipher,
                                           int key_count)
{
  require(pwd_key != nullptr);
  const Uint64 estimated_data_size =
          (m_payload_end == INDEFINITE_OFFSET) ? m_estimated_data_size : m_data_size;
  constexpr size_t max_keying_material_size =
      ndb_ndbxfrm1::header::get_max_keying_material_size();
  /**
   * estimated_data_size is an estimate of the data size that should be encrypted.
   * If not using compression that coincides with the estimate of the raw data.
   * With compression the compressed data size is not take into account so it
   * will typically be an overestimate. Thus, it will generate more keys than needed.
   * However, this way we don't run the risk of generating fewer keys than desired.
   *
   */
  size_t needed_key_iv_pair_count =
      openssl_evp.get_needed_key_iv_pair_count(estimated_data_size);
  size_t max_key_iv_pair_count = 0;

  Uint32 krm;
  require(ndbxfrm1->get_encryption_krm(&krm) == 0);

  if (key_cipher == ndb_ndbxfrm1::cipher_cbc &&
      krm == ndb_ndbxfrm1::krm_pbkdf2_sha256)
  {
    /*
     * PBKDF2 with CBC is used by backup. We need to make sure that new backup
     * files do not use more key-iv pairs than old Ndb programs can handle.
     */
    max_key_iv_pair_count = openssl_evp.get_pbkdf2_max_key_iv_pair_count(
        ndb_ndbxfrm1::header::get_legacy_max_keying_material_size());
  }
  else if (krm == ndb_ndbxfrm1::krm_pbkdf2_sha256)
  {
    max_key_iv_pair_count =
        openssl_evp.get_pbkdf2_max_key_iv_pair_count(max_keying_material_size);
  }
  else if (krm == ndb_ndbxfrm1::krm_aeskw_256)
  {
    max_key_iv_pair_count =
        openssl_evp.get_aeskw_max_key_iv_pair_count(max_keying_material_size);
  }
  if (key_count >= 0 && size_t(key_count) > max_key_iv_pair_count)
  {
    // Too many keys requested
    RETURN(-1);
  }
  if (key_count == -1)
  {
    key_count = std::min(needed_key_iv_pair_count, max_key_iv_pair_count);
  }
  byte keying_material[max_keying_material_size];

  if (krm == ndb_ndbxfrm1::krm_pbkdf2_sha256)
  {
    if (key_count <= 0) RETURN(-1);
    if (size_t(key_count) * ndb_openssl_evp::SALT_LEN >
            ndb_ndbxfrm1::header::get_max_keying_material_size()) RETURN(-1);
    Uint32 kdf_iter_count;
    if (ndbxfrm1->get_encryption_krm_kdf_iter_count(&kdf_iter_count) != 0)
      RETURN(-1);
    for (int i = 0; i < key_count; i++)
    {
      byte *salt = &keying_material[i * ndb_openssl_evp::SALT_LEN];
      openssl_evp.generate_salt256(salt);
      openssl_evp.derive_and_add_key_iv_pair(
          pwd_key, pwd_key_len, kdf_iter_count, salt);
    }
    ndbxfrm1->set_encryption_keying_material(
        keying_material, ndb_openssl_evp::SALT_LEN, key_count);
  }
  else if (krm == ndb_ndbxfrm1::krm_aeskw_256)
  {
    if (key_count <= 0) RETURN(-1);
    // generate and encrypt keys !!
    if (size_t(key_count) *
                    (ndb_openssl_evp::KEY_LEN + ndb_openssl_evp::IV_LEN) +
                ndb_openssl_evp::AESKW_EXTRA >
            ndb_ndbxfrm1::header::get_max_keying_material_size()) RETURN(-1);
    byte keys[ndb_ndbxfrm1::header::get_max_keying_material_size() -
               ndb_openssl_evp::AESKW_EXTRA];
    for (int i = 0; i < 2 * key_count; i++)
    {
      static_assert(ndb_openssl_evp::KEY_LEN == ndb_openssl_evp::IV_LEN);
      byte *key = &keys[i * ndb_openssl_evp::KEY_LEN];
      openssl_evp.generate_key(key, ndb_openssl_evp::KEY_LEN);
    }
    size_t keys_size =
        key_count * (ndb_openssl_evp::KEY_LEN + ndb_openssl_evp::IV_LEN);
    size_t keying_material_size =
        ndb_ndbxfrm1::header::get_max_keying_material_size();
    openssl_evp.add_key_iv_pairs(
        keys, key_count, ndb_openssl_evp::KEY_LEN + ndb_openssl_evp::IV_LEN);
    if (openssl_evp.wrap_keys_aeskw256(keying_material,
                                       &keying_material_size,
                                       keys,
                                       keys_size,
                                       pwd_key,
                                       pwd_key_len) == -1)
      RETURN(-1);
    ndbxfrm1->set_encryption_keying_material(
        keying_material, keying_material_size, 1);
  }
  else
    abort();  // Unknown krm

  if (key_count > 0)
  {
    Uint32 key_selection_mode;
    Uint32 key_data_unit_size = openssl_evp.get_random_access_block_size();
    if (key_count == 1)
      key_selection_mode = ndb_ndbxfrm1::key_selection_mode_same;
    else
      key_selection_mode = ndb_ndbxfrm1::key_selection_mode_mix_pair;
    ndbxfrm1->set_encryption_key_selection_mode(key_selection_mode,
                                                key_data_unit_size);
  }
  return 0;
}

int ndbxfrm_file::write_header(
    ndbxfrm_output_iterator *out,
    size_t data_page_size,
    const byte *pwd_key,
    size_t pwd_key_len,
    int kdf_iter_count,
    int key_cipher,
    int key_count,
    size_t key_data_unit_size)
{
  bool padding = (data_page_size == 0);
  // Write file header
  if (m_file_format == FF_AZ31)
  {
    require(!m_encrypted);
    require(m_compressed);
    if (m_file_block_size != 512)
    {
      RETURN(-1);
    }
    m_file_block_size = 512;  // Backward compatibility requires 512 bytes.
    static_assert(512 % NDB_O_DIRECT_WRITE_ALIGNMENT == 0);
    require(ndb_az31::write_header(out) == 0);

    m_payload_start = 512;
    m_file_size = 0;
    m_payload_end = INDEFINITE_OFFSET;
  }
  else if (m_file_format == FF_NDBXFRM1)
  {
    ndb_ndbxfrm1::header ndbxfrm1;
    ndbxfrm1.set_file_block_size(m_file_block_size);
    if (m_compressed)
    {
      ndbxfrm1.set_compression_method(ndb_ndbxfrm1::compression_deflate);
      if (key_cipher == ndb_ndbxfrm1::cipher_xts)
      {
        // XTS needs at least 16 bytes, use pkcs padding to ensure that.
        require(ndbxfrm1.set_compression_padding(ndb_ndbxfrm1::padding_pkcs) ==
                0);
        require(zlib.set_pkcs_padding() == 0);
      }
    }
    if (m_encrypted)
    {
      if (data_page_size != 0 && key_data_unit_size != 0)
      {
        if (data_page_size % key_data_unit_size != 0)
        {
          RETURN(-1);
        }
        m_data_block_size = key_data_unit_size;
      }
      else if (data_page_size != 0 || key_data_unit_size != 0)
      {
        if (data_page_size != 0)
          RETURN(-1);  // both or none should be zero TEST ndb.backup_passwords
      }
      if (key_data_unit_size != 0 && padding)
      {
        RETURN(-1);  // padding not supported (yet)
      }
      switch (key_cipher)
      {
        case ndb_ndbxfrm1::cipher_cbc:
          require(openssl_evp.set_aes_256_cbc(padding, key_data_unit_size) ==
                  0);
          ndbxfrm1.set_encryption_cipher(key_cipher);
          break;
        case ndb_ndbxfrm1::cipher_xts:
          require(openssl_evp.set_aes_256_xts(padding, key_data_unit_size) ==
                  0);
          ndbxfrm1.set_encryption_cipher(key_cipher);
          break;
        default:
          RETURN(-1);  // unsupported cipher
      }
      ndbxfrm1.set_encryption_padding(padding ? ndb_ndbxfrm1::padding_pkcs : 0);
      const int krm = (kdf_iter_count == 0) ? ndb_ndbxfrm1::krm_aeskw_256
                                            : ndb_ndbxfrm1::krm_pbkdf2_sha256;
      if (krm) ndbxfrm1.set_encryption_krm(krm);
      if (kdf_iter_count != 0)
        ndbxfrm1.set_encryption_krm_kdf_iter_count(kdf_iter_count);
      if (generate_keying_material(
              &ndbxfrm1, pwd_key, pwd_key_len, key_cipher, key_count) == -1)
        RETURN(-1);
    }
    require(ndbxfrm1.prepare_for_write(m_file_block_size) == 0);
    require(ndbxfrm1.get_size() <= out->size());
    require(ndbxfrm1.write_header(out) == 0);
  }

  return 0;
}

int ndbxfrm_file::write_trailer(ndbxfrm_output_iterator *out,
                                ndbxfrm_output_iterator *extra)
{
  require(m_file_op == OP_NONE || m_file_op == OP_WRITE_FORW);
  ndb_off_t file_pos = m_file->get_pos();
  file_pos += m_file_buffer.read_size();
  bool was_compressed = m_compressed;
  int r = -1;
  if (m_file_format == FF_AZ31)
  {
    ndb_az31 az31;
    require(az31.set_data_size(m_data_size) == 0);
    require(az31.set_data_crc32(m_crc32) == 0);

    size_t last_block_size =
        (file_pos + az31.get_trailer_size()) % m_file_block_size;
    size_t padding = (m_file_block_size - last_block_size) % m_file_block_size;
    r = az31.write_trailer(out, padding, extra);
  }
  else if (m_file_format == FF_NDBXFRM1)
  {
    ndb_ndbxfrm1::trailer ndbxfrm1;
    require(ndbxfrm1.set_data_size(m_data_size) == 0);
    if (m_file_op == OP_WRITE_FORW)
    {
      require(ndbxfrm1.set_data_crc32(m_crc32) == 0);
    }
    require(ndbxfrm1.set_file_pos(file_pos) == 0);
    require(ndbxfrm1.set_file_block_size(m_file_block_size) == 0);
    require(ndbxfrm1.prepare_for_write() == 0);
    r = ndbxfrm1.write_trailer(out, extra);
    if (!was_compressed)
    {
      ndb_off_t file_size = m_file->get_size();
      require(ndb_off_t(m_data_size) <=
                file_size + ndb_off_t{BUFFER_SIZE} * 2);
    }
  }
  else if (m_file_format == FF_RAW)
    return 0;
  if (r == -1)
  {
    return -1;
  }
  out->set_last();

  return 0;
}

int ndbxfrm_file::write_forward(ndbxfrm_input_iterator *in)
{
  require(m_file_op == OP_NONE || m_file_op == OP_WRITE_FORW);
  if (m_file_op == OP_NONE && m_data_size == 0)
  {
    if (m_encrypted)
    {
      int rv = openssl_evp_op.encrypt_init(0, 0);
      require(rv == 0);
    }
    if (m_compressed)
    {
      zlib.deflate_init();
    }
    m_append = true;
  }
  m_file_op = OP_WRITE_FORW;
  int n;
  int G = 3;
  ndbxfrm_buffer *file_bufp;
  do
  {
    const byte *in_cbegin = in->cbegin();
    file_bufp = nullptr;
    ndbxfrm_input_iterator file_in = *in;

    if (m_compressed)
    {
      ndbxfrm_buffer *compressed_buffer =
          m_encrypted ? &m_decrypted_buffer : &m_file_buffer;
      if (compressed_buffer->last())
      {
        require(in->last());
        require(in->empty());
        file_bufp = &m_file_buffer;
        file_in = file_bufp->get_input_iterator();
      }
      else
      {
        ndbxfrm_output_iterator out = compressed_buffer->get_output_iterator();
        if (out.size() < NDB_O_DIRECT_WRITE_BLOCKSIZE)
        {
          compressed_buffer->rebase(NDB_O_DIRECT_WRITE_BLOCKSIZE);
          out = compressed_buffer->get_output_iterator();
        }
        int rv = zlib.deflate(&out, in);
        if (rv == -1)
        {
          RETURN(-1);
        }
        if (!in->last()) require(!out.last());
        compressed_buffer->update_write(out);
        file_bufp = &m_file_buffer;
        file_in = file_bufp->get_input_iterator();
      }
    }
    else if (m_encrypted)
    {  // copy app data into m_decrypted_buffer
      if (m_decrypted_buffer.last())
      {
        require(in->last());
        require(in->empty());
      }
      else
      {
        ndbxfrm_output_iterator out = m_decrypted_buffer.get_output_iterator();
        if (out.size() < NDB_O_DIRECT_WRITE_BLOCKSIZE)
        {
          m_decrypted_buffer.rebase(NDB_O_DIRECT_WRITE_BLOCKSIZE);
          out = m_decrypted_buffer.get_output_iterator();
        }
        out.copy_from(in);
        require(!out.last());
        if (in->last() && in->empty()) out.set_last();

        m_decrypted_buffer.update_write(out);
      }
    }

    if (m_encrypted)
    {  // encrypt data from m_decrypted_buffer into m_file_buffer
      if (m_file_buffer.last())
      {
        require(m_decrypted_buffer.last());
        require(m_decrypted_buffer.read_size() == 0);
        file_bufp = &m_file_buffer;
        file_in = file_bufp->get_input_iterator();
      }
      else
      {
        ndbxfrm_input_iterator c_in = m_decrypted_buffer.get_input_iterator();
        ndbxfrm_output_iterator out = m_file_buffer.get_output_iterator();
        if (out.size() < NDB_O_DIRECT_WRITE_BLOCKSIZE)
        {
          m_file_buffer.rebase(NDB_O_DIRECT_WRITE_BLOCKSIZE);
          out = m_file_buffer.get_output_iterator();
        }
        if (out.size() >= m_data_block_size &&
            (c_in.size() >= m_data_block_size || c_in.last()))
        {
          int rv = openssl_evp_op.encrypt(&out, &c_in);
          if (rv == -1)
          {
            RETURN(-1);
          }
        }
        m_decrypted_buffer.update_read(c_in);
        m_decrypted_buffer.rebase(NDB_O_DIRECT_WRITE_BLOCKSIZE);
        m_file_buffer.update_write(out);
        file_bufp = &m_file_buffer;
        file_in = file_bufp->get_input_iterator();
      }
    }

    // Write to file
    size_t write_len = file_in.size();
    if (file_bufp != nullptr)
    {
      /*
       * For buffered files always append full blocks.
       * Partial last block will be written on close.
       */
      size_t block_size = std::max({size_t{m_file_block_size},
                                    size_t(m_file->get_block_size()),
                                    size_t{NDB_O_DIRECT_WRITE_BLOCKSIZE}});
      write_len -= write_len % block_size;
    }
    if (write_len > 0)
    {
      n = m_file->append(file_in.cbegin(), write_len);
    }
    else
    {
      n = 0;
    }
    if (n > 0) file_in.advance(n);
    // Fail if not all written and no buffer is used.
    if (n == -1 || (file_bufp == nullptr && !file_in.empty()))
    {
      RETURN(-1);
    }
    if (file_bufp != nullptr)
    {
      file_bufp->update_read(file_in);
      file_bufp->rebase(NDB_O_DIRECT_WRITE_BLOCKSIZE);
    }
    else
      in->advance(n);

    n = in->cbegin() - in_cbegin;
    if (n > 0) m_crc32 = crc32(m_crc32, in_cbegin, n);
    m_data_size += n;
    if (in->empty() && in->last())
    {
      require(G--);
      if ((write_len == 0 || file_in.empty()) && file_in.last())
      {
        break;
      }
    }
  } while (!in->empty() || in->last());

  if (in->last())
  {
    require(in->empty());
    require(m_decrypted_buffer.read_size() == 0);
    require(file_bufp == nullptr || m_file_buffer.last());
  }
  return 0;
}

int ndbxfrm_file::read_forward(ndbxfrm_output_iterator *out)
{
  if (m_file_op == OP_WRITE_FORW)
  {
    return -1;
  }
  if (m_data_pos == 0)
  {
    if (m_encrypted)
    {
      // Init forward read operation
      if (openssl_evp_op.decrypt_init(0, m_payload_start) == -1) RETURN(-1);
    }
    if (m_compressed)
    {
      if (zlib.inflate_init() == -1)
      {
        RETURN(-1);
      }
    }
    m_file_op = OP_READ_FORW;
  }
  if (m_file_op != OP_READ_FORW) RETURN(-1);
  require(in_file_mode());
  if (out->last()) RETURN(-1);
  byte *out_begin = out->begin();
  // copy from buffer
  if (!m_encrypted && !m_compressed &&
      (m_file_buffer.read_size() > 0 || m_file_buffer.last()))
  {
    ndbxfrm_input_iterator in = m_file_buffer.get_input_iterator();
    if (!in.empty())
    {
      out->copy_from(&in);
      m_file_buffer.update_read(in);
      m_file_buffer.rebase(m_file_block_size);
    }
    if (m_file_buffer.read_size() == 0 && m_file_buffer.last())
    {
      const size_t n = out->begin() - out_begin;
      m_crc32 = crc32(m_crc32, out_begin, n);
      m_data_pos += n;
      out->set_last();
      return 0;
    }
    if (out->empty())
    {
      const size_t n = out->begin() - out_begin;
      m_crc32 = crc32(m_crc32, out_begin, n);
      m_data_pos += n;
      return 1;
    }
  }
  bool progress;
  int G = 20;  // loop guard
  do
  {
    require(--G);
    progress = false;
    // read from file
    if (m_file_pos <= m_payload_end)
    {
      ndbxfrm_output_iterator f_out = (m_encrypted || m_compressed)
                                          ? m_file_buffer.get_output_iterator()
                                          : *out;
      if (!f_out.last())
      {
        size_t size = f_out.size();
        if (m_encrypted || m_compressed)
        {
          size_t block_size = m_file_block_size == 0 ? NDB_O_DIRECT_WRITE_BLOCKSIZE : m_file_block_size;
          size = f_out.size() / block_size * block_size;
        }
        if (m_file_pos + uintmax_t{size} > uintmax_t(m_payload_end))
        {
          size = m_payload_end - m_file_pos;
        }
        if (size > 0)
        {
          int r = m_file->read_forward(f_out.begin(), size);
          if (r == -1)
          {
            RETURN(-1);
          }
          if (m_file_pos + r >= m_payload_end)
          {
            r = m_payload_end - m_file_pos;
          }
          progress |= (r > 0);
          m_file_pos += r;
          f_out.advance(r);
        }
        if (m_file_pos == m_payload_end)
        {
          f_out.set_last();
          progress = true;
        }
        if (m_encrypted || m_compressed)
        {
          m_file_buffer.update_write(f_out);
        }
        else
        {
          *out = f_out;
        }
      }
    }
    // decrypt
    if (m_encrypted)
    {
      ndbxfrm_input_iterator f_in = m_file_buffer.get_input_iterator();
      /*
       * If we know that reads will be requested for full blocks we could have
       * directly decrypted into out, but unfortunately tools like ndb_restore
       * and ndb_print_backup_file read in small chunks which may not be
       * multiple of cipher size so need to decrypt into buffer even if no
       * compression step.
       */
      ndbxfrm_output_iterator d_out = m_decrypted_buffer.get_output_iterator();
      byte *d_out_begin = d_out.begin();
      if (!d_out.last())
      {
        int r = openssl_evp_op.decrypt(&d_out, &f_in);
        if (r == -1)
        {
          return -1;
        }
        progress |= (d_out.begin() != d_out_begin) || d_out.last();
        m_file_buffer.update_read(f_in);
        m_file_buffer.rebase(m_file_block_size);
        m_decrypted_buffer.update_write(d_out);
      }
      if (!m_compressed)
      {  // Copy out decrypted data
        ndbxfrm_input_iterator in = m_decrypted_buffer.get_input_iterator();
        if (!in.empty())
        {
          out->copy_from(&in);
          m_decrypted_buffer.update_read(in);
          m_decrypted_buffer.rebase(m_file_block_size);
        }
        if (m_decrypted_buffer.read_size() == 0 && m_decrypted_buffer.last())
        {
          out->set_last();
        }
      }
    }
    // inflate
    if (m_compressed)
    {
      ndbxfrm_input_iterator c_in =
          m_encrypted ? m_decrypted_buffer.get_input_iterator()
                      : m_file_buffer.get_input_iterator();
      byte *o_b = out->begin();
      int r = zlib.inflate(out, &c_in);
      if (r == -1)
      {
        return -1;
      }
      progress |= (o_b != out->begin()) || out->last();
      if (m_encrypted)
      {
        m_decrypted_buffer.update_read(c_in);
        m_decrypted_buffer.rebase(m_file_block_size);
      }
      else
      {
        m_file_buffer.update_read(c_in);
        m_file_buffer.rebase(m_file_block_size);
      }
    }

    if (out->last())
    {
      const size_t n = out->begin() - out_begin;
      m_crc32 = crc32(m_crc32, out_begin, n);
      m_data_pos += n;
      return 0;
    }
    if (out->empty())
    {
      const size_t n = out->begin() - out_begin;
      m_crc32 = crc32(m_crc32, out_begin, n);
      m_data_pos += n;
      return 1;
    }
  } while (progress);
  assert(progress);
  const size_t n = out->begin() - out_begin;
  m_crc32 = crc32(m_crc32, out_begin, n);
  m_data_pos += n;
  return 2;
}

int ndbxfrm_file::read_backward(ndbxfrm_output_reverse_iterator *out)
{
  m_file_op = OP_READ_BACKW;
  require(!m_compressed);
  if (out->last()) RETURN(-1);
  byte *out_begin = out->begin();
  // copy from buffer
  if (!m_encrypted &&
      (m_file_buffer.reverse_read_size() > 0 || m_file_buffer.last()))
  {
    ndbxfrm_input_reverse_iterator in =
        m_file_buffer.get_input_reverse_iterator();
    if (!in.empty())
    {
      out->copy_from(&in);
      m_file_buffer.update_reverse_read(in);
      m_file_buffer.rebase_reverse(m_file_block_size);
    }
    if (m_file_buffer.reverse_read_size() == 0 && m_file_buffer.last())
    {
      out->set_last();
      m_data_pos -= out_begin - out->begin();
      return 0;
    }
    if (out->empty())
    {
      m_data_pos -= out_begin - out->begin();
      return 1;
    }
  }
  bool progress;
  int G = 20;  // loop guard
  do
  {
    progress = false;
    require(--G);
    // read from file
    if (m_file_pos > m_payload_start)
    {
      ndbxfrm_output_reverse_iterator f_out =
          (m_encrypted || m_compressed)
              ? m_file_buffer.get_output_reverse_iterator()
              : *out;
      if (!f_out.last())
      {
        size_t size = f_out.size();
        if (m_encrypted || m_compressed)
        {
          size_t block_size =
              (m_file_block_size == 0) ? NDB_O_DIRECT_WRITE_BLOCKSIZE : m_file_block_size;
          size = f_out.size() / block_size * block_size;
        }
        if (uintmax_t(m_file_pos) < m_payload_start + uintmax_t{size})
        {
          size = m_file_pos - m_payload_start;
        }
        if (size > 0)
        {
          int r = m_file->read_backward(f_out.begin() - size, size);
          if (r == -1)
          {
            RETURN(-1);
          }
          if (m_file_pos <= m_payload_start + r)
          {
            r = m_file_pos - m_payload_start;
          }
          progress |= (r > 0);
          m_file_pos -= r;
          f_out.advance(r);
        }
        if (m_file_pos == m_payload_start)
        {
          f_out.set_last();
          progress = true;
        }
        if (m_encrypted)
        {
          m_file_buffer.update_reverse_write(f_out);
        }
        else
        {
          *out = f_out;
        }
      }
    }
    // decrypt
    if (m_encrypted)
    {
      ndbxfrm_input_reverse_iterator f_in =
          m_file_buffer.get_input_reverse_iterator();
      /*
       * If we know that reads will be requested for full blocks we could have
       * directly decrypted into out, but unfortunately tools like ndb_restore
       * and ndb_print_backup_file read in small chunks which may not be
       * multiple of cipher size so need to decrypt into buffer even if no
       * compression step.
       */
      ndbxfrm_output_reverse_iterator d_out =
          m_decrypted_buffer.get_output_reverse_iterator();
      byte *d_out_begin = d_out.begin();
      if (!d_out.last())
      {
        int r = openssl_evp_op.decrypt_reverse(&d_out, &f_in);
        if (r == -1)
        {
          return -1;
        }
        progress |= (d_out.begin() != d_out_begin) || d_out.last();
        m_file_buffer.update_reverse_read(f_in);
        m_file_buffer.rebase_reverse(m_file_block_size);
        m_decrypted_buffer.update_reverse_write(d_out);
      }
      {  // Copy out decrypted data
        ndbxfrm_input_reverse_iterator in =
            m_decrypted_buffer.get_input_reverse_iterator();
        if (!in.empty())
        {
          out->copy_from(&in);
          m_decrypted_buffer.update_reverse_read(in);
          m_decrypted_buffer.rebase_reverse(m_file_block_size);
        }
        if (m_decrypted_buffer.reverse_read_size() == 0 &&
            m_decrypted_buffer.last())
        {
          out->set_last();
        }
      }
    }
    if (out->last())
    {
      m_data_pos -= out_begin - out->begin();
      return 0;
    }
    if (out->empty())
    {
      m_data_pos -= out_begin - out->begin();
      return 1;
    }
  } while (progress);
  // Should not reach here, either loop guard is too low or a bug.
  assert(progress);
  m_data_pos -= out_begin - out->begin();
  return 2;
}

ndb_off_t ndbxfrm_file::move_to_end()
{
  require(is_open());
  ndb_off_t file_pos = m_file_block_size > 0
                       ? ndb_ceil_div(m_payload_end,
                                      (ndb_off_t)m_file_block_size) *
                             m_file_block_size
                       : m_payload_end;
  require(m_file->set_pos(file_pos) == 0);
  m_file_buffer.init_reverse();
  m_decrypted_buffer.init_reverse();
  if (m_encrypted)
  {
    openssl_evp_op.decrypt_end();
  }
  bool use_ndbxfrm1 = (m_file_format == FF_NDBXFRM1);
  ndbxfrm_output_reverse_iterator out(nullptr, nullptr, false);
  int r;
  ndbxfrm_output_reverse_iterator f_out =
      m_file_buffer.get_output_reverse_iterator();
  size_t count = f_out.size();
  if ((ndb_off_t)count > file_pos) count = file_pos;
  m_file_pos = file_pos;
  r = m_file->read_backward(f_out.begin() - count, count);
  if (r == -1)
  {
    RETURN(-1);
  }
  m_file_pos -= r;
  if (size_t(r) != count)
  {
    RETURN(-1);
  }

  if (m_payload_start >= (m_file->get_pos()))
  {
    r -= (m_payload_start - m_file->get_pos());
    f_out.set_last();
  }
  f_out.advance(r);
  m_file_buffer.update_reverse_write(f_out);

  require(r != -1);
  if (use_ndbxfrm1)
  {
    ndbxfrm_input_reverse_iterator rin =
        m_file_buffer.get_input_reverse_iterator();
    rin.advance(file_pos - m_payload_end);
    m_file_buffer.update_reverse_read(rin);
  }

  if (m_encrypted)
  {
    // Init reverse read operation
    if (openssl_evp_op.decrypt_init_reverse(m_data_size, m_payload_end) == -1)
      return -1;
    m_file_op = OP_READ_BACKW;
  }
  m_data_pos = m_data_size;
  require(m_payload_end >= m_payload_start);
  return m_data_size;
}

#ifdef TEST_NDBXFRM_FILE

#include "kernel/signaldata/FsOpenReq.hpp"

int main()
{
  ndb_openssl_evp::library_init();

  using byte = unsigned char;
  const char test_file[] = "TEST_NDBXFRM_FILE.dat";
  int rc;

  ndb_file file;
  ndbxfrm_file xfile;
  bool compress = true;  // use_gz
  const byte *pwd = reinterpret_cast<const byte *>("DUMMY");
  size_t pwd_len = 5;
  int kdf_iter_count = 1;
  int key_cipher = ndb_ndbxfrm1::cipher_xts;
  int key_count = -1;
  size_t key_data_unit_size = ndbxfrm_file::BUFFER_SIZE;
  size_t file_block_size = ndbxfrm_file::BUFFER_SIZE;
  Uint64 data_size = ndbxfrm_file::INDEFINITE_SIZE;
  byte wr_buf[ndbxfrm_file::BUFFER_SIZE + NDB_O_DIRECT_WRITE_BLOCKSIZE];
  byte rd_buf[ndbxfrm_file::BUFFER_SIZE + NDB_O_DIRECT_WRITE_BLOCKSIZE];

  rc = file.create(test_file);
  if (rc == -1)
  {
    perror(test_file);
    fprintf(
        stderr, "ERROR: Please remove file %s and test again.\n", test_file);
    return EXIT_FAILURE;
  }
  require(rc == 0);

  rc = file.open(test_file, FsOpenReq::OM_WRITEONLY);
  require(rc == 0);

  rc = xfile.create(file,
                    compress,
                    pwd,
                    pwd_len,
                    kdf_iter_count,
                    key_cipher,
                    key_count,
                    key_data_unit_size,
                    file_block_size,
                    data_size,
                    false);
  require(rc == 0);

  memset(wr_buf, 17, ndbxfrm_file::BUFFER_SIZE);
  ndbxfrm_input_iterator in = {wr_buf, wr_buf + ndbxfrm_file::BUFFER_SIZE, false};
  rc = xfile.write_forward(&in);
  require(rc == 0);

  memset(wr_buf, 53, NDB_O_DIRECT_WRITE_BLOCKSIZE + 1);
  in = ndbxfrm_input_iterator{wr_buf, wr_buf + NDB_O_DIRECT_WRITE_BLOCKSIZE + 1, true};
  rc = xfile.write_forward(&in);
  require(rc == 0);

  rc = xfile.close(false);
  require(rc == 0);

  rc = file.close();
  require(rc == 0);

  xfile.reset();

  rc = file.open(test_file, FsOpenReq::OM_READONLY);
  require(rc == 0);

  rc = xfile.open(file, pwd, pwd_len);
  require(rc == 0);

  ndbxfrm_output_iterator out = {rd_buf, rd_buf + sizeof(rd_buf), false};
  rc = xfile.read_forward(&out);
  require(rc == 1);  // 1 indicates more to read

  out = ndbxfrm_output_iterator{rd_buf, rd_buf + sizeof(rd_buf), false};
  rc = xfile.read_forward(&out);
  require(rc == 0);

  rc = xfile.close(false);
  require(rc == 0);

  rc = file.sync();
  require(rc == 0);

  rc = file.close();
  require(rc == 0);

  rc = file.remove(test_file);
  require(rc == 0);

  ndb_openssl_evp::library_end();
}

#endif
