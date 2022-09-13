/* Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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

#include <algorithm>
#include <stdio.h>
#include "my_byteorder.h" // WORDS_BIGENDIAN
#include "my_compiler.h" // unlikely
#include "util/ndb_ndbxfrm1.h"
#include "util/require.h"

#ifndef REQUIRE
#define REQUIRE(r) \
do \
{ \
  if (unlikely(!(r))) \
  { \
    fprintf(stderr, "\nYYY: %s: %u: %s: r = %d\n", __FILE__, __LINE__, \
             __func__, (r)); \
    require((r)); \
  } \
} while (0)
#endif
//#define RETURN(rv) REQUIRE(rv)
#define RETURN(rv) return(rv)

const char ndb_ndbxfrm1::magic[8] = {'N', 'D', 'B', 'X', 'F', 'R', 'M', '1'};

ndb_ndbxfrm1::header::header() : m_zero_pad_size(0)
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
 * -2 - bad magic
 * -1 - ok magic, but other parts of header is bad
 *  0 - ok magic, store needed data in len
 *  1 - need more data,
 */
int ndb_ndbxfrm1::header::detect_header(const ndbxfrm_input_iterator* in,
                                        size_t* header_size_ptr)
{
  const byte* buf = in->cbegin();
  size_t len = in->size();

  if (len < 8)
    return need_more_input;
  if (memcmp(buf, magic, 8) != 0)
    return -2;

  if (len < sizeof(fixed_header::magic))
    return need_more_input;
  const fixed_header::magic* mh =
    reinterpret_cast<const fixed_header::magic*>(buf);

  bool toggle_endian = (mh->m_endian != native_endian_marker);
  if (toggle_endian && (mh->m_endian != reverse_endian_marker))
    RETURN(-1); // Bad endian marker
  Uint32 header_size = mh->m_header_size;
  if (toggle_endian) toggle_endian32(&header_size);
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
  while (copy_size > 0 && p[copy_size - 1] == 0)
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
  m_zero_pad_size = zero_pad_size;

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
  if ((m_buffer.m_header.m_flags & fixed_header::flag_compress_method_mask)
      != 0)
    RETURN(-1);  // already set

  switch (method)
  {
    case compression_deflate:
      m_buffer.m_header.m_flags |= fixed_header::flag_compress_method_deflate;
      break;
    default:
      RETURN(-1);
  }
  return 0;
}

int ndb_ndbxfrm1::header::set_compression_padding(unsigned int padding)
{
  if ((m_buffer.m_header.m_flags & fixed_header::flag_compress_padding_mask)
      != 0)
    RETURN(-1);  // already set

  switch (padding)
  {
    case padding_pkcs:
      m_buffer.m_header.m_flags |= fixed_header::flag_compress_padding_pkcs;
      break;
    default:
      RETURN(-1);
  }
  return 0;
}

int ndb_ndbxfrm1::header::get_compression_method() const
{
  switch (m_buffer.m_header.m_flags & fixed_header::flag_compress_method_mask)
  {
  case 0: return 0;
  case fixed_header::flag_compress_method_deflate: return compression_deflate;
  default:
    RETURN(-1);
  }
}

int ndb_ndbxfrm1::header::get_compression_padding() const
{
  switch (m_buffer.m_header.m_flags & fixed_header::flag_compress_padding_mask)
  {
  case 0: return 0;
  case fixed_header::flag_compress_padding_pkcs: return padding_pkcs;
  default:
    RETURN(-1);
  }
}

int ndb_ndbxfrm1::header::set_encryption_cipher(Uint32 cipher)
{
  if ((m_buffer.m_header.m_flags & fixed_header::flag_encrypt_cipher_mask) != 0)
    RETURN(-1);  // already set

  switch (cipher)
  {
    case 0:
      break;
    case cipher_cbc:
      m_buffer.m_header.m_flags |=
          fixed_header::flag_encrypt_cipher_aes_256_cbc;
      break;
    case cipher_xts:
      m_buffer.m_header.m_flags |=
          fixed_header::flag_encrypt_cipher_aes_256_xts;
      break;
    default:
      RETURN(-1);
  }
  return 0;
}

int ndb_ndbxfrm1::header::set_encryption_padding(Uint32 padding)
{
  if ((m_buffer.m_header.m_flags & fixed_header::flag_encrypt_padding_mask) !=
      0)
    RETURN(-1);  // already set

  switch (padding)
  {
    case 0:
      break;
    case padding_pkcs:
      m_buffer.m_header.m_flags |= fixed_header::flag_encrypt_padding_pkcs;
      break;
    default:
      RETURN(-1);
  }
  return 0;
}

int ndb_ndbxfrm1::header::set_encryption_krm(Uint32 krm)
{
  if ((m_buffer.m_header.m_flags & fixed_header::flag_encrypt_krm_mask) != 0)
    RETURN(-1);  // already set

  switch (krm)
  {
    case 0:
      break;
    case krm_pbkdf2_sha256:
      m_buffer.m_header.m_flags |= fixed_header::flag_encrypt_krm_pbkdf2_sha256;
      break;
    case krm_aeskw_256:
      m_buffer.m_header.m_flags |= fixed_header::flag_encrypt_krm_aeskw_256;
      break;
    default:
      RETURN(-1);
  }
  return 0;
}

int ndb_ndbxfrm1::header::set_encryption_krm_kdf_iter_count(Uint32 count)
{
  if (m_buffer.m_header.m_encrypt_krm_kdf_iterator_count != 0)
    RETURN(-1);  // already set

  if (count == 0) RETURN(-1);

  m_buffer.m_header.m_encrypt_krm_kdf_iterator_count = count;
  return 0;
}

int ndb_ndbxfrm1::header::set_encryption_key_selection_mode(Uint32 key_selection_mode,
                                                            Uint32 key_data_unit_size)
{
  if ((m_buffer.m_header.m_flags & fixed_header::flag_encrypt_key_selection_mode_mask) !=
      0)
    RETURN(-1);  // already set

  if (m_buffer.m_header.m_encrypt_key_data_unit_size != 0)
    RETURN(-1);  // already set

  switch (key_selection_mode)
  {
    case key_selection_mode_same:
      m_buffer.m_header.m_flags |= fixed_header::flag_encrypt_key_selection_mode_same;
      break;
    case key_selection_mode_pair:
      m_buffer.m_header.m_flags |= fixed_header::flag_encrypt_key_selection_mode_pair;
      break;
    case key_selection_mode_mix_pair:
      m_buffer.m_header.m_flags |=
          fixed_header::flag_encrypt_key_selection_mode_mix_pair;
      break;
    default:
      RETURN(-1);
  }

  m_buffer.m_header.m_encrypt_key_data_unit_size = key_data_unit_size;
  return 0;
}

int ndb_ndbxfrm1::header::set_encryption_keying_material(
    const byte* keying_material,
    size_t keying_material_size,
    size_t keying_material_count)
{
  if (m_buffer.m_header.m_encrypt_krm_keying_material_position_in_octets != 0)
    RETURN(-1);

  if (keying_material == nullptr) RETURN(-1);

  if (keying_material_size == 0) RETURN(-1);

  if (keying_material_count == 0) RETURN(-1);

  size_t material_octets_size = keying_material_size * keying_material_count;
  if (m_buffer.m_header.m_octets_size + material_octets_size > MAX_OCTETS_SIZE)
    RETURN(-1);

  m_buffer.m_header.m_encrypt_krm_keying_material_position_in_octets =
      m_buffer.m_header.m_octets_size;
  m_buffer.m_header.m_encrypt_krm_keying_material_size = keying_material_size;
  m_buffer.m_header.m_encrypt_krm_keying_material_count = keying_material_count;
  memcpy(m_buffer.m_octets + m_buffer.m_header.m_octets_size,
         keying_material,
         material_octets_size);
  m_buffer.m_header.m_octets_size += material_octets_size;
  m_buffer.m_header.m_magic.m_header_size += material_octets_size;
  return 0;
}

int ndb_ndbxfrm1::header::get_encryption_cipher(Uint32* cipher) const
{
  if (cipher == nullptr) RETURN(-1);

  switch (m_buffer.m_header.m_flags & fixed_header::flag_encrypt_cipher_mask)
  {
    case 0: *cipher = 0; return 0;
    case fixed_header::flag_encrypt_cipher_aes_256_cbc:
      *cipher = cipher_cbc;
      return 0;
    case fixed_header::flag_encrypt_cipher_aes_256_xts:
      *cipher = cipher_xts;
      return 0;
    default:
      RETURN(-1);
  }
}

int ndb_ndbxfrm1::header::get_encryption_padding(Uint32* padding) const
{
  if (padding == nullptr) RETURN(-1);

  switch (m_buffer.m_header.m_flags & fixed_header::flag_encrypt_padding_mask)
  {
    case 0: *padding = 0; return 0;
    case fixed_header::flag_encrypt_padding_pkcs:
      *padding = padding_pkcs;
      return 0;
    default:
      RETURN(-1);
  }
}

int ndb_ndbxfrm1::header::get_encryption_krm(Uint32* krm) const
{
  if (krm == nullptr) RETURN(-1);

  switch (m_buffer.m_header.m_flags & fixed_header::flag_encrypt_krm_mask)
  {
    case 0:
      *krm = 0;
      return 0;
    case fixed_header::flag_encrypt_krm_pbkdf2_sha256:
      *krm = krm_pbkdf2_sha256;
      return 0;
    case fixed_header::flag_encrypt_krm_aeskw_256:
      *krm = krm_aeskw_256;
      return 0;
    default:
      RETURN(-1);
  }
}

int ndb_ndbxfrm1::header::get_encryption_krm_kdf_iter_count(Uint32* count) const
{
  if (count == nullptr) RETURN(-1);

  *count = m_buffer.m_header.m_encrypt_krm_kdf_iterator_count;
  return 0;
}

int ndb_ndbxfrm1::header::get_encryption_key_selection_mode(
    Uint32* key_selection_mode, Uint32* key_data_unit_size) const
{
  if (key_selection_mode == nullptr) RETURN(-1);
  if (key_data_unit_size == nullptr) RETURN(-1);

  switch (m_buffer.m_header.m_flags & fixed_header::flag_encrypt_key_selection_mode_mask)
  {
    case fixed_header::flag_encrypt_key_selection_mode_same:
      *key_selection_mode = key_selection_mode_same;
      break;
    case fixed_header::flag_encrypt_key_selection_mode_pair:
      *key_selection_mode = key_selection_mode_pair;
      break;
    case fixed_header::flag_encrypt_key_selection_mode_mix_pair:
      *key_selection_mode = key_selection_mode_mix_pair;
      break;
    default:
      RETURN(-1);
  }
  *key_data_unit_size = m_buffer.m_header.m_encrypt_key_data_unit_size;
  return 0;
}

int ndb_ndbxfrm1::header::get_encryption_keying_material(
    byte* keying_material,
    size_t material_space,
    size_t* keying_material_size,
    size_t* keying_material_count) const
{
  if (keying_material == nullptr) RETURN(-1);
  if (keying_material_size == nullptr) RETURN(-1);
  if (keying_material_count == nullptr) RETURN(-1);

  const size_t material_octets_size =
      m_buffer.m_header.m_encrypt_krm_keying_material_size *
      m_buffer.m_header.m_encrypt_krm_keying_material_count;

  if (material_space < material_octets_size) RETURN(-1);

  *keying_material_size = m_buffer.m_header.m_encrypt_krm_keying_material_size;
  *keying_material_count =
      m_buffer.m_header.m_encrypt_krm_keying_material_count;
  memcpy(
      keying_material,
      &m_buffer.m_octets[m_buffer.m_header
                             .m_encrypt_krm_keying_material_position_in_octets],
      material_octets_size);
  return 0;
}

int ndb_ndbxfrm1::trailer::set_data_size(Uint64 data_size)
{
  if (m_buffer.m_trailer.m_data_size != 0) RETURN(-1);
  m_buffer.m_trailer.m_data_size = data_size;
  return 0;
}

int ndb_ndbxfrm1::trailer::set_data_crc32(long crc32)
{
  if ((m_buffer.m_trailer.m_flags & fixed_trailer::flag_data_checksum_mask) !=
      0)
    RETURN(-1);
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
  if (size == nullptr) RETURN(-1);

  *size = m_buffer.m_trailer.m_data_size;
  return 0;
}

int ndb_ndbxfrm1::trailer::get_data_crc32(Uint32* crc32) const
{
  if (crc32 == nullptr) RETURN(-1);

  if ((m_buffer.m_trailer.m_flags & fixed_trailer::flag_data_checksum_mask) !=
      (fixed_trailer::flag_data_checksum_in_trailer |
       fixed_trailer::flag_data_checksum_crc32))
    return -1;  // RETURN(-1);
  *crc32 = m_buffer.m_trailer.m_data_checksum[0] |
           (Uint32(m_buffer.m_trailer.m_data_checksum[1]) << 8) |
           (Uint32(m_buffer.m_trailer.m_data_checksum[2]) << 16) |
           (Uint32(m_buffer.m_trailer.m_data_checksum[3]) << 24);
  return 0;
}

int ndb_ndbxfrm1::trailer::set_file_pos(ndb_off_t file_pos)
{
  if (m_file_pos != 0) RETURN(-1);

  m_file_pos = file_pos;
  return 0;
}

int ndb_ndbxfrm1::trailer::set_file_block_size(size_t file_block_size)
{
  if (m_file_block_size != 0) RETURN(-1);

  m_file_block_size = file_block_size;
  return 0;
}

int ndb_ndbxfrm1::header::prepare_for_write(Uint32 header_size)
{
  Uint32 zero_pad_size = 0;
  const Uint32 file_block_size =
      header_size != 0 ? m_buffer.m_header.m_file_block_size : 0;

  const Uint32 header_size_need =
      m_buffer.m_header.m_magic.m_fixed_header_size +
      m_buffer.m_header.m_octets_size;
  if (header_size == 0)
  {
    header_size = header_size_need;
    if (file_block_size > 0 && header_size % file_block_size != 0)
    {
      zero_pad_size = file_block_size - header_size % file_block_size;
      header_size += zero_pad_size;
    }
  }
  else
  {
    if (file_block_size > 0 && (header_size % file_block_size != 0))
    {
      RETURN(-1);
    }
    if (header_size < header_size_need)
    {
      RETURN(-1);
    }
    zero_pad_size = header_size - header_size_need;
  }
  m_buffer.m_header.m_magic.m_header_size = header_size;
  m_zero_pad_size = zero_pad_size;
  return 0;
}

size_t ndb_ndbxfrm1::header::get_size() const
{
  return m_buffer.m_header.m_magic.m_header_size;
}

int ndb_ndbxfrm1::header::write_header(ndbxfrm_output_iterator* out) const
{
  Uint32 cipher = 0;
  require(get_compression_method() ||
          (get_encryption_cipher(&cipher) == 0 && cipher != 0));
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
  size_t write_size =
      std::min(size_t{m_buffer.m_header.m_magic.m_fixed_header_size},
               sizeof(ndb_ndbxfrm1::header::fixed_header));
  memcpy(p, &m_buffer.m_header, write_size);
  if (m_buffer.m_header.m_magic.m_fixed_header_size >
      sizeof(ndb_ndbxfrm1::header::fixed_header))
  {
    memset(p + write_size, 0,
           m_buffer.m_header.m_magic.m_fixed_header_size - write_size);
  }
  p += m_buffer.m_header.m_magic.m_fixed_header_size;
  avail = endp - p;
  if (avail < m_buffer.m_header.m_octets_size)
    RETURN(-1);
  memcpy(p, &m_buffer.m_octets, m_buffer.m_header.m_octets_size);
  p+= m_buffer.m_header.m_octets_size;
  if (m_buffer.m_header.m_magic.m_header_size !=
      m_buffer.m_header.m_magic.m_fixed_header_size +
          m_buffer.m_header.m_octets_size + m_zero_pad_size)
    RETURN(-1);
  memset(p, 0, m_zero_pad_size);
  p += m_zero_pad_size;

  out->advance(p - buf);
  return 0;
}

int ndb_ndbxfrm1::trailer::prepare_for_write(Uint32 trailer_size)
{
  Uint32 zero_pad_size = 0;
  if (trailer_size == 0)
  {
    trailer_size = m_buffer.m_trailer.m_magic.m_fixed_trailer_size;
    if (m_file_block_size > 0 &&
        (m_file_pos + trailer_size) % m_file_block_size != 0)
    {
      zero_pad_size =
          m_file_block_size - (m_file_pos + trailer_size) % m_file_block_size;
      trailer_size += zero_pad_size;
    }
  }
  else
  {
    if (m_file_block_size > 0 && (trailer_size % m_file_block_size != 0))
    {
      RETURN(-1);
    }
    if (m_file_block_size > 0 && (m_file_pos % m_file_block_size != 0))
    {
      RETURN(-1);
    }
    if (trailer_size < m_buffer.m_trailer.m_magic.m_fixed_trailer_size)
    {
      RETURN(-1);
    }
    zero_pad_size =
        trailer_size - m_buffer.m_trailer.m_magic.m_fixed_trailer_size;
  }
  m_buffer.m_trailer.m_magic.m_trailer_size = trailer_size;
  m_zero_pad_size = zero_pad_size;
  return 0;
}

size_t ndb_ndbxfrm1::trailer::get_size() const
{
  return m_buffer.m_trailer.m_magic.m_trailer_size;
}

int ndb_ndbxfrm1::trailer::write_trailer(ndbxfrm_output_iterator* out,
                                         ndbxfrm_output_iterator* extra) const
{
  /*
   * Trailer could span over two file blocks and typically the output buffer is
   * only one file block big.
   *
   * First will the out buffer be filled, then if needed the extra buffer.
   */
  byte* buf = out->begin();
  size_t len = out->size() + (extra == nullptr ? 0 : extra->size());

  if (m_file_pos == 0) RETURN(-1);

  if (m_file_block_size == 0) RETURN(-1);

  byte* p = buf;

  size_t magic_size = sizeof(m_buffer.m_trailer.m_magic);
  size_t trailer_size = m_buffer.m_trailer.m_magic.m_trailer_size;
  size_t fixed_trailer_size = m_buffer.m_trailer.m_magic.m_fixed_trailer_size;

  if (trailer_size != m_zero_pad_size + fixed_trailer_size) RETURN(-1);

  if (trailer_size > len) RETURN(-1);

  /*
   * First fill the out buffer by try copying the three parts, zero padding +
   * tailer + magic, respecting the amount out output space remaining in each
   * copy.
   */
  size_t l = std::min(out->size(), m_zero_pad_size);
  memset(p, 0, l);
  out->advance(l);
  p += l;

  l = std::min(out->size(), fixed_trailer_size - magic_size);
  memcpy(p, &m_buffer.m_trailer, l);
  out->advance(l);
  p += l;

  l = std::min(out->size(), magic_size);
  memcpy(p, &m_buffer.m_trailer.m_magic, l);
  out->advance(l);
  p += l;

  /*
   * If not all parts managed to be copied, copy the rest into the extra buffer.
   * All three parts are copied, but without copying the part already in out
   * buffer.
   */
  if (p - buf < (ptrdiff_t)trailer_size)
  { // Fill extra
    require(extra != nullptr);
    size_t off = p - buf;
    require(len - off <= extra->size());

    p = extra->begin();

    // Fill extra with zero padding not copied to out buffer.
    if (off < m_zero_pad_size)
    {
      l = m_zero_pad_size - off;
      memset(p, 0, l);
      extra->advance(l);
      p += l;
      off = 0;
    }
    else off -= m_zero_pad_size;

    // Fill extra with trailer not copied to out buffer.
    l = fixed_trailer_size - magic_size;
    if (off < l)
    {
      memcpy(p, &m_buffer.m_trailer + off , l - off);
      extra->advance(l - off);
      p += l - off;
      off = 0;
    }
    else off -= l;

    // Fill extra with magic not copied to out buffer.
    l = magic_size;
    if (off < l)
    {
      memcpy(p, &m_buffer.m_trailer.m_magic + off, l - off);
      extra->advance(l - off);
      p += l - off;
      off = 0;
    }
    else off -= l;
    require(off == 0);
  }

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
  memcpy(&m_buffer.m_trailer.m_magic.m_magic[0], ndb_ndbxfrm1::magic,
         sizeof(ndb_ndbxfrm1::magic));
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
    if ((m_flags & flag_compress_method_mask) != flag_compress_method_deflate)
      RETURN(-1);

    if ((m_flags & flag_compress_padding_mask) != flag_compress_padding_none &&
        (m_flags & flag_compress_padding_mask) != flag_compress_padding_pkcs)
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

    Uint64 material_end = Uint64{m_encrypt_krm_keying_material_size} *
                          Uint64{m_encrypt_krm_keying_material_count} +
                      m_encrypt_krm_keying_material_position_in_octets;
    if (material_end > m_octets_size)
      RETURN(-1);

    switch (m_flags & flag_encrypt_cipher_mask)
    {
    case flag_encrypt_cipher_aes_256_cbc: break;
    case flag_encrypt_cipher_aes_256_xts: break;
    default:
      RETURN(-1);
    }

    switch (m_flags & flag_encrypt_krm_mask)
    {
      case flag_encrypt_krm_pbkdf2_sha256:
        if (m_encrypt_krm_kdf_iterator_count == 0 ||
            m_encrypt_krm_keying_material_size == 0 ||
            m_encrypt_krm_keying_material_count == 0 ||
            m_encrypt_krm_key_count != 0)
          RETURN(-1);
        break;
      case flag_encrypt_krm_aeskw_256:
        if (m_encrypt_krm_kdf_iterator_count != 0 ||
            m_encrypt_krm_keying_material_size == 0 ||
            m_encrypt_krm_keying_material_count == 0 ||
            m_encrypt_krm_key_count == 0)
          RETURN(-1);
        break;
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

    switch (m_flags & flag_encrypt_key_selection_mode_mask)
    {
    case flag_encrypt_key_selection_mode_same:
      if (m_encrypt_krm_keying_material_count != 1) RETURN(-1);
      break;
    case flag_encrypt_key_selection_mode_pair:
    case flag_encrypt_key_selection_mode_mix_pair:
      if (m_encrypt_krm_keying_material_count == 0) RETURN(-1);
      if (m_encrypt_key_data_unit_size == 0)
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
    if (m_encrypt_krm_kdf_iterator_count != 0) RETURN(-1);
    if (m_encrypt_krm_keying_material_size != 0) RETURN(-1);
    if (m_encrypt_krm_keying_material_count != 0) RETURN(-1);
    if (m_encrypt_krm_key_count != 0) RETURN(-1);
    if (m_encrypt_key_data_unit_size != 0)
      RETURN(-1);
    if (m_encrypt_krm_keying_material_position_in_octets != 0) RETURN(-1);
  }

  if (!is_all_zeros(&m_zeros01, sizeof(m_zeros01)))
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
  static_assert(sizeof(ndb_ndbxfrm1::trailer::fixed_trailer) == 56,
   "Remember update ndb_ndbxfrm1::trailer::fixed_trailer::toggle_endian() when "
   "adding new fields.");
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
  toggle_endian32(&m_encrypt_krm_kdf_iterator_count);
  toggle_endian32(&m_encrypt_krm_keying_material_size);
  toggle_endian32(&m_encrypt_krm_keying_material_count);
  toggle_endian32(&m_encrypt_key_data_unit_size);
  toggle_endian32(&m_encrypt_krm_keying_material_position_in_octets);
  toggle_endian32(&m_encrypt_krm_key_count);
  static_assert(sizeof(ndb_ndbxfrm1::header::fixed_header) == 160,
   "Remember update ndb_ndbxfrm1::header::fixed_header::toggle_endian() when "
   "adding new fields.");
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
  if (fixed_trailer_size > len)
    RETURN(-1);
  if (fixed_trailer_size < sizeof(fixed_trailer::magic))
    RETURN(-1);
  const fixed_trailer* trailerp =
      reinterpret_cast<const fixed_trailer*>(endp - fixed_trailer_size);

  Uint32 copy_size = fixed_trailer_size - sizeof(fixed_trailer::magic);
  const byte* pt = reinterpret_cast<const byte*>(trailerp);
  while (copy_size > 0 && pt[copy_size - 1] == 0)
  {
    copy_size--;
  }
  if (copy_size > sizeof(fixed_trailer) - sizeof(fixed_trailer::magic))
    RETURN(-1);
  memcpy(&m_buffer.m_trailer.m_magic, magicp, sizeof(fixed_trailer::magic));
  memcpy(&m_buffer.m_trailer, trailerp, copy_size);
  if (detect_toggle_endian)
    m_buffer.m_trailer.toggle_endian(); 

  // skip check of zero-padding in previous block
  Uint32 zero_pad_size =
      std::min(size_t{trailer_size}, len) - fixed_trailer_size;
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

// printers

void ndb_ndbxfrm1::header::printf(FILE* out) const
{
  if (out == nullptr)
    return;
    
  auto fixed_header = m_buffer.m_header;
  fprintf(out, "header: {\n");
  fprintf(out, "  fixed_header: {\n");
  fprintf(out, "    magic: {\n");
  fprintf(out, "      magic: { %u, %u, %u, %u, %u, %u, %u, %u },\n",
          fixed_header.m_magic.m_magic[0], fixed_header.m_magic.m_magic[1],
          fixed_header.m_magic.m_magic[2], fixed_header.m_magic.m_magic[3],
          fixed_header.m_magic.m_magic[4], fixed_header.m_magic.m_magic[5],
          fixed_header.m_magic.m_magic[6], fixed_header.m_magic.m_magic[7]);
  fprintf(out, "      endian: %llu,\n", fixed_header.m_magic.m_endian);
  fprintf(out, "      header_size: %u,\n", fixed_header.m_magic.m_header_size);
  fprintf(out, "      fixed_header_size: %u,\n",
          fixed_header.m_magic.m_fixed_header_size);
  fprintf(out, "      zeros: { %u, %u }\n", fixed_header.m_magic.m_zeros[0],
          fixed_header.m_magic.m_zeros[1]);
  fprintf(out, "    },\n");
  fprintf(out, "    flags: %llu,\n", fixed_header.m_flags);
  fprintf(out, "    flag_extended: %llu,\n",
          (fixed_header.m_flags & fixed_header::flag_extended));
  fprintf(out, "    flag_zeros: %llu,\n",
          (fixed_header.m_flags & fixed_header::flag_zeros));
  fprintf(out, "    flag_file_checksum: %llu,\n",
          (fixed_header.m_flags & fixed_header::flag_file_checksum_mask));
  fprintf(out, "    flag_data_checksum: %llu,\n",
          (fixed_header.m_flags & fixed_header::flag_data_checksum_mask) >> 4);
  fprintf(out, "    flag_compress: %llu,\n",
          (fixed_header.m_flags & fixed_header::flag_compress_mask) >> 8);
  fprintf(
      out, "    flag_compress_method: %llu,\n",
      (fixed_header.m_flags & fixed_header::flag_compress_method_mask) >> 8);
  fprintf(
      out, "    flag_compress_padding: %llu,\n",
      (fixed_header.m_flags & fixed_header::flag_compress_padding_mask) >> 28);
  fprintf(out, "    flag_encrypt: %llu,\n",
          (fixed_header.m_flags & fixed_header::flag_encrypt_mask) >> 12);
  fprintf(
      out, "    flag_encrypt_cipher: %llu,\n",
      (fixed_header.m_flags & fixed_header::flag_encrypt_cipher_mask) >> 12);
  fprintf(out,
          "    flag_encrypt_krm: %llu,\n",
          (fixed_header.m_flags & fixed_header::flag_encrypt_krm_mask) >> 16);
  fprintf(
      out, "    flag_encrypt_padding: %llu,\n",
      (fixed_header.m_flags & fixed_header::flag_encrypt_padding_mask) >> 20);
  fprintf(
      out, "    flag_encrypt_key_selection_mode: %llu,\n",
      (fixed_header.m_flags & fixed_header::flag_encrypt_key_selection_mode_mask) >> 24);
  fprintf(out, "    dbg_writer_ndb_version: %u,\n",
          fixed_header.m_dbg_writer_ndb_version);
  fprintf(out, "    octets_size: %u,\n", fixed_header.m_octets_size);
  fprintf(out, "    file_block_size: %u,\n", fixed_header.m_file_block_size);
  fprintf(out, "    trailer_max_size: %u,\n", fixed_header.m_trailer_max_size);
  fprintf(out, "    file_checksum: { %u, %u, %u, %u },\n",
          fixed_header.m_file_checksum[0], fixed_header.m_file_checksum[1],
          fixed_header.m_file_checksum[2], fixed_header.m_file_checksum[3]);
  fprintf(out, "    data_checksum: { %u, %u, %u, %u },\n",
          fixed_header.m_data_checksum[0], fixed_header.m_data_checksum[1],
          fixed_header.m_data_checksum[2], fixed_header.m_data_checksum[3]);
  fprintf(out, "    zeros01: { %u },\n", fixed_header.m_zeros01[0]);
  fprintf(out, "    compress_dbg_writer_header_version: { ... },\n");
  fprintf(out, "    compress_dbg_writer_library_version: { ... },\n");
  fprintf(out, "    encrypt_dbg_writer_header_version: { ... },\n");
  fprintf(out, "    encrypt_dbg_writer_library_version: { ... },\n");
  fprintf(out,
          "    encrypt_key_definition_iterator_count: %u,\n",
          fixed_header.m_encrypt_krm_kdf_iterator_count);
  fprintf(out,
          "    encrypt_krm_keying_material_size: %u,\n",
          fixed_header.m_encrypt_krm_keying_material_size);
  fprintf(out,
          "    encrypt_krm_keying_material_count: %u,\n",
          fixed_header.m_encrypt_krm_keying_material_count);
  fprintf(out, "    encrypt_key_data_unit_size: %u,\n",
          fixed_header.m_encrypt_key_data_unit_size);
  fprintf(out,
          "    encrypt_krm_keying_material_position_in_octets: %u,\n",
          fixed_header.m_encrypt_krm_keying_material_position_in_octets);
  fprintf(out, "  },\n");
  fprintf(out, "  octets: {\n");
  for (unsigned i = 0; i < fixed_header.m_octets_size; i++)
  {
    if (i % 16 == 0) fprintf(out, "    ");
    fprintf(out, " %u,", m_buffer.m_octets[i]);
    if (i % 16 == 15) fprintf(out, "\n");
  }
  if (fixed_header.m_octets_size % 16 != 0) fprintf(out, "\n");
  fprintf(out, "  }\n");
  fprintf(out, "}\n");
}

void ndb_ndbxfrm1::trailer::printf(FILE* out) const
{
  if (out == nullptr)
    return;
    
  auto fixed_trailer = m_buffer.m_trailer;
  fprintf(out, "trailer: {\n");
  fprintf(out, "  fixed_trailer: {\n");
  fprintf(out, "    flags: %llu,\n", fixed_trailer.m_flags);
  fprintf(out, "    flag_extended: %llu,\n",
          (fixed_trailer.m_flags & fixed_trailer::flag_extended));
  fprintf(out, "    flag_zeros: %llu,\n",
          (fixed_trailer.m_flags & fixed_trailer::flag_zeros));
  fprintf(out, "    flag_file_checksum: %llu,\n",
          (fixed_trailer.m_flags & fixed_trailer::flag_file_checksum_mask));
  fprintf(
      out, "    flag_data_checksum: %llu,\n",
      (fixed_trailer.m_flags & fixed_trailer::flag_data_checksum_mask) >> 4);
  fprintf(out, "    data_size: %llu,\n", fixed_trailer.m_data_size);
  fprintf(out, "    file_checksum: { %u, %u, %u, %u },\n",
          fixed_trailer.m_file_checksum[0], fixed_trailer.m_file_checksum[1],
          fixed_trailer.m_file_checksum[2], fixed_trailer.m_file_checksum[3]);
  fprintf(out, "    data_checksum: { %u, %u, %u, %u },\n",
          fixed_trailer.m_data_checksum[0], fixed_trailer.m_data_checksum[1],
          fixed_trailer.m_data_checksum[2], fixed_trailer.m_data_checksum[3]);
  fprintf(out, "    magic: {\n");
  fprintf(out, "      zeros: { %u, %u }\n", fixed_trailer.m_magic.m_zeros[0],
          fixed_trailer.m_magic.m_zeros[1]);
  fprintf(out, "      fixed_trailer_size: %u,\n",
          fixed_trailer.m_magic.m_fixed_trailer_size);
  fprintf(out, "      trailer_size: %u,\n",
          fixed_trailer.m_magic.m_trailer_size);
  fprintf(out, "      endian: %llu,\n", fixed_trailer.m_magic.m_endian);
  fprintf(out, "      magic: { %u, %u, %u, %u, %u, %u, %u, %u },\n",
          fixed_trailer.m_magic.m_magic[0], fixed_trailer.m_magic.m_magic[1],
          fixed_trailer.m_magic.m_magic[2], fixed_trailer.m_magic.m_magic[3],
          fixed_trailer.m_magic.m_magic[4], fixed_trailer.m_magic.m_magic[5],
          fixed_trailer.m_magic.m_magic[6], fixed_trailer.m_magic.m_magic[7]);
  fprintf(out, "    },\n");
  fprintf(out, "  }\n");
  fprintf(out, "}\n");
}
