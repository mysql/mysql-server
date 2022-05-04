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

#ifndef NDB_UTIL_NDB_NDBXFRM_H
#define NDB_UTIL_NDB_NDBXFRM_H

#include <stdio.h>

#include "ndb_version.h" // NDB_VERSION_D
#include "ndb_types.h"
#include "ndbxfrm_iterator.h"

/** @class ndb_ndbxfrm1
 *
 * ndb_ndbxfrm1
 * ============
 *
 * Defines header and trailer and helper functions for writing and reading files
 * using NDBXFRM1 format.
 *
 * The information in header and trailer are primarily to give the reader of
 * the file necessary information to be able to read the file.
 *
 * Not only should it be enough information for the process that already knows
 * about the file but also for tools that do not know about the actual data and
 * only will read the file in stream mode from start to end, like the tool
 * ndbxfrm that can be used to decrypt and decompress a file and possibly
 * compress and reencrypt it.
 *
 * File vs memory layout
 * ---------------------
 *
 * The header and trailers are represented by a C++-struct in memory, and which
 * is written as is to disk.  There should be no hidden padding and all scalar
 * value types should have explicit size and alignment assumed to be same as
 * the size.
 *
 * For the case that file is read from a system with reverse byte order, there
 * are explicit toggle_endian() functions that should be called.  Only
 * little-endian and big-endian byte order is supported.
 *
 * Backward and forward compatibility
 * ----------------------------------
 *
 * The header and trailer evolve by either adding new fields or use previously
 * unused bits.  When extending the headers and trailers one need to make sure
 * they are both backward and forward compatible.  The rule for that is that a
 * value with all zero-bits should behave the same as if there was no value.
 * If one add a field and the all zero-bits value is not neutral one also need
 * to introduce a bit indicating whether field is used or not.  If a value is
 * not used it should always be all zero-bits.
 *
 * If a new reader reads a file written by a old writer that uses a smaller
 * header than the new reader supports, then the reader reads the complete
 * header into memory add pad it with zero-bits to the size that the reader
 * supports.  Compared with an old reader that supports the same header as
 * writer both the new and old reader will treat the file equally.
 *
 * And vice versa, if a old reader reads a file written by a new writer using
 * a bigger header than the old reader supports.  The old reader reads and uses
 * the shorter part of the header that it supports, if the rest of written
 * header is all zeros the reader can proceed, if not all bits are zero the
 * reader should fail due to unsupported features used in header.  When the old
 * reader proceeds the result should be the same as it was a new reader.
 *
 * There is no way to add fields that and old reader can ignore, all new fields
 * will break reader unless the all-zero-bits value is used.
 */

struct ndb_ndbxfrm1
{
  using byte = unsigned char;

  class header;
  class trailer;

  static const char magic[8];
  static constexpr Uint64 native_endian_marker = 0xFEDCBA9876543210;
  static constexpr Uint64 reverse_endian_marker = 0x1032547698BADCFE;

  static void toggle_endian32(Uint32* x, size_t n = 1);
  static void toggle_endian64(Uint64* x, size_t n = 1);
  static bool is_all_zeros(const void* buf, size_t len);

  static constexpr Uint32 compression_deflate = 1;
  static constexpr Uint32 cipher_cbc = 1;
  static constexpr Uint32 cipher_xts = 2;
  static constexpr Uint32 padding_pkcs = 1;
  static constexpr Uint32 krm_pbkdf2_sha256 = 1;
  static constexpr Uint32 krm_aeskw_256 = 2;
  static constexpr Uint32 key_selection_mode_same = 0;
  static constexpr Uint32 key_selection_mode_pair = 1;
  static constexpr Uint32 key_selection_mode_mix_pair = 2;
};

class ndb_ndbxfrm1::header
{
public:
  header();

  static constexpr size_t get_legacy_max_keying_material_size()
  {
    return LEGACY_MAX_OCTETS_SIZE;
  }
  static constexpr size_t get_max_keying_material_size()
  {
    return MAX_OCTETS_SIZE;
  }
  int set_file_block_size(size_t file_block_size);
  int set_compression_method(Uint32 flag_compress);
  int set_compression_padding(Uint32 flag_padding);
  int set_encryption_cipher(Uint32 cipher);
  int set_encryption_padding(Uint32 padding);
  int set_encryption_krm(Uint32 krm);
  int set_encryption_krm_kdf_iter_count(Uint32 count);
  int set_encryption_key_selection_mode(Uint32 key_selection_mode, Uint32 key_data_unit_size);
  int set_encryption_keying_material(const byte* keying_material,
                                     size_t keying_material_size,
                                     size_t keying_material_count);

  int prepare_for_write(Uint32 header_size = 0);
  size_t get_size() const; // output size needed by write_header()
  int write_header(ndbxfrm_output_iterator* out) const;

  static int detect_header(const ndbxfrm_input_iterator* in, size_t* header_size);
  int read_header(ndbxfrm_input_iterator* in);
  int get_file_block_size(size_t* file_block_size);
  int get_trailer_max_size(size_t* trailer_max_size);
  int get_compression_method() const;
  int get_compression_padding() const;
  int get_encryption_cipher(Uint32* cipher) const;
  int get_encryption_padding(Uint32* padding) const;
  int get_encryption_krm(Uint32* krm) const;
  int get_encryption_krm_kdf_iter_count(Uint32* count) const;
  int get_encryption_key_selection_mode(Uint32* key_selection_mode, Uint32* key_data_unit_size) const;
  int get_encryption_keying_material(byte* keying_material,
                                     size_t keying_material_space,
                                     size_t* keying_material_size,
                                     size_t* keying_material_count) const;

  void printf(FILE* out) const;
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
    static constexpr Uint64 flag_zeros = 0xFFFFFFFFECECCECC;

    static constexpr Uint64 flag_file_checksum_mask      = 0x0000000F;
    static constexpr Uint64 flag_file_checksum_in_header = 0x00000001;
    static constexpr Uint64 flag_file_checksum_crc32     = 0x00000002;
    static constexpr Uint64 flag_data_checksum_mask      = 0x000000F0;
    static constexpr Uint64 flag_data_checksum_in_header = 0x00000010;
    static constexpr Uint64 flag_data_checksum_crc32     = 0x00000020;

    static constexpr Uint64 flag_compress_method_mask    = 0x00000F00;
    // RFC1951 DEFLATE Compressed Data Format Specification version 1.3
    static constexpr Uint64 flag_compress_method_deflate = 0x00000100;
    static constexpr Uint64 flag_compress_padding_mask   = 0xF0000000;
    static constexpr Uint64 flag_compress_padding_none   = 0x00000000;
    static constexpr Uint64 flag_compress_padding_pkcs   = 0x10000000;
    // If all bits in flag_compress_mask is zero, no compression is used
    static constexpr Uint64 flag_compress_mask           = 0xF0000F00;

    static constexpr Uint64 flag_encrypt_cipher_mask         = 0x0000F000;
    static constexpr Uint64 flag_encrypt_cipher_aes_256_cbc  = 0x00001000;
    static constexpr Uint64 flag_encrypt_cipher_aes_256_xts  = 0x00002000;
    static constexpr Uint64 flag_encrypt_krm_mask = 0x000F0000;
    // RFC2898 PKCS #5: Password-Based Cryptography Specification Version 2.0
    static constexpr Uint64 flag_encrypt_krm_pbkdf2_sha256 = 0x00010000;
    // RFC3394 Advanced Encryption Standard (AES) Key Wrap Algorithm
    static constexpr Uint64 flag_encrypt_krm_aeskw_256 = 0x00020000;
    static constexpr Uint64 flag_encrypt_padding_mask        = 0x00F00000;
    static constexpr Uint64 flag_encrypt_padding_none        = 0x00000000;
    // PKCS#7 also RFC5652 Cryptographic Message Syntax (CMS)
    static constexpr Uint64 flag_encrypt_padding_pkcs        = 0x00100000;
    static constexpr Uint64 flag_encrypt_key_selection_mode_mask      = 0x0F000000;
    /*
     * flag_encrypt_key_selection_mode_same - use same key/iv pair for all data
     * units.
     */
    static constexpr Uint64 flag_encrypt_key_selection_mode_same      = 0x00000000;
    /*
     * flag_encrypt_key_selection_mode_pair - pair key#n with iv#n and use that
     * for data unit#n.
     * If more data units than pair start over with pair #1
     *
     * In pseudo code, if number of keys and iv is N.
     * key(data_unit#n) = key#(n%N)
     * iv(data_unit#n) = iv#(n%N)
     */
    static constexpr Uint64 flag_encrypt_key_selection_mode_pair      = 0x01000000;
    /*
     * flag_encrypt_key_selection_mode_mix_pair - For first data units use
     * first key and first iv, for next data unit use next iv and so forth.
     * Then all ivs been used, use next key and first iv and so forth.
     *
     * In pseudo code, if number of keys and iv is N.
     * key(data_unit#n) = key#((n/N)%N)
     * iv(data_unit#n) = iv#(n%N)
     */
    static constexpr Uint64 flag_encrypt_key_selection_mode_mix_pair  = 0x02000000;
    // If all bits in flag_encrypt_mask is zero, no encryption is used
    static constexpr Uint64 flag_encrypt_mask                = 0x0FFFF000;
    
    // payload start:Uint32
    Uint64 m_flags;
    Uint32 m_dbg_writer_ndb_version; // = NDB_VERSION_D
    Uint32 m_octets_size;
    /*
     * If m_file_block_size is not zero it indicates that the file size is a
     * multiple of m_file_block_size.
     * If the file is actually a file the reader can typically determine how
     * big the file is by other means.
     * But if the file is not an actual file but read as a stream of some kind
     * one will need to know how much to readahead to be able to detect the
     * trailer.
     * To get the needed readahead one also need to know how big the trailer
     * may be, that is given by m_trailer_max_size.  The trailer will always be
     * written as very last part of file and if needed it will be preceded by
     * zero-padding such that the trailer ends on an even file block.
     * In "worst" case appending the trailer would start a new file block with
     * one byte, in that case one need to zero pad with file block size minus
     * one byte and then append the trailer, in that case the readahead needed
     * is file block size plus trailer size for detecting the end of the file.
     * The trailer have m_trailer_size which give the size of trailer including
     * zero-padding.
     * The m_file_block_size and m_trailer_max_size are both mandatory.
     */
    Uint32 m_file_block_size;
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
    Uint32 m_encrypt_krm_kdf_iterator_count;
    Uint32 m_encrypt_krm_keying_material_size;
    Uint32 m_encrypt_krm_keying_material_count;
    /*
     * m_encrypt_key_data_unit_size - determines how much data is encrypted as
     * an unit.
     *
     * If there are several keys and ivs one can choose how to form key and iv
     * pairs for each encrypted data unit by flag_encrypt_key_selection_mode_xxx.
     * This is used with XTS-mode for example using the pages size, 32768 bytes,
     * as data unit size.
     *
     * If m_encrypt_key_data_unit_size is zero, all data it encrypted in
     * sequence with one key and iv.  This is used with CBC-mode.
     *
     * If m_encrypt_key_data_unit_size was set to a non-zero value with
     * CBC-mode it would mean that each data unit is compressed by its own, no
     * chaining between data units and each data unit can use different key and
     * iv pair as given by flag_encrypt_key_selection_mode_xxx.
     *
     * The data unit size gives the unit size of unencrypted data.
     *
     * Depending on encryption and padding mode used the size of encrypted data
     * may differ from unencrypted data size.
     */
    Uint32 m_encrypt_key_data_unit_size;
    Uint32 m_encrypt_krm_keying_material_position_in_octets;
    Uint32 m_encrypt_krm_key_count;
    Uint32 m_zeros02[1];

    int toggle_endian();
    int validate() const;

  };

  static constexpr size_t MIN_HEADER_SIZE = 8;
  static constexpr size_t MAX_HEADER_SIZE = 512;
  static_assert(sizeof(fixed_header) <= MAX_HEADER_SIZE);
  static constexpr size_t LEGACY_MAX_OCTETS_SIZE = 16000;
  static constexpr size_t MAX_OCTETS_SIZE = 32000;
  static_assert(MAX_HEADER_SIZE + MAX_OCTETS_SIZE <= 32768);

  static constexpr size_t MAX_BUFFER_SIZE = MAX_HEADER_SIZE + MAX_OCTETS_SIZE;

  struct
  {
    struct fixed_header m_header;
    byte m_octets[MAX_OCTETS_SIZE];
  } m_buffer;
  size_t m_zero_pad_size;
};

class ndb_ndbxfrm1::trailer
{
public:
  trailer();

  int set_data_size(Uint64 size);
  int set_data_crc32(long crc32);
  int set_file_pos(ndb_off_t file_pos);
  int set_file_block_size(size_t file_block_size);
  int prepare_for_write(Uint32 trailer_size = 0);
  size_t get_size() const;
  int write_trailer(ndbxfrm_output_iterator* out,
                    ndbxfrm_output_iterator* extra = nullptr) const;

  int read_trailer(ndbxfrm_input_reverse_iterator* in);
  int get_data_size(Uint64* size) const;
  int get_data_crc32(Uint32* crc32) const;
  int get_trailer_size(size_t* size) const;

  void printf(FILE* out) const;
private:
  int prepare_trailer_for_write();

  int write_ndbxfrm1_trailer(byte* buf, size_t* len, ndb_off_t file_pos,
                             size_t file_block_size);
  int read_ndbxfrm1_trailer(const byte* buf, size_t len, ndb_off_t file_pos);
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
    /*
     * m_data_size - the size of the untransformed data
     */
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
  static_assert(sizeof(fixed_trailer) % 8 == 0);

  struct
  {
    struct fixed_trailer m_trailer;
  } m_buffer;

  ndb_off_t m_file_pos;
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
    p += 4;
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
    p += 8;
    n--;
  }
}

inline
int ndb_ndbxfrm1::trailer::get_trailer_size(size_t* size) const
{
  *size = m_buffer.m_trailer.m_magic.m_trailer_size;
  return 0;
}

#endif
