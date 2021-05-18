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

#include "util/ndbxfrm_readfile.h"

#include "portlib/ndb_file.h"
#include "util/ndb_az31.h"
#include "util/ndb_ndbxfrm1.h"
#include "util/ndb_openssl_evp.h"
#include "util/ndb_zlib.h"
#include "util/ndbxfrm_buffer.h"
#include "util/ndbxfrm_iterator.h"

#define RETURN(rv) return(rv)
//#define RETURN(rv) abort()

ndbxfrm_readfile::ndbxfrm_readfile()
    : m_file_format(FF_UNKNOWN), openssl_evp_op(&openssl_evp), m_data_size(0)
{}

bool ndbxfrm_readfile::is_open() const
{
  return m_file_format != FF_UNKNOWN;
}

int ndbxfrm_readfile::open(ndb_file& file,
                           const byte* pwd,
                           size_t pwd_len)
{
  m_eof = false;
  m_file_eof = false;
  m_decrypted_buffer.init();
  m_file_buffer.init();
  m_payload_start = m_payload_end = 0;
  m_file = &file;
  int rv;

  // Detect and read file header
  ndbxfrm_output_iterator out = m_file_buffer.get_output_iterator();
  rv = m_file->read_forward(out.begin(), out.size());
  if (rv == -1)
  {
    RETURN(-1);
  }
  if (size_t(rv) < out.size()) m_file_eof = true;
  if (size_t(rv) < out.size()) out.set_last();
  out.advance(rv);
  m_file_buffer.update_write(out);

  ndbxfrm_input_iterator in = m_file_buffer.get_input_iterator();
  const byte* in_begin = in.cbegin();
  size_t header_size = 0;
  if (ndb_az31::detect_header(&in) == 0)
  {
    rv = ndb_az31::read_header(&in);
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
    size_t trailer_max_size = 12;
    m_read_ahead = trailer_max_size + m_file_block_size;
    m_file_buffer.update_read(in);
    m_file_buffer.rebase(m_file_block_size);
    m_file_format = FF_AZ31;
    m_compressed = true;
    m_encrypted = false;
    if (zlib.inflate_init() == -1)
    {
      RETURN(-1);
    }
  }
  else if (ndb_ndbxfrm1::header::detect_header(&in, &header_size) == 0)
  {
    if (header_size > in.size())
    {
      RETURN(-1);
    }
    ndb_ndbxfrm1::header ndbxfrm;
    rv = ndbxfrm.read_header(&in);
    if (rv == -1)
    {
      RETURN(-1);
    }
    if (rv == 1)
    {
      RETURN(-1);
    }
    m_file_format = FF_NDBXFRM1;
    ndbxfrm.get_file_block_size(&m_file_block_size);
    size_t trailer_max_size;
    ndbxfrm.get_trailer_max_size(&trailer_max_size);
    m_read_ahead = trailer_max_size + m_file_block_size;

    m_compressed = (ndbxfrm.get_compression_method() != 0);
    if (m_compressed)
    {
      if (zlib.inflate_init() == -1)
      {
        RETURN(-1);
      }
    }

    Uint32 cipher = 0;
    ndbxfrm.get_encryption_cipher(&cipher);
    m_encrypted = (cipher != 0);
    if (m_encrypted)
    {
      Uint32 padding = 0;
      Uint32 data_unit_size = 0;
      Uint32 kdf = 0;
      Uint32 kdf_iter_count = 0;
      byte salt[ndb_openssl_evp::SALT_LEN];
      size_t salt_size = 0;
      size_t salt_count = 0;

      ndbxfrm.get_encryption_padding(&padding);
      // TODO: get_data_unit_size() and check that it is zero
      ndbxfrm.get_encryption_kdf(&kdf);
      ndbxfrm.get_encryption_kdf_iter_count(&kdf_iter_count);
      ndbxfrm.get_encryption_salts(salt, sizeof(salt), &salt_size, &salt_count);

      if (cipher != 1)  // cbc
        RETURN(-1);
      if (!(padding == 0 /* none */ || padding == 1 /* PKCS */))
        RETURN(-1);
      if (kdf != 1)  // pbkdf2_sha256
        RETURN(-1);
      if (salt_size != sizeof(salt) || salt_count != 1)
       RETURN(-1);

      openssl_evp.reset();
      openssl_evp.set_aes_256_cbc((padding == 1), data_unit_size);
      openssl_evp.derive_and_add_key_iv_pair(
          pwd, pwd_len, kdf_iter_count, salt);
    }

    m_payload_start = in.cbegin() - in_begin;
    m_file_buffer.update_read(in);
    m_file_buffer.rebase(m_file_block_size);
  }
  else
  {
    m_file_format = FF_RAW;
    m_compressed = m_encrypted = false;
    m_file_block_size = 0;
    m_read_ahead = 0;
  }
  m_payload_start = header_size;

  if (m_encrypted)
  {
    // Init forward read operation
    if (openssl_evp_op.decrypt_init(0, m_payload_start) == -1)
      RETURN(-1);
  }

  // If all file read, read trailer
  if (m_file_format == FF_AZ31 || m_file_format == FF_NDBXFRM1)
  {
    ndbxfrm_input_iterator f_in = m_file_buffer.get_input_iterator();
    if (f_in.last())
    {
      size_t trailer_size;
      require(read_trailer_forward(&trailer_size) == 0);
    }
    require(rv != -1);
  }
  require(is_open());
  return 0;
}

int ndbxfrm_readfile::close()
{
  if (!m_eof)
  {
    /*
     * If file closed without reading end of file, make sure to cleanup
     * compression and/or encryption state avoiding memory leak.
     */
    if (m_compressed)
    {
      zlib.inflate_end();
    }
    if (m_encrypted)
    {
      openssl_evp_op.decrypt_end();
    }
  }
  if (!is_open())
  {
    RETURN(-1);
  }
  return 0;
}

int ndbxfrm_readfile::read_trailer(size_t* trailer_size)
{
  ndbxfrm_buffer* file_buffer = &m_file_buffer;
  ndbxfrm_input_reverse_iterator in = file_buffer->get_input_reverse_iterator();
  const byte* in_beg = in.cbegin();

  if (m_file_format == FF_AZ31)
  {
    ndb_az31 az31;
    int r = az31.read_trailer(&in);
    if (r == -1)
    {
      RETURN(-1);
    }
    if (r == 1)
    {
      RETURN(-1);
    }
    az31.get_data_size(&m_data_size);
    file_buffer->update_reverse_read(in);
  }
  else if (m_file_format == FF_NDBXFRM1)
  {
    ndb_ndbxfrm1::trailer ndbxfrm1;
    int r = ndbxfrm1.read_trailer(&in);
    if (r == -1)
    {
      RETURN(-1);
    }
    if (r == 1)
    {
      RETURN(-1);
    }
    ndbxfrm1.get_data_size(&m_data_size);
    file_buffer->update_reverse_read(in);
  }
  *trailer_size = in_beg - in.cbegin();
  require(*trailer_size > 0);

  size_t m_file_end = m_file->get_size();
  m_payload_end = m_file_end - *trailer_size;
  return 0;
}

int ndbxfrm_readfile::read_trailer_forward(size_t* trailer_size_ptr)
{
  ndbxfrm_input_iterator f_in = m_file_buffer.get_input_iterator();
  require(f_in.last());
  ndbxfrm_input_reverse_iterator in(f_in.cend(), f_in.cbegin(), false);
  if (m_file_format == FF_AZ31)
  {
    ndb_az31 az31;
    int r = az31.read_trailer(&in);
    if (r == -1)
    {
      RETURN(-1);
    }
    if (r == 1)
    {
      RETURN(-1);
    } 
  }
  else if (m_file_format == FF_NDBXFRM1)
  {
    ndb_ndbxfrm1::trailer ndbxfrm1;
    int r = ndbxfrm1.read_trailer(&in);
    if (r == -1)
    {
      RETURN(-1);
    }
    if (r == 1)
    {
      RETURN(-1);
    }
  }
  else
  {
    RETURN(-1);
  }
  *trailer_size_ptr = f_in.size() - in.size();
  f_in.reduce(f_in.size() - in.size());
  m_file_buffer.update_read(f_in);
  ndbxfrm_output_iterator out =
      ndbxfrm_output_iterator((byte*)f_in.cend(), (byte*)f_in.cend(), true);
  f_in = m_file_buffer.get_input_iterator();
  m_file_buffer.update_write(out);

  size_t m_file_end = m_file->get_size();
  require(*trailer_size_ptr > 0);
  m_payload_end = m_file_end - *trailer_size_ptr;
  return 0;
}

int ndbxfrm_readfile::read_forward(ndbxfrm_output_iterator* out)
{
  if (m_eof)
  {
    out->set_last();
    return 0;
  }

  if (m_file_format == FF_RAW)
  {
    ndbxfrm_input_iterator in = m_file_buffer.get_input_iterator();
    if (!in.empty())
    {
      size_t copy_len = std::min(in.size(), out->size());
      memcpy(out->begin(), in.cbegin(), copy_len);
      out->advance(copy_len);
      in.advance(copy_len);
      m_file_buffer.update_read(in);
      m_file_buffer.rebase(m_file_block_size);
    }
    if (!in.empty())
    {
      require(out->empty());
    }
    else if (in.last())
    {
      out->set_last();
    }
    else if (!out->empty())
    {
      int r;
      if (!m_file_eof)
      {
        r = m_file->read_forward(out->begin(), out->size());
        if (r == -1)
        {
          RETURN(-1);
        }
      }
      else
        r = 0;
      if (size_t(r) < out->size())
      {
        m_file_eof = true;
      }
      if (size_t(r) < out->size())
      {
        out->set_last();
      }
      out->advance(r);
    }
    if (out->last())
    {
      m_eof = true;
    }
    return out->last() ? 0 : 1;
  }

  int G = 20; // Loop guard
  for (;;)
  {
    require(--G);
    if (m_encrypted)
    {  // decrypt file_buffer into decrypted_buffer
      ndbxfrm_input_iterator in = m_file_buffer.get_input_iterator();
      ndbxfrm_output_iterator d_out = m_decrypted_buffer.get_output_iterator();
      if (in.last() || in.size() >= m_read_ahead)
      {
        if (!in.last()) in.reduce(m_read_ahead);
        if (!in.empty())
        {
          int r;
          if (d_out.empty())
            r = 1;
          else
            r = openssl_evp_op.decrypt(&d_out, &in);
          if (r == -1)
          {
            RETURN(-1);
          }
          if (r == 0)
          {
            if (!in.empty() || !in.last() || !d_out.last())
            {
              RETURN(-1);
            }
            r = openssl_evp_op.decrypt_end();
            m_file_eof = true;
          }
          else
          {
            require(r == 1 || r == 2);
            require(!d_out.last());
            if (r == 2)  // no progress
            {
              ;
            }
          }
          m_file_buffer.update_read(in);
          m_file_buffer.rebase(m_file_block_size);
        }
        else if (in.last() && !d_out.last())
        {
          require(m_file_eof);
          d_out.set_last();
        }
        m_decrypted_buffer.update_write(d_out);
      }
      else
      {
        require(!m_file_eof);
      }
    }

    // out can be passed in as empty, else could require(!out->empty()).
    require(!out->last());

    if (m_compressed)
    {  // decompress compressed_buffer into callers buffer
      ndbxfrm_buffer* compressed_buffer =
          m_encrypted ? &m_decrypted_buffer : &m_file_buffer;
      ndbxfrm_input_iterator in = compressed_buffer->get_input_iterator();
      if (in.last() || in.size() >= m_read_ahead)
      {
        if (!in.last()) in.reduce(m_read_ahead);
        if (in.last() || !in.empty())
        {
          int r;
          if (out->empty())
            r = 1;
          else
          {
            r = zlib.inflate(out, &in);
          }
          if (r == -1)
          {
            RETURN(-1);
          }
          if (r == 0)
          {
            require(out->last());
            require(in.last());
            require(in.empty());
            if (!in.empty() || !in.last() || !out->last())
            {
              RETURN(-1);
            }
            r = zlib.inflate_end();
            m_eof = true;
          }
          else
          {
            require(r == 1);
            require(!out->last());
          }
          compressed_buffer->update_read(in);
          compressed_buffer->rebase(m_file_block_size);
        }
        else if (in.last() && !out->last())
        {
          require(in.empty());
          require(m_file_eof);
          out->set_last();
        }
      }
      else
      {
        require(!m_file_eof);
      }
    }
    else
    {  // copy decrypted_buffer into callers buffer (no decompression needed)
      ndbxfrm_input_iterator c_in = m_decrypted_buffer.get_input_iterator();
      size_t copy_len = std::min(c_in.size(), out->size());
      memcpy(out->begin(), c_in.cbegin(), copy_len);
      c_in.advance(copy_len);
      out->advance(copy_len);
      if (c_in.empty() && c_in.last()) out->set_last();
      m_decrypted_buffer.update_read(c_in);
      m_decrypted_buffer.rebase(m_file_block_size);
    }

    if (out->last() && !m_eof)
    {
      if (m_compressed) zlib.inflate_end();
      if (m_encrypted)
      {
        openssl_evp_op.decrypt_end();
      }
      m_eof = true;
    }
    if (out->empty() || out->last()) return out->last() ? 0 : 1;

    // read more into file_buffer
    ndbxfrm_output_iterator fout = m_file_buffer.get_output_iterator();
    if (fout.last())
    {
      require(m_file_eof);
      require(out->empty() || out->last());
      abort();
    }
    require(!fout.last());
    require(!m_file_eof);
    int r = m_file->read_forward(fout.begin(), fout.size());
    if (r == -1)
    {
      RETURN(-1);
    }
    if (size_t(r) < fout.size()) m_file_eof = true;
    fout.advance(r);
    require(r == 0 || !fout.last());
    require(!fout.last());
    if (!fout.empty() && !fout.last())
    {
      fout.set_last();
      m_file_buffer.update_write(fout);
      ndbxfrm_input_iterator f_in = m_file_buffer.get_input_iterator();
      size_t pre_size = f_in.size();

      size_t trailer_size;
      require(read_trailer_forward(&trailer_size) == 0);
      f_in = m_file_buffer.get_input_iterator();
      require(pre_size == f_in.size() + trailer_size);
      require(f_in.last());
    }
    else
    {
      m_file_buffer.update_write(fout);
    }
  }
}

int ndbxfrm_readfile::read_backward(ndbxfrm_output_reverse_iterator* out)
{
  require(!out->empty() && !out->last());

  if (m_file_format == FF_RAW)
  {
    ndbxfrm_input_reverse_iterator in =
        m_file_buffer.get_input_reverse_iterator();
    if (!in.empty())
    {
      size_t copy_len = std::min(in.size(), out->size());
      memcpy(out->begin() - copy_len, in.cbegin() - copy_len, copy_len);
      out->advance(copy_len);
      in.advance(copy_len);
      m_file_buffer.update_reverse_read(in);
      m_file_buffer.rebase_reverse(m_file_block_size);
    }
    if (!in.empty())
    {
      require(out->empty());
    }
    else if (in.last())
    {
      out->set_last();
    }
    else if (!out->empty())
    {
      int r;
      bool eof = false;
      if (!m_file_eof)
      {
        size_t count = out->size();
        eof = (count >= size_t(m_file->get_pos()));
        if (eof) count = size_t(m_file->get_pos());
        r = m_file->read_backward(out->begin() - count, count);
        if (r == -1)
        {
          RETURN(-1);
        }
        if (m_payload_start >= size_t(m_file->get_pos()))
        {
          r -= (m_payload_start - m_file->get_pos());
          eof = true;
        }
      }
      else
      {
        r = 0;
      }
      if (size_t(r) < out->size() || eof)
      {
        m_file_eof = true;
      }
      if (size_t(r) < out->size() || eof)
      {
        out->set_last();
      }
      out->advance(r);
    }
    if (out->last())
    {
      m_eof = true;
    }
    return out->last() ? 0 : 1;
  }

  require(!out->empty() && !out->last());
  int G = 20;
  for (;;)
  {
    require(--G);
    if (m_encrypted)
    {  // decrypt file_buffer into decrypted_buffer
       // require(!m_compressed); // do not support both yet
      ndbxfrm_input_reverse_iterator in =
          m_file_buffer.get_input_reverse_iterator();
      ndbxfrm_output_reverse_iterator d_out =
          m_decrypted_buffer.get_output_reverse_iterator();
      if (in.last() || !in.empty())  // || in.size() >= m_read_ahead)
      {
        if (!in.empty())
        {
          int r;
          if (d_out.empty())
            r = 1;
          else
            r = openssl_evp_op.decrypt_reverse(&d_out, &in);
          if (r == -1)
          {
            RETURN(-1);
          }
          if (r == 0)
          {
            if (!in.empty() || !in.last() || !d_out.last())
            {
              RETURN(-1);
            }
            r = openssl_evp_op.decrypt_end();
            m_eof = true;
          }
          else
          {
            require(r == 1 || r == 2);
            require(!d_out.last());
            if (r == 2)  // no progress
            {
              ;
            }
          }
          m_file_buffer.update_reverse_read(in);
          m_file_buffer.rebase_reverse(m_file_block_size);
        }
        else if (in.last() && !d_out.last())
        {
          require(m_file_eof);
          d_out.set_last();
        }
        m_decrypted_buffer.update_reverse_write(d_out);
      }
      else
      {
        require(!m_file_eof);
      }
    }

    require(!out->empty() && !out->last());

    require(!m_compressed);
    // copy decrypted_buffer into callers buffer (no decompression needed)
    ndbxfrm_input_reverse_iterator c_in =
        m_decrypted_buffer.get_input_reverse_iterator();
    size_t copy_len = std::min(c_in.size(), out->size());
    memcpy(out->begin() - copy_len, c_in.cbegin() - copy_len, copy_len);
    c_in.advance(copy_len);
    out->advance(copy_len);
    if (c_in.empty() && c_in.last()) out->set_last();
    m_decrypted_buffer.update_reverse_read(c_in);
    m_decrypted_buffer.rebase_reverse(m_file_block_size);

    if (out->last() && !m_eof)
    {
      require(!m_compressed);
      if (m_encrypted)
      {
        openssl_evp_op.decrypt_end();
      }
      m_eof = true;
    }
    if (out->empty() || out->last()) return out->last() ? 0 : 1;

    // read more into file_buffer
    ndbxfrm_output_reverse_iterator fout =
        m_file_buffer.get_output_reverse_iterator();
    if (fout.last())
    {
      m_file_eof = true;
      require(m_file_eof);
      out->set_last();
      return 0;
    }
    else
    {
      require(!fout.last());
      require(!m_file_eof);
      size_t count = fout.size();
      if (count > size_t(m_file->get_pos())) count = size_t(m_file->get_pos());
      int r = m_file->read_backward(fout.begin() - count, count);
      if (r == -1)
      {
        RETURN(-1);
      }
      if (size_t(m_file->get_pos()) <= m_payload_start)
      {
        r -= (m_payload_start - m_file->get_pos());
        fout.set_last();
        m_file_eof = true;
      }
      if (size_t(r) < fout.size()) m_file_eof = true;
      fout.advance(r);
      // require(r==0 || !fout.last());
    }
    if (!fout.empty())  //  && !fout.last())
    {
      fout.set_last();
      m_file_eof = true;
      m_file_buffer.update_reverse_write(fout);
      ndbxfrm_input_reverse_iterator in =
          m_file_buffer.get_input_reverse_iterator();
      require(in.last());
    }
    else
    {
      m_file_buffer.update_reverse_write(fout);
    }
  }
  return 0;
}

off_t ndbxfrm_readfile::move_to_end()
{
  require(is_open());
  off_t sz = m_file->get_size();
  require(sz != -1);
  require(m_file->set_pos(sz) == 0);

  const size_t file_size = sz;
  m_file_buffer.init_reverse();
  m_decrypted_buffer.init_reverse();
  if (m_encrypted)
  {
    openssl_evp_op.decrypt_end();
  }
  bool use_ndbxfrm1 = (m_file_format == FF_NDBXFRM1);

  ndbxfrm_output_reverse_iterator out(nullptr, nullptr, false);
  m_file_eof = false;
  int r;
  ndbxfrm_output_reverse_iterator f_out =
      m_file_buffer.get_output_reverse_iterator();
  size_t count = f_out.size();
  if (count > file_size) count = file_size;
  r = m_file->read_backward(f_out.begin() - count, count);
  if (r == -1)
  {
    RETURN(-1);
  }
  if (size_t(r) != count)
  {
    RETURN(-1);
  }
  if (m_payload_start >= Uint64(m_file->get_pos()))
  {
    r -= (m_payload_start - m_file->get_pos());
    f_out.set_last();
  }
  f_out.advance(r);
  m_file_buffer.update_reverse_write(f_out);

  require(r != -1);
  if (use_ndbxfrm1)
  {
    require(m_file_buffer.reverse_read_size() > 0);
    m_file_format = FF_NDBXFRM1;
    size_t trailer_size;
    if (read_trailer(&trailer_size) == -1) return -1;
    require(trailer_size <= file_size);
    require(trailer_size > 0);
    m_payload_end = file_size - trailer_size;
    require(m_data_size + 16 >= m_payload_end - m_payload_start);
  }
  else
  {
    require(m_payload_end == 0);
    m_payload_end = file_size;
  }

  if (m_encrypted)
  {
    // Init reverse read operation
    if (openssl_evp_op.decrypt_init_reverse(m_data_size, m_payload_end) == -1)
      return -1;
  }
  return m_data_size;  // sz;
}

off_t ndbxfrm_readfile::get_size() const
{
  return m_file_format == FF_RAW ? m_file->get_size() : m_data_size;
}
