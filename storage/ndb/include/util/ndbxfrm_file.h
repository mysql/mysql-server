/* Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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

#ifndef NDB_UTIL_NDBXFRM_FILE_H
#define NDB_UTIL_NDBXFRM_FILE_H

#include "portlib/ndb_file.h"
#include "util/ndb_ndbxfrm1.h"
#include "util/ndb_openssl_evp.h"
#include "util/ndb_zlib.h"
#include "util/ndbxfrm_buffer.h"
#include "util/ndbxfrm_iterator.h"

#include <cassert>

/** @class ndbxfrm_file
 *
 * ndbxfrm_file
 * ============
 *
 * ndbxfrm_file is a common class for writing and reading Ndb backup files and
 * the data nodes own files.
 *
 * Overview
 * --------
 *
 * Both data nodes themself and tools accessing the same files uses this class.
 *
 * Intention is that all files that Ndb uses should be efficient written and
 * read in, for Ndb, natural way.
 *
 * It should be possible to compress or encrypt any file without changing the
 * file operation methods used.
 *
 * The class method should also be independent of any differences between
 * supported operating system or third party libraries used.
 *
 * Although it might be tempting to treat this class suitable for general usage
 * this will likely never be the case, but there will be several restriction of
 * what kind of file operation will be supported to simplify implementation and
 * testing.
 *
 * One general limitation is that files are assumed to only be opened by one
 * process at a time.  Typically also only accessed by one thread at a time
 * although there are some support of multi threaded access from one process in
 * which case it is assumed that the different threads never access the same
 * part of file in parallel.
 *
 * Since this class will be used inside data nodes that puts some limits on
 * implementation, such as avoiding heap allocations or too much stack.
 *
 * ### File formats
 *
 * If no transformation, that is compression or encryption, of file data is
 * requested ndbxfrm_file will write data as is to file in raw format.
 *
 * Else if some transformation is requested a wrapped file format will be used,
 * adding a file header and file trailer around the transformed data.
 *
 * The header and trailer themself are never compressed nor encrypted.
 *
 * For backward compatibility compressed backup files will use same file format,
 * called AZ31, consisting of a file header with fixed content and a trailer
 * containing checksum and size of uncompressed data.
 *
 * For other transformed data a new file format called NDBXFRM1 will be used,
 * which also adds a file header and file trailer around the transformed data.
 *
 * ### File operations
 *
 * #### Stream mode access
 *
 * All files should be possible to create by writing from start to end in order
 * without rewriting earlier parts of file.
 *
 * Any files should also be able to read from start to end in order.
 *
 * This is not a strict requirement for Ndb but makes it possible to write
 * tools inspecting and manipulating files easily.
 *
 * This property do have some implication on the file format.  Since for
 * example encryption methods typically have no way to signal end of encrypted
 * data by itself one need to read ahead to be able to detect file trailer
 * which in turn can tell where the payload data ends.
 *
 * Also this makes the trailer the natural place for storing for example
 * checksums and size information of data.
 *
 * This mode is used for backup files and LCP data files.
 *
 * Note, that if a block encryption mode such as XTS is used with OpenSSL, one
 * either need to require that all accesses uses whole block buffers or one
 * will need an extra buffering step for a block since OpenSSL API in those
 * cases requires full block to be passed in one function call to decrypter or
 * encrypter.  If compression is used a buffering step between compress and
 * encryption step since the block size of raw data typically is not kept after
 * compression or decompression step.
 *
 * Other things that could require some kind of block size access for stream
 * mode can be certain optimized access mode to underlying file, such as
 * O_DIRECT on Linux, which also has requirements of the alignment of buffer in
 * memory.
 *
 * Future note: One could compress block and encrypt that and then pad with
 * zeros to block size again.  This would keep block size also after
 * compression, but this would only be useful if underlying storage can avoid
 * storing the zero-padding.  Another approach could be to compress a block and
 * then write a preamble with the compressed size before writing the compressed
 * data.
 *
 * #### Reverse stream read mode access
 *
 * For backup undo log files are read in backward direction.  This is currently
 * only implemented for CBC mode without compression.
 *
 * Due to the extra complexity to be able to handle this mode, there should be
 * no new uses of it.
 *
 * #### Random block mode access
 *
 * In block mode access any write or read always access complete blocks.  And
 * all accesses should be at aligned blocks as seen from raw untransformed
 * data.  The position read from file may be different, although currently we
 * only use same block size in data as on file.  For transformed data the file
 * will have header and trailer which displaces the file position from data
 * position.  Strictly a data block do not need to be aligned according to
 * block size in file although it will typically be.
 *
 * Currently there is no support for compression in this mode.
 *
 * The minimal block size supported by an implementation is the least
 * supporting any special mode requirements for underlying file, such as for
 * example O_DIRECT, and, if encryption is used the key reuse block size, this
 * will be called the random access block size.  The random access block size
 * is not a property of the file itself but depends on how the implementation
 * can access the file.
 *
 * Since there need not be any buffering file state in this mode it is suitable
 * also for multi threaded use, as long as the threads do no access the same
 * block in parallel.  At least not if one of them is a write access.
 *
 * #### Padding
 *
 * There are two kinds of padding.
 *
 * The encryption may require the data to be aligned to the cipher block, for
 * example 16 bytes as for CBC-mode, if the data unit size is not guaranteed to
 * be a multiple of that padding is needed.  If this kind of padding is needed
 * we typically turn it on for OpenSSL to handle.  If a block cipher mode, such
 * as XTS, is used data unit size do not need to be a multiple of cipher block
 * size, although the data unit size must be at least one cipher block size.
 * In that case the last data unit will possibly need padding if it can be less
 * than 16 bytes.
 *
 * For files which is intended for block mode access the transformed data will
 * start on an even file block size offset. In that case header may be byte
 * padded with zeros. These bytes is not part of transformed data and will for
 * example not be encrypted.  Likewise if one want file do end on an even
 * block, the trailer is padded by prepending zero bytes to trailer.
 *
 * #### Big reads and writes and buffering
 *
 * One open problem is how to transform big chunks of data without doing big
 * heap or stack allocations, or doing wasteful preallocation of big buffers.
 *
 * The current approach is for this class to provide functions to do the read
 * or write access and the data transformation in different steps.
 *
 * ### Usage limitations
 *
 * -   Only one process at a time are allowed to have file open.
 *
 * -   Files can not change file size after being fully created.
 *
 *     To extend a file one would need to overwrite and rewrite trailer.
 *
 *     Also if whole file is compressed with deflate or encrypted with CBC-mode
 *     one would need to read the file from start to end to be able to
 *     correctly apply the compression or encryption for the extended part.
 *
 *     Allow file size changes could be a possible extension but is currently
 *     not needed, if so likely only supporting paged based transformations.
 *
 *     Even if file does not change size the amount of disk block usage may
 *     change as typical for sparse files.
 *
 * ### Implementation limitations
 *
 * -   "Moderate" stack usage allowed.
 *
 * -   No internal heap allocations allowed.
 *
 * ### Block sizes
 *
 * There are many notions of block sizes in implementation and often they
 * coincide which each other and in some places implementation probably assume
 * that they coincide.
 *
 * -    m_file_block_size.  The smallest unit of data written to read from a
 *      physical file, the file size will always be an even multiple of file
 *      block size. If file block size is zero there are no notion of file
 *      blocks.
 *
 * -    m_file->get_block_size().  If not zero it determines at which positions
 *      one may read or write a block from a file, the position should always
 *      be an even multiple of the block size.  And accesses should be in whole
 *      blocks except the last block in file which could be shorter, in
 *      contrast to file block size there the last block always will have a
 *      full file block size.
 *
 * -    key_data_unit_size.  This determines how big chunks of unencrypted data
 *      will be using the same key.  For the CBC-mode encryption there key is
 *      only used for the first cipher-block and rest of data is chain
 *      encrypted size zero is used.  For XTS-mode encryption the key data unit
 *      size should be a multiple of the XTS data unit size and is often the
 *      same size.  Often the key_data_unit_size also coincide with
 *      m_file_block_size.
 *
 * -    data_block_size.  The size of a data block that are encrypted as an one
 *      unit.  The last block of data may be shorter, but for XTS it must be
 *      at least 16 bytes.
 *
 * -    random_access_block_size.  The smallest block size that can be randomly
 *      accessed.  Typically maximum of m_file->get_block_size and
 *      data_block_size.  If file do not support random access, as compressed or
 *      CBC-mode encrypted files it will be zero.
 *
 * Currently the block sizes seen from file perspective matches the block sizes
 * of unencrypted data seen from application.  If support for example for
 * authenticated encryption that will change.
 */

class ndbxfrm_file
{
 public:
  static constexpr size_t BUFFER_SIZE = ndbxfrm_buffer::size();
  static constexpr Uint64 INDEFINITE_SIZE = UINT64_MAX;
  static constexpr ndb_off_t INDEFINITE_OFFSET = -1;
  static_assert((ndb_off_t)INDEFINITE_SIZE == -1);
  static_assert(UINT64_MAX == Uint64(INDEFINITE_OFFSET));

  using byte = unsigned char;
  ndbxfrm_file();
  bool is_open() const;
  void reset();
  /*
   * open returns 0 on success, -1 on failure.
   */
  int open(ndb_file& file,
           const byte* pwd_key,
           size_t pwd_key_len);

  /*
   * Returns 0 on success, -1 on failure.
   * Used by ndbxfrmt tool to access file header and trailer even if no
   * or wrong password is provided.
   */
  int read_header_and_trailer(ndb_file &file, ndb_ndbxfrm1::header& header,
                              ndb_ndbxfrm1::trailer& trailer);

  int create(ndb_file& file,
             bool compress,
             const byte* pwd,
             size_t pwd_len,
             int kdf_iter_count, // 0 - aeskw, else pbkdf2, -1 let ndb_ndbxfrm decide
             int key_cipher,  // 0 - none, ndb_ndbxfrm1::cipher_*
             int key_count, // -1 let ndbxfrm_file decide
             size_t key_data_unit_size,
             size_t file_block_size,
             Uint64 data_size,
             bool is_data_size_estimated
             );
  /*
   * Use abort when you for example has not fulfilled the initialization of the
   * file content and intend to remove the file after close.
   * For some transforms the data that application so far have passed to
   * ndbxfrm_file may not be possible to fulfil the transform for.
   * The abort flag will in such case ignore writing the pending data.
   *
   * Also when reading a file abort flag could be used when only part of file
   * is read and one need to skip implicit checksum checks.
   */
  int close(bool abort);
  ndb_off_t get_size() const { return m_data_size; }
  ndb_off_t get_file_size() const { return m_file_size; }
  ndb_off_t get_file_pos() const { return m_file_pos; }
  size_t get_data_block_size() const { return m_data_block_size; }
  ndb_off_t get_data_size() const { return m_data_size; }
  ndb_off_t get_data_pos() const { return m_data_pos; }
  bool has_definite_data_size() const;
  bool has_definite_file_size() const;
  static bool is_definite_size(Uint64 size);
  static bool is_definite_offset(ndb_off_t offset);
  bool is_compressed() const { return m_compressed; }
  bool is_encrypted() const { return m_encrypted; }
  size_t get_random_access_block_size() const;
  bool is_transformed() const { return is_compressed() || is_encrypted(); }
  /*
   * When read and write pages one will need to do it in two steps.
   * For write first call transform_pages followed by write_transformed_pages,
   * and for read first call read_untransformed_pages followed by
   * untransform_pages.
   * For files that does not transform (in this case encrypt) data one can skip
   * the transform and untransform steps.
   * The reason for this split is that when writing multiple pages one want to
   * call the operating system with one write call.  But then one would need to
   * transform the input data into an equally big temporary buffer or inplace.
   * Instead of providing a more complex interface to read and write supporting
   * different approaches, the caller need to provide the transformed buffers.
   */
  int transform_pages(ndb_openssl_evp::operation* op,
                      ndb_off_t data_pos,
                      ndbxfrm_output_iterator* out,
                      ndbxfrm_input_iterator* in);
  int untransform_pages(ndb_openssl_evp::operation* op,
                        ndb_off_t data_pos,
                        ndbxfrm_output_iterator* out,
                        ndbxfrm_input_iterator* in);
  int read_transformed_pages(ndb_off_t data_pos, ndbxfrm_output_iterator* out);
  int write_transformed_pages(ndb_off_t data_pos, ndbxfrm_input_iterator* in);

  int write_forward(ndbxfrm_input_iterator* in);

  int read_forward(ndbxfrm_output_iterator* out);
  int read_backward(ndbxfrm_output_reverse_iterator* out);
  ndb_off_t move_to_end();
  ndb_off_t get_payload_start() const;

 private:
  // file fixed properties
  ndb_file* m_file;
  size_t m_file_block_size;
  ndb_off_t m_payload_start;
  bool m_append;
  bool m_encrypted;
  bool m_compressed;
  bool m_is_estimated_data_size;
  bool m_have_data_crc32;
  ndb_openssl_evp openssl_evp;
  enum
  {
    FF_UNKNOWN,
    FF_RAW,
    FF_AZ31,
    FF_NDBXFRM1
  } m_file_format;
  alignas(ndb_openssl_evp::MEMORY_ALIGN)
      byte m_encryption_keys[ndb_openssl_evp::MEMORY_NEED];
  size_t m_data_block_size;
  Uint32 m_data_crc32;

  // file status
  ndb_off_t m_payload_end;
  ndb_off_t m_file_pos;
  Uint64 m_data_size;
  Uint64 m_file_size;
  Uint64 m_estimated_data_size;

  // operation per block properties
  ndb_openssl_evp::operation openssl_evp_op;
  // operation per file properties
  ndb_zlib zlib;
  enum
  {
    OP_NONE,
    OP_WRITE_FORW,
    OP_READ_FORW,
    OP_READ_BACKW
  } m_file_op;
  Uint32 m_crc32;  // should be zeroed for new op

  ndbxfrm_buffer m_decrypted_buffer;
  ndbxfrm_buffer m_file_buffer;
  Uint64 m_data_pos;

  /*
  * open returns 0 on success, -1 on failure, -2 almost succeeded
  * header and trailer are valid but for example unwrapping encryption
  * keys failed.
  * The -2 is needed to allow "ndbxfrm --[detailed-]info" to access file
  * header and trailer even if no or wrong password is provided.
  */
  int open(ndb_file& file, const byte* pwd_key,
           size_t pwd_key_len, ndb_ndbxfrm1::header& header,
           ndb_ndbxfrm1::trailer& trailer);

  int flush_payload();

  bool in_file_mode() const { return (m_payload_end >= 0); }
  bool in_stream_mode() const { return !in_file_mode(); }
  int read_header(ndbxfrm_input_iterator* in,
                  const byte* pwd_key,
                  size_t pwd_key_len,
                  size_t* max_trailer_size,
                  ndb_ndbxfrm1::header& header);
  int read_trailer(ndbxfrm_input_reverse_iterator* in,
                   ndb_ndbxfrm1::trailer& trailer);
  int generate_keying_material(ndb_ndbxfrm1::header* ndbxfrm1,
                               const byte* pwd_key,
                               size_t pwd_key_len,
                               int key_cipher,
                               int key_count);
  int write_header(
      ndbxfrm_output_iterator* out,
      size_t data_page_size,
      const byte* pwd_key,
      size_t pwd_key_len,
      int kdf_iter_count,
      int key_cipher,  // 0 - none, 1 - cbc, 2 - xts (always no padding)
      int key_count,
      size_t key_data_unit_size);
  int write_trailer(ndbxfrm_output_iterator* out,
                    ndbxfrm_output_iterator* extra);
};

inline size_t ndbxfrm_file::get_random_access_block_size() const
{
  if (m_compressed)
  {
    size_t alignment = zlib.get_random_access_block_size();
#if !defined(NDEBUG)
    /*
     * If both compression and encryption is activated, and compression allows
     * random access, then it is assumed that encryption also allows random
     * access for same alignment.
     */
    if (alignment > 0 && m_encrypted)
    {
      size_t align = openssl_evp.get_random_access_block_size();
      assert(align > 0);
      assert(alignment % align == 0);
    }
#endif
    return alignment;
  }
  if (m_encrypted)
  {
    return openssl_evp.get_random_access_block_size();
  }
  size_t alignment = m_file->get_block_size();
  if (alignment > 0) return alignment;
  return 1;
}

inline bool ndbxfrm_file::has_definite_data_size() const
{
  return (m_data_size != INDEFINITE_SIZE);
}

inline bool ndbxfrm_file::has_definite_file_size() const
{
  return (m_file_size != INDEFINITE_SIZE);
}

inline bool ndbxfrm_file::is_definite_size(Uint64 size)
{
  return (size != INDEFINITE_SIZE);
}

inline bool ndbxfrm_file::is_definite_offset(ndb_off_t offset)
{
  return (offset != INDEFINITE_OFFSET);
}

inline ndb_off_t ndbxfrm_file::get_payload_start() const
{
  return m_payload_start;
}
#endif
