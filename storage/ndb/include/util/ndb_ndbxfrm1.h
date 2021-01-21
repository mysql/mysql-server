/* Copyright (c) 2020, 2021, Oracle and/or its affiliates.

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

#ifndef NDB_UTIL_NDB_NDBXFRM_H
#define NDB_UTIL_NDB_NDBXFRM_H

#include "ndb_version.h" // NDB_VERSION_D
#include "ndb_types.h"
#include "ndbxfrm_iterator.h"

struct ndb_ndbxfrm1
{
  using byte = unsigned char;

  class header;
  class trailer;

  static const char magic[8];
  static constexpr Uint64 native_endian_marker = 0xFEDCBA9876543210;
  static constexpr Uint64 reverse_endian_marker = 0x0123456789ABCDEF;

  static void toggle_endian32(Uint32* x, size_t n = 1);
  static void toggle_endian64(Uint64* x, size_t n = 1);
  static bool is_all_zeros(const void* buf, size_t len);
};

class ndb_ndbxfrm1::header
{
public:
  header();

  int set_file_block_size(size_t file_block_size);
  int set_compression_method(Uint32 flag_compress);
  int set_encryption_cipher(Uint32 cipher); // 1 CBC-STREAM
  int set_encryption_padding(Uint32 padding); // 1 PKCS
  int set_encryption_kdf(Uint32 kdf); // 1 pbkdf2_sha256
  int set_encryption_kdf_iter_count(Uint32 count);
  int set_encryption_salts(const byte* salts, size_t salt_size, size_t salt_count);

  int prepare_for_write();
  size_t get_size() const; // output size needed by write_header()
  int write_header(ndbxfrm_output_iterator* out) const;

  static int detect_header(ndbxfrm_input_iterator* in, size_t* header_size);
  int read_header(ndbxfrm_input_iterator* in);
  int get_file_block_size(size_t* file_block_size);
  int get_trailer_max_size(size_t* trailer_max_size);
  int get_compression_method() const;
  int get_encryption_cipher(Uint32* cipher) const;
  int get_encryption_padding(Uint32* padding) const;
  int get_encryption_kdf(Uint32* kdf) const;
  int get_encryption_kdf_iter_count(Uint32* count) const;
  int get_encryption_salts(byte* salts, size_t salt_space, size_t* salt_size, size_t* salt_count) const;

private:
  int validate_header() const;
  int prepare_header_for_write();

  struct transform_version // 16 bytes align as Uint64 at least
  {
    static constexpr Uint32 flag_product_mask       = 0x000000FF;
    // type_char: "1.2.11"
    static constexpr Uint32 flag_product_zlib       = 0x00000001;
    // type_uint32: 0x1010107fL "1.1.1g release"
    static constexpr Uint32 flag_product_OpenSSL    = 0x00000002;
    static constexpr Uint32 flag_version_type_mask  = 0x00000F00;
    static constexpr Uint32 flag_version_type_char  = 0x00000100;
    static constexpr Uint32 flag_version_type_int32 = 0x00000200;
    static constexpr Uint32 flag_size_mask          = 0x0000F000;
    static constexpr Uint32 flag_extended           = 0x80000000;
    static constexpr Uint32 flag_zeros              = 0xFFFF0000;
    Uint32 m_flags;
    union
    {
      char m_char[12];
      Uint32 m_int32[3];
    };

    int toggle_endian();
    int validate() const;
  };
  struct fixed_header
  {
    // Magic part - 32 bytes
    struct magic
    {
      // magic NDBXFRM1 - always same byte order, N first.
      char m_magic[8];
      // endian 0xFEDCBA9876543210
      Uint64 m_endian;
      // header size: including magic, extra octets, and, zero padding
      Uint32 m_header_size;
      Uint32 m_fixed_header_size;
      Uint32 m_zeros[2];

      int validate() const;
      int toggle_endian();
    } m_magic;

    // Common part#1

    static constexpr Uint64 flag_extended    = 0x8000000000000000;
    static constexpr Uint64 flag_zeros       = 0xFFFFFFFFFCEECECC;

    static constexpr Uint64 flag_file_checksum_mask      = 0x0000000F;
    static constexpr Uint64 flag_file_checksum_in_header = 0x00000001;
    static constexpr Uint64 flag_file_checksum_crc32     = 0x00000002;
    static constexpr Uint64 flag_data_checksum_mask      = 0x000000F0;
    static constexpr Uint64 flag_data_checksum_in_header = 0x00000010;
    static constexpr Uint64 flag_data_checksum_crc32     = 0x00000020;

    // RFC1951 DEFLATE Compressed Data Format Specification version 1.3
    static constexpr Uint64 flag_compress_deflate  = 0x00000100;
    // If all bits in flag_compress_mask is zero, no compression is used
    static constexpr Uint64 flag_compress_mask     = 0x00000F00;

    static constexpr Uint64 flag_encrypt_cipher_mask         = 0x0000F000;
    static constexpr Uint64 flag_encrypt_cipher_aes_256_cbc  = 0x00001000;
    static constexpr Uint64 flag_encrypt_cipher_aes_256_xts  = 0x00002000;
    static constexpr Uint64 flag_encrypt_kdf_mask            = 0x000F0000;
    // RFC2898 PKCS #5: Password-Based Cryptography Specification Version 2.0
    static constexpr Uint64 flag_encrypt_kdf_pbkdf2_sha256   = 0x00010000;
    static constexpr Uint64 flag_encrypt_padding_mask        = 0x00F00000;
    static constexpr Uint64 flag_encrypt_padding_none        = 0x00000000;
    // PKCS#7 also RFC5652 Cryptographic Message Syntax (CMS)
    static constexpr Uint64 flag_encrypt_padding_pkcs        = 0x00100000;
    static constexpr Uint64 flag_encrypt_key_reuse_mask      = 0x0F000000;
    static constexpr Uint64 flag_encrypt_key_reuse_same      = 0x00000000;
    static constexpr Uint64 flag_encrypt_key_reuse_pair      = 0x01000000;
    static constexpr Uint64 flag_encrypt_key_reuse_mix_pair  = 0x02000000;
    // If all bits in flag_encrypt_mask is zero, no encryption is used
    static constexpr Uint64 flag_encrypt_mask                = 0x0FFFF000;
    
    // payload start:Uint32
    Uint64 m_flags;
    Uint32 m_dbg_writer_ndb_version; // = NDB_VERSION_D
    Uint32 m_octets_size;
    /*
     * File is guaranteed to be in multiples of block size
     * 0 means no block size
     */
    Uint32 m_file_block_size;
    /*
     * May be needed to know how much one need to pre-read before knowing that
     * end of file is reached and one can deduce how much at end of file that
     * do not belong to payload. (round up m_trailer_size  to nearest
     * blocksize and add one blocksize). =0 indicates no trailer
     */
    Uint32 m_trailer_max_size;
    byte m_file_checksum[4];
    byte m_data_checksum[4];
    Uint32 m_zeros01[1];

    /* Compress
     * zlib : ZLIB_VERSION "1.2.11" compare with zlibVersion()
     */
    transform_version m_compress_dbg_writer_header_version; // compiled for
    transform_version m_compress_dbg_writer_library_version; // linked with

    /* Encrypt
     * OpenSSL : OPENSSL_VERSION_NUM 0x1010107fL
     * compare with OpenSSL_version_num(OPENSSL_VERSION)
     */
    transform_version m_encrypt_dbg_writer_header_version;
    transform_version m_encrypt_dbg_writer_library_version;
    Uint32 m_encrypt_key_definition_iterator_count;
    Uint32 m_encrypt_key_definition_salt_size;
    Uint32 m_encrypt_key_definition_salt_count;
    // If zero no key reuse, only apply key at start
    Uint32 m_encrypt_key_reuse_data_unit_size;
    Uint32 m_encrypt_key_definition_salts_position_in_octets;
    Uint32 m_zeros02[1];

    int toggle_endian();
    int validate() const;

  };

  static constexpr size_t MIN_HEADER_SIZE = 8;
  static constexpr size_t MAX_HEADER_SIZE = 512;
  static_assert(sizeof(fixed_header) <= MAX_HEADER_SIZE, "");
  static constexpr size_t MAX_OCTETS_SIZE = 32000;
  static_assert(MAX_HEADER_SIZE + MAX_OCTETS_SIZE <= 32768, "");

  static constexpr size_t MAX_BUFFER_SIZE = MAX_HEADER_SIZE + MAX_OCTETS_SIZE;

  struct
  {
    struct fixed_header m_header;
    byte m_octets[MAX_OCTETS_SIZE];
  } m_buffer;
};

class ndb_ndbxfrm1::trailer
{
public:
  trailer();

  int set_data_size(Uint64 size);
  int set_data_crc32(long crc32);
  int set_file_pos(off_t file_pos);
  int set_file_block_size(size_t file_block_size);
  int prepare_for_write();
  size_t get_size() const;
  int write_trailer(ndbxfrm_output_iterator* out) const;

  int read_trailer(ndbxfrm_input_reverse_iterator* in);
  int get_data_size(Uint64* size) const;
  int get_data_crc32(Uint32* crc32) const;
private:
  int prepare_trailer_for_write();

  int write_ndbxfrm1_trailer(byte* buf, size_t* len, off_t file_pos, size_t file_block_size);
  int read_ndbxfrm1_trailer(const byte* buf, size_t len, off_t file_pos);
  int validate_trailer() const;

  struct fixed_trailer
  {
    static constexpr Uint64 flag_extended   = 0x8000000000000000;
    static constexpr Uint64 flag_zeros      = 0xFFFFFFFFFFFFFFCC;
    static constexpr Uint64 flag_file_checksum_mask       = 0x0000000F;
    static constexpr Uint64 flag_file_checksum_in_trailer = 0x00000001;
    static constexpr Uint64 flag_file_checksum_crc32      = 0x00000002;
    static constexpr Uint64 flag_data_checksum_mask       = 0x000000F0;
    static constexpr Uint64 flag_data_checksum_in_trailer = 0x00000010;
    static constexpr Uint64 flag_data_checksum_crc32      = 0x00000020;
    Uint64 m_flags;
    Uint64 m_data_size;
    byte m_file_checksum[4];
    byte m_data_checksum[4];

    /*
     * Note: When reading trailer, magic should be read first by itself.
     * And then start of trailer read by itself.
     * Writer can have used a bigger or smaller trailer, growing and
     * shrinking in the "middle".
     */
    struct magic
    {
      Uint32 m_zeros[2];
      Uint32 m_fixed_trailer_size; // including magic, and, zero padding
      Uint32 m_trailer_size; // including magic, and, zero padding
      Uint64 m_endian; // 0xFEDCBA9876543210
      char m_magic[8]; // NDBXFRM1

      int validate() const;
      int toggle_endian();
    } m_magic;

    int toggle_endian();
    int validate() const;
  };
  static_assert(sizeof(fixed_trailer) % 8 == 0, "");

  struct
  {
    struct fixed_trailer m_trailer;
  } m_buffer;

  off_t m_file_pos;
  size_t m_file_block_size;
  size_t m_zero_pad_size;
};

inline
void ndb_ndbxfrm1::toggle_endian32(Uint32* x, size_t n)
{
  using std::swap;
  byte* p = reinterpret_cast<byte*>(x);
  while (n > 0)
  {
    swap(p[0], p[3]);
    swap(p[1], p[2]);
    p += sizeof(*p);
    n--;
  }
}

inline
void ndb_ndbxfrm1::toggle_endian64(Uint64* x, size_t n)
{
  using std::swap;
  byte* p = reinterpret_cast<byte*>(x);
  while (n > 0)
  {
    swap(p[0], p[7]);
    swap(p[1], p[6]);
    swap(p[2], p[5]);
    swap(p[3], p[4]);
    p += sizeof(*p);
    n--;
  }
}

#endif
