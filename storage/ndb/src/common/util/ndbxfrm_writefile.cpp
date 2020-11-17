/* Copyright (c) 2020, Oracle and/or its affiliates.

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

#include "portlib/ndb_file.h"
#include "util/ndb_az31.h"
#include "util/ndb_ndbxfrm1.h"
#include "util/ndb_openssl_evp.h"
#include "util/ndb_zlib.h"
#include "util/ndbxfrm_buffer.h"
#include "util/ndbxfrm_iterator.h"
#include "util/ndbxfrm_writefile.h"

#define RETURN(rv) return(rv)
//#define RETURN(rv) abort()

ndbxfrm_writefile::ndbxfrm_writefile()
: m_file_format(FF_UNKNOWN),
  openssl_evp_op(&openssl_evp),
  m_data_size(0)
{}

bool ndbxfrm_writefile::is_open() const
{
  return m_file_format != FF_UNKNOWN;
}

int ndbxfrm_writefile::open(ndb_file& file, bool compress, byte* pwd, size_t pwd_len, int kdf_iter_count)
{
  m_eof = false;
  m_file_eof = false;

  m_file = &file;

  m_compressed = compress;
  m_encrypted = (pwd != nullptr);

  m_file_buffer.init();
  m_decrypted_buffer.init(); // Only needed for encrypted

  if (m_encrypted)
  {
    m_file_format = FF_NDBXFRM1;
  }
  else if (m_compressed)
  {
    m_file_format = FF_AZ31;
    m_decrypted_buffer.set_last();
  }
  else
  {
    m_file_format = FF_RAW;
    m_decrypted_buffer.set_last();
  }

  // Write file header
  if (m_file_format == FF_AZ31)
  {
    m_file_block_size = 512; // Backward compatibility requires 512 bytes.
    static_assert(512 == NDB_O_DIRECT_WRITE_ALIGNMENT, "");
    ndbxfrm_output_iterator out = m_file_buffer.get_output_iterator();
    require(ndb_az31::write_header(&out) == 0);
    m_file_buffer.update_write(out);
  }
  else if (m_file_format == FF_NDBXFRM1)
  {
    m_file_block_size = NDB_O_DIRECT_WRITE_ALIGNMENT;
    ndbxfrm_output_iterator out = m_file_buffer.get_output_iterator();
    ndb_ndbxfrm1::header ndbxfrm1;
    ndbxfrm1.set_file_block_size(m_file_block_size);
    if (m_compressed)
    {
      ndbxfrm1.set_compression_method(1 /* deflate */);
    }
    if (m_encrypted)
    {
      openssl_evp.set_aes_256_cbc(true, 0);
      ndbxfrm1.set_encryption_cipher(1 /* CBC-STREAM */);
      ndbxfrm1.set_encryption_padding(1 /* ON PKCS */);

      byte salt[ndb_openssl_evp::SALT_LEN];
      openssl_evp.generate_salt256(salt);
      ndbxfrm1.set_encryption_salts(salt, ndb_openssl_evp::SALT_LEN, 1);
      openssl_evp.derive_and_add_key_iv_pair(pwd, pwd_len, kdf_iter_count, salt);
      ndbxfrm1.set_encryption_kdf(1 /* pbkdf2_sha256 */);
      ndbxfrm1.set_encryption_kdf_iter_count(kdf_iter_count);
      int rv = openssl_evp_op.encrypt_init(0, 0);
      require(rv == 0);
    }
    require(ndbxfrm1.prepare_for_write() == 0);
    require(ndbxfrm1.get_size() <= out.size());
    require(ndbxfrm1.write_header(&out) == 0);
    m_file_buffer.update_write(out);
  }
  if (m_compressed)
  {
    zlib.deflate_init();
  }

  return 0;
}

int ndbxfrm_writefile::flush_payload()
{
  // Flush all transformed data to file buffer.
  if (m_file_buffer.last())
  { // All should already be compressed and encrypted as needed.
    require(m_decrypted_buffer.last());
    require(m_decrypted_buffer.read_size() == 0);
    if (m_file_buffer.read_size() == 0)
      return 0; // Nothing more to write to file.
  }
  else if (!m_encrypted || !m_decrypted_buffer.last())
  {
    // Mark that there will be no more payload.
    byte dummy[1];
    ndbxfrm_input_iterator in(dummy, dummy, true);
    int r = write_forward(&in);
    if (r == -1)
      return -1;
    require(r == 0);
    require(m_decrypted_buffer.last());
    require(m_decrypted_buffer.read_size() == 0);
  } 

  return 0;
}

int ndbxfrm_writefile::close(bool no_flush)
{
  if (!is_open())
  {
    RETURN(-1);
  }
  if (!no_flush)
  {
    if (flush_payload() == -1)
    {
      RETURN(-1);
    }
  }

  if (m_encrypted)
  {
    openssl_evp_op.encrypt_end();
    openssl_evp.reset();
    m_encrypted = false;
  }

  if (m_compressed)
  {
    zlib.deflate_end();
    m_compressed = false;
  }

  if (!no_flush && m_file_format != FF_RAW)
  {
    // Allow trailer to be written into buffer.
    m_file_buffer.clear_last();

    // AZ31 have a 12 byte trailer padded to file block size.
    // NDBXFRM1 trailer is at most 512 bytes plus padding.
    size_t max_trailer_size = m_file_block_size + 512;
    if (m_file_buffer.write_space() < max_trailer_size)
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
      m_file_buffer.update_read(in);
      m_file_buffer.rebase(m_file_block_size);
    }
    if (m_file_buffer.write_space() < max_trailer_size)
    {
      RETURN(-1);
    }

    ndbxfrm_output_iterator out = m_file_buffer.get_output_iterator();
    int r = write_trailer(&out);
    if (r == -1)
    {
      RETURN(-1);
    }
    m_file_buffer.update_write(out);
    m_file_format = FF_RAW;
  }

  if (!no_flush)
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
    m_file_buffer.update_read(in);
    m_file_buffer.rebase(m_file_block_size);
  }

  return 0;
}


int ndbxfrm_writefile::write_trailer(ndbxfrm_output_iterator *out)
{
  off_t file_pos = m_file->get_pos();
  file_pos += m_file_buffer.read_size();

  int r = -1;
  if (m_file_format == FF_AZ31)
  {
    ndb_az31 az31;
    require(az31.set_data_size(m_data_size) == 0);
    require(az31.set_data_crc32(m_crc32) == 0);

    size_t last_block_size =
      (file_pos + az31.get_trailer_size()) % m_file_block_size;
    size_t padding = (m_file_block_size - last_block_size) % m_file_block_size;
    r = az31.write_trailer(out, padding);
  }
  else if (m_file_format == FF_NDBXFRM1)
  {
    ndb_ndbxfrm1::trailer ndbxfrm1;
    require(ndbxfrm1.set_data_size(m_data_size) == 0);
    require(ndbxfrm1.set_data_crc32(m_crc32) == 0);
    require(ndbxfrm1.set_file_pos(file_pos) == 0);
    require(ndbxfrm1.set_file_block_size(m_file_block_size) == 0);
    require(ndbxfrm1.prepare_for_write() == 0);
    r = ndbxfrm1.write_trailer(out);
  }
  if (r == -1)
  {
    return -1;
  }
  out->set_last();

  return 0;
}

int ndbxfrm_writefile::write_forward(ndbxfrm_input_iterator* in)
{
  const byte* in_cbegin = in->cbegin();
  ndbxfrm_buffer* file_bufp = nullptr;
  ndbxfrm_input_iterator file_in = *in;

  if (m_compressed)
  {
    ndbxfrm_buffer* compressed_buffer = m_encrypted
                                        ? &m_decrypted_buffer
                                        : &m_file_buffer;
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
  { // copy app data into m_decrypted_buffer
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
      size_t copy_len = std::min(in->size(), out.size());
      memcpy(out.begin(), in->cbegin(), copy_len);
      out.advance(copy_len);
      in->advance(copy_len);
      require(!out.last());
      if (in->last() && in->empty()) out.set_last();

      m_decrypted_buffer.update_write(out);
    }
  }

  if (m_encrypted)
  { // encrypt data from m_decrypted_buffer into m_file_buffer
    if (m_file_buffer.last())
    {
      require(m_decrypted_buffer.last());
      require(m_decrypted_buffer.read_size()==0);
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
      int rv = openssl_evp_op.encrypt(&out, &c_in);
      if (rv == -1)
      {
        RETURN(-1);
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
    write_len -= write_len % NDB_O_DIRECT_WRITE_BLOCKSIZE;
  int n;
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

  return 0;
}
