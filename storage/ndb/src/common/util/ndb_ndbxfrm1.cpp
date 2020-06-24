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

#include "my_byteorder.h" // WORDS_BIGENDIAN
#include "ndb_global.h" // require()
#include "util/ndb_ndbxfrm1.h"

#define RETURN(rv) return(rv)
//#define RETURN(rv) abort()

const char ndb_ndbxfrm1::magic[8] = {'N', 'D', 'B', 'X', 'F', 'R', 'M', '1'};

ndb_ndbxfrm1::header::header()
{
  memset(&m_buffer, 0, sizeof(m_buffer));
  memcpy(m_buffer.m_header.m_magic.m_magic, magic, sizeof(magic));
  m_buffer.m_header.m_magic.m_endian = native_endian_marker;
  m_buffer.m_header.m_magic.m_header_size = sizeof(m_buffer.m_header);
  m_buffer.m_header.m_magic.m_fixed_header_size = sizeof(m_buffer.m_header);
  m_buffer.m_header.m_dbg_writer_ndb_version = NDB_VERSION_D;
  m_buffer.m_header.m_trailer_max_size = sizeof(ndb_ndbxfrm1::trailer);
}

ndb_ndbxfrm1::trailer::trailer()
: m_file_pos(0),
  m_file_block_size(0),
  m_zero_pad_size(0)
{
  memset(&m_buffer, 0, sizeof(m_buffer));
  memcpy(m_buffer.m_trailer.m_magic.m_magic, magic, sizeof(magic));
  m_buffer.m_trailer.m_magic.m_endian = native_endian_marker;
  m_buffer.m_trailer.m_magic.m_trailer_size = sizeof(m_buffer.m_trailer);
  m_buffer.m_trailer.m_magic.m_fixed_trailer_size = sizeof(m_buffer.m_trailer);
}

/*
 * -1 - bad magic
 *  0 - ok magic, store needed data in len
 *  1 - need more data,
 */
int ndb_ndbxfrm1::header::detect_header(ndbxfrm_input_iterator* in, size_t* header_size_ptr)
{
  const byte* buf = in->cbegin();
  size_t len = in->size();

  if (len < 8)
    return 1;
  if (memcmp(buf, magic, 8) != 0)
    return -1;

  if (len < sizeof(fixed_header::magic))
    return 1;
  const fixed_header::magic* mh =
    reinterpret_cast<const fixed_header::magic*>(buf);

  bool toggle_endian = (mh->m_endian != native_endian_marker);
  if (toggle_endian && (mh->m_endian != reverse_endian_marker))
    RETURN(-1); // Bad endian marker
  Uint32 header_size = mh->m_header_size;
  if (toggle_endian) toggle_endian32(&header_size);
  if (header_size > MAX_HEADER_SIZE + MAX_OCTETS_SIZE)
    RETURN(-1);
  if (header_size < MIN_HEADER_SIZE)
    RETURN(-1);
  if (header_size % 8 != 0)
    RETURN(-1);

  *header_size_ptr = header_size;
  return 0;
}

bool ndb_ndbxfrm1::is_all_zeros(const void* buf, size_t len)
{
  const byte* p = static_cast<const byte*>(buf);
  const byte* endp = p + len;
  while (p < endp && *p == 0) p++;
  return (p == endp);
}

int ndb_ndbxfrm1::header::read_header(ndbxfrm_input_iterator* in)
{
  const byte* buf = in->cbegin();
  size_t len = in->size();

  memset(&m_buffer, 0, sizeof(m_buffer));

  const byte* p = buf;

  if (len < sizeof(fixed_header::magic))
    RETURN(-1);
  const fixed_header::magic* magicp =
    reinterpret_cast<const fixed_header::magic*>(p);

  bool detect_toggle_endian = (magicp->m_endian == reverse_endian_marker);
  if (!detect_toggle_endian &&
      magicp->m_endian != native_endian_marker)
  {
    RETURN(-1); // Bad endian
  }

  Uint32 header_size = magicp->m_header_size;
  Uint32 fixed_header_size = magicp->m_fixed_header_size;
  if (detect_toggle_endian)
  {
    toggle_endian32(&header_size);
    toggle_endian32(&fixed_header_size);
  }
  if (header_size > len)
    RETURN(-1);
  if (fixed_header_size > header_size)
    RETURN(-1);
  Uint32 copy_size = fixed_header_size;
  while (copy_size > 0 && p[copy_size - 1] != 0)
  {
    copy_size--;
  }
  if (copy_size > sizeof(fixed_header))
    RETURN(-1);
  memset(&m_buffer.m_header, 0, sizeof(m_buffer.m_header));
  memcpy(&m_buffer.m_header, p, copy_size);
  if (detect_toggle_endian)
    m_buffer.m_header.toggle_endian(); 

  Uint32 octets_size = m_buffer.m_header.m_octets_size;
  if (fixed_header_size + octets_size > header_size)
    RETURN(-1);
  if (octets_size > sizeof(m_buffer.m_octets))
    RETURN(-1);
  memcpy(&m_buffer.m_octets, &buf[fixed_header_size], octets_size);

  Uint32 zero_pad_size = header_size - (fixed_header_size + octets_size);
  p = &buf[fixed_header_size + octets_size];
  for (Uint32 i = 0; i < zero_pad_size; i++)
    if (p[i] != 0)
      RETURN(-1);

  in->advance(header_size);
  return 0;
}

int ndb_ndbxfrm1::header::set_file_block_size(size_t file_block_size)
{
  m_buffer.m_header.m_file_block_size = file_block_size;
  return 0;
}

int ndb_ndbxfrm1::header::get_file_block_size(size_t* file_block_size)
{
  *file_block_size = m_buffer.m_header.m_file_block_size;
  return 0;
}

int ndb_ndbxfrm1::header::get_trailer_max_size(size_t* trailer_max_size)
{
  *trailer_max_size = m_buffer.m_header.m_trailer_max_size;
  return 0;
}

int ndb_ndbxfrm1::header::set_compression_method(unsigned int method)
{
  require(method == 1); // TODO use name deflate
  require((m_buffer.m_header.m_flags & fixed_header::flag_compress_mask) == 0);
  m_buffer.m_header.m_flags |= fixed_header::flag_compress_deflate;
  return 0;
}

int ndb_ndbxfrm1::header::get_compression_method() const
{
  switch (m_buffer.m_header.m_flags & fixed_header::flag_compress_mask)
  {
  case 0: return 0;
  case fixed_header::flag_compress_deflate: return 1;
  default:
    RETURN(-1);
  }
}

int ndb_ndbxfrm1::header::set_encryption_cipher(Uint32 cipher)
{
  require(cipher == 1); // cbc
  require((m_buffer.m_header.m_flags & fixed_header::flag_encrypt_cipher_mask) == 0);
  m_buffer.m_header.m_flags |= fixed_header::flag_encrypt_cipher_aes_256_cbc;
  return 0;
}

int ndb_ndbxfrm1::header::set_encryption_padding(Uint32 padding)
{
  require(padding == 1); // pkcs
  require((m_buffer.m_header.m_flags & fixed_header::flag_encrypt_padding_mask) == 0);
  m_buffer.m_header.m_flags |= fixed_header::flag_encrypt_padding_pkcs;
  return 0;
}

int ndb_ndbxfrm1::header::set_encryption_kdf(Uint32 kdf)
{
  require(kdf == 1); // pkdf2_sha256
  require((m_buffer.m_header.m_flags & fixed_header::flag_encrypt_kdf_mask) == 0);
  m_buffer.m_header.m_flags |= fixed_header::flag_encrypt_kdf_pbkdf2_sha256;
  return 0;
}

int ndb_ndbxfrm1::header::set_encryption_kdf_iter_count(Uint32 count)
{
  require(m_buffer.m_header.m_encrypt_key_definition_iterator_count == 0);
  m_buffer.m_header.m_encrypt_key_definition_iterator_count = count;
  return 0;
}

int ndb_ndbxfrm1::header::set_encryption_salts(const byte* salts, size_t salt_size, size_t salt_count)
{
  require(m_buffer.m_header.m_octets_size == 0);
  require(m_buffer.m_header.m_encrypt_key_definition_salts_position_in_octets == 0);
  m_buffer.m_header.m_encrypt_key_definition_salt_size = salt_size;
  m_buffer.m_header.m_encrypt_key_definition_salt_count = salt_count;
  size_t salt_octets_size = salt_size * salt_count;
  if (m_buffer.m_header.m_octets_size + salt_octets_size > MAX_OCTETS_SIZE)
    return -1;
  memcpy(m_buffer.m_octets, salts, salt_octets_size);
  m_buffer.m_header.m_octets_size += salt_octets_size;
  m_buffer.m_header.m_magic.m_header_size += salt_octets_size;
  return 0;
}

int ndb_ndbxfrm1::header::get_encryption_cipher(Uint32* cipher) const
{
  switch (m_buffer.m_header.m_flags & fixed_header::flag_encrypt_cipher_mask)
  {
    case 0: *cipher = 0; return 0;
    case fixed_header::flag_encrypt_cipher_aes_256_cbc: *cipher = 1; return 0;
    default:
      RETURN(-1);
  }
}

int ndb_ndbxfrm1::header::get_encryption_padding(Uint32* padding) const
{
  switch (m_buffer.m_header.m_flags & fixed_header::flag_encrypt_padding_mask)
  {
    case 0: *padding = 0; return 0;
    case fixed_header::flag_encrypt_padding_pkcs: *padding = 1; return 0;
    default:
      RETURN(-1);
  }
}

int ndb_ndbxfrm1::header::get_encryption_kdf(Uint32* kdf) const
{
  switch (m_buffer.m_header.m_flags & fixed_header::flag_encrypt_kdf_mask)
  {
    case 0: *kdf = 0; return 0;
    case fixed_header::flag_encrypt_kdf_pbkdf2_sha256: *kdf = 1; return 0;
    default:
      RETURN(-1);
  }
}

int ndb_ndbxfrm1::header::get_encryption_kdf_iter_count(Uint32* count) const
{
  *count = m_buffer.m_header.m_encrypt_key_definition_iterator_count;
  return 0;
}

int ndb_ndbxfrm1::header::get_encryption_salts(byte* salts, size_t salt_space, size_t* salt_size, size_t* salt_count) const
{
  if (salt_space < m_buffer.m_header.m_encrypt_key_definition_salt_size * m_buffer.m_header.m_encrypt_key_definition_salt_count)
    return -1;
  *salt_size = m_buffer.m_header.m_encrypt_key_definition_salt_size;
  *salt_count = m_buffer.m_header.m_encrypt_key_definition_salt_count;
  memcpy(salts,
         &m_buffer.m_octets[m_buffer.m_header.m_encrypt_key_definition_salts_position_in_octets],
         m_buffer.m_header.m_encrypt_key_definition_salt_size * m_buffer.m_header.m_encrypt_key_definition_salt_count);
  return 0;
}

int ndb_ndbxfrm1::trailer::set_data_size(Uint64 data_size)
{
  if (m_buffer.m_trailer.m_data_size != 0)
    return -1;
  m_buffer.m_trailer.m_data_size = data_size;
  return 0;
}

int ndb_ndbxfrm1::trailer::set_data_crc32(long crc32)
{
  if ((m_buffer.m_trailer.m_flags &
       fixed_trailer::flag_data_checksum_mask) != 0)
    return -1;
  m_buffer.m_trailer.m_data_checksum[0] = (crc32 & 0xff);
  m_buffer.m_trailer.m_data_checksum[1] = (crc32 & 0xff00) >> 8;
  m_buffer.m_trailer.m_data_checksum[2] = (crc32 & 0xff0000) >> 16;
  m_buffer.m_trailer.m_data_checksum[3] = (crc32 & 0xff000000) >> 24;
  m_buffer.m_trailer.m_flags |= fixed_trailer::flag_data_checksum_in_trailer |
                                fixed_trailer::flag_data_checksum_crc32;
  return 0;
}

int ndb_ndbxfrm1::trailer::get_data_size(Uint64* size) const
{
  *size = m_buffer.m_trailer.m_data_size;
  return 0;
}

int ndb_ndbxfrm1::trailer::get_data_crc32(Uint32* crc32) const
{
  if ((m_buffer.m_trailer.m_flags &
       fixed_trailer::flag_data_checksum_mask) != 0)
    return -1;
  *crc32 = m_buffer.m_trailer.m_data_checksum[0] |
           (Uint32(m_buffer.m_trailer.m_data_checksum[1]) << 8) |
           (Uint32(m_buffer.m_trailer.m_data_checksum[2]) << 16) |
           (Uint32(m_buffer.m_trailer.m_data_checksum[3]) << 24);
  return 0;
}

int ndb_ndbxfrm1::trailer::set_file_pos(off_t file_pos)
{
  m_file_pos = file_pos;
  return 0;
}

int ndb_ndbxfrm1::trailer::set_file_block_size(size_t file_block_size)
{
  m_file_block_size = file_block_size;
  return 0;
}

int ndb_ndbxfrm1::header::prepare_for_write()
{
  return 0;
}

size_t ndb_ndbxfrm1::header::get_size() const
{
  return m_buffer.m_header.m_magic.m_header_size;
}

int ndb_ndbxfrm1::header::write_header(ndbxfrm_output_iterator* out) const
{
  byte* buf = out->begin();
  size_t len = out->size();

  byte* p = buf;
  byte* endp = p + len;

  if (len < m_buffer.m_header.m_magic.m_header_size)
    RETURN(-1);
  p = buf;
  Uint32 avail = endp - p;
  if (avail < m_buffer.m_header.m_magic.m_fixed_header_size)
    RETURN(-1);
  size_t write_size = std::min(size_t(m_buffer.m_header.m_magic.m_fixed_header_size),
                               sizeof(ndb_ndbxfrm1::header::fixed_header));
  memcpy(p, &m_buffer.m_header, write_size);
  if (m_buffer.m_header.m_magic.m_fixed_header_size > sizeof(ndb_ndbxfrm1::header::fixed_header))
  {
    memset(p+write_size, 0, m_buffer.m_header.m_magic.m_fixed_header_size - write_size);
  }
  p += m_buffer.m_header.m_magic.m_fixed_header_size;
  avail = endp - p;
  if (avail < m_buffer.m_header.m_octets_size)
    RETURN(-1);
  memcpy(p, &m_buffer.m_octets, m_buffer.m_header.m_octets_size);
  p+= m_buffer.m_header.m_octets_size;
  Uint32 zero_pad_size = m_buffer.m_header.m_magic.m_header_size -
                         m_buffer.m_header.m_magic.m_fixed_header_size -
                         m_buffer.m_header.m_octets_size;
  memset(p, 0, zero_pad_size);
  p += zero_pad_size;

  out->advance(p - buf);
  return 0;
}

int ndb_ndbxfrm1::trailer::prepare_for_write()
{
  size_t trailer_size = m_buffer.m_trailer.m_magic.m_trailer_size;
  if (m_file_block_size > 0 &&
      (m_file_pos + trailer_size) % m_file_block_size != 0)
  {
    m_zero_pad_size = m_file_block_size -
                      (m_file_pos + trailer_size) % m_file_block_size;
    m_buffer.m_trailer.m_magic.m_trailer_size += m_zero_pad_size;
  }
  else
  {
    m_zero_pad_size = 0;
  }
  return 0;
}

size_t ndb_ndbxfrm1::trailer::get_size() const
{
  return m_buffer.m_trailer.m_magic.m_trailer_size;
}

int ndb_ndbxfrm1::trailer::write_trailer(ndbxfrm_output_iterator* out) const
{
  byte* buf = out->begin();
  size_t len = out->size();

  require(m_file_pos > 0);
  require(m_file_block_size > 0);

  byte* p = buf;

  size_t magic_size = sizeof(m_buffer.m_trailer.m_magic);
  size_t trailer_size = m_buffer.m_trailer.m_magic.m_trailer_size;
  size_t fixed_trailer_size = m_buffer.m_trailer.m_magic.m_fixed_trailer_size;
  require(trailer_size == m_zero_pad_size + fixed_trailer_size);
  require(trailer_size <= len);

  memset(p, 0, m_zero_pad_size);
  p += m_zero_pad_size;

  memcpy(p, &m_buffer.m_trailer, fixed_trailer_size - magic_size);
  p += fixed_trailer_size - magic_size;

  memcpy(p, &m_buffer.m_trailer.m_magic, magic_size);
  p += magic_size;

  out->advance(trailer_size);
  return 0;
}

int ndb_ndbxfrm1::header::prepare_header_for_write()
{
  memcpy(&m_buffer.m_header.m_magic.m_magic[0],
         ndb_ndbxfrm1::magic,
         sizeof(ndb_ndbxfrm1::magic));
  m_buffer.m_header.m_magic.m_endian = native_endian_marker;
  m_buffer.m_header.m_dbg_writer_ndb_version = NDB_VERSION_D;

  return 0;
}

int ndb_ndbxfrm1::trailer::prepare_trailer_for_write()
{
  memcpy(&m_buffer.m_trailer.m_magic.m_magic[0], ndb_ndbxfrm1::magic, sizeof(ndb_ndbxfrm1::magic));
  m_buffer.m_trailer.m_magic.m_endian = native_endian_marker;
  return 0;
}

int ndb_ndbxfrm1::header::validate_header() const
{
  return m_buffer.m_header.validate();
}

int ndb_ndbxfrm1::header::fixed_header::magic::validate() const
{
  if (memcmp(m_magic, ndb_ndbxfrm1::magic, sizeof(ndb_ndbxfrm1::magic)) != 0)
    RETURN(-1);
  if (m_endian != native_endian_marker)
    RETURN(-1);
  if (m_header_size > MAX_HEADER_SIZE + MAX_OCTETS_SIZE)
    RETURN(-1);
  if (m_fixed_header_size > MAX_HEADER_SIZE)
    RETURN(-1);
  if (!is_all_zeros(&m_zeros, sizeof(m_zeros)))
    RETURN(-1);
  return 0;
}

int ndb_ndbxfrm1::header::fixed_header::magic::toggle_endian()
{
  if (m_endian != reverse_endian_marker)
    RETURN(-1);

  toggle_endian64(&m_endian);
  toggle_endian32(&m_header_size);
  toggle_endian32(&m_fixed_header_size);

  return 0;
}

int ndb_ndbxfrm1::header::fixed_header::validate() const
{
  const bool compress = ((m_flags & flag_compress_mask) != 0);
  const bool encrypt = ((m_flags & flag_encrypt_mask) != 0);

  if (m_magic.validate() == -1)
    RETURN(-1);

  if ((m_flags & flag_zeros) != 0)
    RETURN(-1);

  if (m_dbg_writer_ndb_version != NDB_VERSION_D)
    RETURN(-1);

  if (m_octets_size > MAX_OCTETS_SIZE)
    RETURN(-1);

  if (m_magic.m_header_size < m_magic.m_fixed_header_size + m_octets_size)
    RETURN(-1);
 
  if (compress)
  {
    if ((m_flags & flag_compress_mask) != flag_compress_deflate)
      RETURN(-1);

    if (m_compress_dbg_writer_header_version.validate() == -1)
      RETURN(-1);
    if (m_compress_dbg_writer_library_version.validate() == -1)
      RETURN(-1);
  }
  else
  {
    if (!is_all_zeros(&m_compress_dbg_writer_header_version,
                      sizeof(m_compress_dbg_writer_header_version)))
      RETURN(-1);
    if (!is_all_zeros(&m_compress_dbg_writer_library_version,
                      sizeof(m_compress_dbg_writer_library_version)))
      RETURN(-1);
  }

  if (encrypt)
  {
    if (m_encrypt_dbg_writer_header_version.validate() == -1)
      RETURN(-1);
    if (m_encrypt_dbg_writer_library_version.validate() == -1)
      RETURN(-1);

    Uint64 salt_end = Uint64(m_encrypt_key_definition_salt_size) *
                      Uint64(m_encrypt_key_definition_salt_count) +
                      Uint64(m_encrypt_key_definition_salts_position_in_octets);
    if (salt_end > m_octets_size)
      RETURN(-1);

    switch (m_flags & flag_encrypt_cipher_mask)
    {
    case flag_encrypt_cipher_aes_256_cbc: break;
    case flag_encrypt_cipher_aes_256_xts: break;
    default:
      RETURN(-1);
    }

    switch (m_flags & flag_encrypt_kdf_mask)
    {
    case flag_encrypt_kdf_pbkdf2_sha256: break;
    default:
      RETURN(-1);
    }

    switch (m_flags & flag_encrypt_padding_mask)
    {
    case flag_encrypt_padding_none: break;
    case flag_encrypt_padding_pkcs: break;
    default:
      RETURN(-1);
    }

    switch (m_flags & flag_encrypt_key_reuse_mask)
    {
    case flag_encrypt_key_reuse_same:
      if (m_encrypt_key_definition_salt_count != 1)
        RETURN(-1);
    break;
    case flag_encrypt_key_reuse_pair:
    case flag_encrypt_key_reuse_mix_pair:
      if (m_encrypt_key_definition_salt_count == 0)
        RETURN(-1);
      if (m_encrypt_key_reuse_data_unit_size == 0)
        RETURN(-1);
      break;
    default:
      RETURN(-1);
    }
  }
  else
  {
    if (!is_all_zeros(&m_encrypt_dbg_writer_header_version,
                      sizeof(m_encrypt_dbg_writer_header_version)))
      RETURN(-1);
    if (!is_all_zeros(&m_encrypt_dbg_writer_library_version,
                      sizeof(m_encrypt_dbg_writer_library_version)))
      RETURN(-1);
    if (m_encrypt_key_definition_iterator_count != 0)
      RETURN(-1);
    if (m_encrypt_key_definition_salt_size != 0)
      RETURN(-1);
    if (m_encrypt_key_definition_salt_count != 0)
      RETURN(-1);
    if (m_encrypt_key_reuse_data_unit_size != 0)
      RETURN(-1);
    if (m_encrypt_key_definition_salts_position_in_octets != 0)
      RETURN(-1);
  }

  if (!is_all_zeros(&m_zeros01, sizeof(m_zeros01)))
    RETURN(-1);

  if (!is_all_zeros(&m_zeros02, sizeof(m_zeros02)))
    RETURN(-1);

  return 0;
}

int ndb_ndbxfrm1::trailer::fixed_trailer::validate() const
{
  if ((m_flags & flag_zeros) != 0)
    RETURN(-1);
  return 0;
}

int ndb_ndbxfrm1::trailer::fixed_trailer::toggle_endian()
{
  if (m_magic.toggle_endian() == -1)
    RETURN(-1);
  toggle_endian64(&m_flags);
  toggle_endian64(&m_data_size);
  return 0;
}

int ndb_ndbxfrm1::trailer::fixed_trailer::magic::toggle_endian()
{
  if (m_endian != reverse_endian_marker)
    RETURN(-1);
  toggle_endian64(&m_endian);
  toggle_endian32(&m_trailer_size);
  return 0;
}

int ndb_ndbxfrm1::header::transform_version::validate() const
{
  switch (m_flags & flag_product_mask)
  {
  case flag_product_zlib: break;
  case flag_product_OpenSSL: break;
  default: RETURN(-1);
  }
  switch (m_flags & flag_version_type_mask)
  {
  case flag_version_type_char: break;
  case flag_version_type_int32: break;
  default: RETURN(-1);
  }
  return 0;
}

int ndb_ndbxfrm1::header::fixed_header::toggle_endian()
{
  if (m_magic.toggle_endian() == -1)
    RETURN(-1);

  toggle_endian64(&m_flags);
  toggle_endian32(&m_dbg_writer_ndb_version);
  toggle_endian32(&m_octets_size);
  toggle_endian32(&m_file_block_size);
  toggle_endian32(&m_trailer_max_size);

  if (m_compress_dbg_writer_header_version.toggle_endian() == -1)
    RETURN(-1);
  if (m_compress_dbg_writer_library_version.toggle_endian() == -1)
    RETURN(-1);

  if (m_encrypt_dbg_writer_header_version.toggle_endian() == -1)
    RETURN(-1);
  if (m_encrypt_dbg_writer_library_version.toggle_endian() == -1)
    RETURN(-1);
  toggle_endian32(&m_encrypt_key_definition_iterator_count);
  toggle_endian32(&m_encrypt_key_definition_salt_size);
  toggle_endian32(&m_encrypt_key_definition_salt_count);
  toggle_endian32(&m_encrypt_key_reuse_data_unit_size);
  toggle_endian32(&m_encrypt_key_definition_salts_position_in_octets);
  return 0;
}

int ndb_ndbxfrm1::header::transform_version::toggle_endian()
{
  toggle_endian32(&m_flags);
  if ((m_flags & flag_version_type_mask) == flag_version_type_int32)
    toggle_endian32(m_int32, 3);
  return 0;
}

int ndb_ndbxfrm1::trailer::fixed_trailer::magic::validate() const
{
  if (memcmp(m_magic, ndb_ndbxfrm1::magic, sizeof(ndb_ndbxfrm1::magic)) != 0)
    RETURN(-1);
  if (m_endian != native_endian_marker)
    RETURN(-1);
  // m_trailer_size
  if (!is_all_zeros(&m_zeros, sizeof(m_zeros)))
    RETURN(-1);
  return 0;
}

int ndb_ndbxfrm1::trailer::read_trailer(ndbxfrm_input_reverse_iterator* in)
{
  const byte* buf = in->cend();
  size_t len = in->size();

  memset(&m_buffer.m_trailer, 0, sizeof(m_buffer.m_trailer));

  const byte* p = buf;
  const byte* endp = p + len;

  if (len < sizeof(fixed_trailer::magic))
    RETURN(-1);
  const fixed_trailer::magic* magicp =
    reinterpret_cast<const fixed_trailer::magic*>(
                                        endp - sizeof(fixed_trailer::magic));

  bool detect_toggle_endian = (magicp->m_endian != native_endian_marker);
  Uint32 trailer_size = magicp->m_trailer_size;
  Uint32 fixed_trailer_size = magicp->m_fixed_trailer_size;
  if (detect_toggle_endian)
  {
    if (magicp->m_endian != reverse_endian_marker)
      RETURN(-1);
    toggle_endian32(&trailer_size);
    toggle_endian32(&fixed_trailer_size);
  }
  if (trailer_size > len)
    RETURN(-1);
  if (fixed_trailer_size > trailer_size)
    RETURN(-1);
  if (fixed_trailer_size < sizeof(fixed_trailer::magic))
    RETURN(-1);
  const fixed_trailer* trailerp = reinterpret_cast<const fixed_trailer*>(endp - fixed_trailer_size);

  Uint32 copy_size = fixed_trailer_size - sizeof(fixed_trailer::magic);
  const byte* pt = reinterpret_cast<const byte*>(trailerp);
  while (copy_size > 0 && pt[copy_size - 1] != 0)
  {
    copy_size--;
  }
  if (copy_size > sizeof(fixed_trailer) - sizeof(fixed_trailer::magic))
    RETURN(-1);
  memcpy(&m_buffer.m_trailer.m_magic, magicp, sizeof(fixed_trailer::magic));
  memcpy(&m_buffer.m_trailer, trailerp, copy_size);
  if (detect_toggle_endian)
    m_buffer.m_trailer.toggle_endian(); 

  Uint32 zero_pad_size = trailer_size - fixed_trailer_size;
  pt = pt - zero_pad_size;
  for (Uint32 i = 0; i < zero_pad_size; i++)
    if (pt[i] != 0)
      RETURN(-1);

  in->advance(endp - pt);
  return 0;
}

int ndb_ndbxfrm1::trailer::validate_trailer() const
{
  if (m_buffer.m_trailer.m_magic.validate() == -1)
    RETURN(-1);
  if (m_buffer.m_trailer.validate() == -1)
    RETURN(-1);
  return 0;
}
