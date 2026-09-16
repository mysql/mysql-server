/* Copyright (c) 2023, 2026, Oracle and/or its affiliates.

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
*/

#pragma once

#include <cstdlib> /* size_t */
#include <functional>
#include <span>
#include "os0file.h" /* IORequest */

namespace ib::fil {

/* Interface for handling operations on the opened tablespace node handle. */
class Tablespace_node_handle_interface {
 public:
  using byte = unsigned char;

  enum class Status { SUCCESS = 0, IO_ERROR };

  /** Type used for numbering the pages in the node. */
  using Page_number = uint32_t;

  /** Closes the opened node handle */
  virtual ~Tablespace_node_handle_interface() = default;

  /** Returns true if the node must be flushed in general to ensure data
  durability.
  @return true if flush is needed, false otherwise. */
  [[nodiscard]] virtual bool needs_flushes_for_durability() const = 0;

  /** Ensures any data written so far to this tablespace node before this call
  will be made durable and will survive either software or hardware crash. */
  [[nodiscard]] virtual Status flush() = 0;

  /** Truncate the node storage to the given size.
  @param[in]  size       Expected size in pages of node after truncation. If the
                         size provided is more than or equal to the current size
                         of the node storage, return success.
  @return SUCCESS if truncated successfully, error code otherwise. */
  [[nodiscard]] virtual Status truncate(Page_number size) = 0;

  /** Makes the @p number_of_pages pages, starting with page number @p
  first_page contain all zeros. This can be use to efficiently extend the node
  size. It may be implemented more efficiently than actually writing buffers
  with zeros, but only if @p optimize_writes is set to true.
  @param[in] first_page       Number of page in node from to be overwritten with
                              zeros.
  @param[in] number_of_pages  Number of pages to overwrite with zeros.
  @param[in] optimize_writes  If true the implementation can use faster and
                              possibly less robust way to zero the range.
  @return SUCCESS if zeroed successfully, error code otherwise. */
  [[nodiscard]] virtual Status fill_range_with_zeros(
      Page_number first_page, Page_number number_of_pages,
      bool optimize_writes) = 0;

  using Callback = std::function<void(dberr_t io_result)>;

  enum class Status_IO {
    SUCCESS = 0,
    IO_DECRYPT_FAIL,
    CORRUPTION,
    IO_OVERFLOW,
    OUT_OF_MEMORY,
    IO_DECOMPRESS_FAIL,
    IO_NO_PUNCH_HOLE,
    UNSUPPORTED,
    IO_ERROR
  };

  /** Reads a requested page synchronously.

  Before the page is returned, the page will be decrypted and decompressed,
  depending on information present in the page header.

  The size of the buffer in bytes should be at least the tablespace physical
  page size which was passed to tablespaces_nodes::open() or calculated from
  flags passed to tablespaces_nodes::create().

  @param[in]   req        IO request type, compression and encryption
                          information.
  @param[out]  buffer     A buffer where to read the data in. It must be aligned
                          in memory to physical page size. It must be able to
                          store physical page size bytes. The memory pointed is
                          managed by the caller and must remain valid until the
                          call finishes.
                          TODO : buffer to be replaced with std::span.
  @param[in]   page_no    Offset from the first page in the node to read
                          from.
  @return Status_IO::SUCCESS on successful read, otherwise error code */
  [[nodiscard]] virtual Status_IO read_page(IORequest req, byte *buffer,
                                            Page_number page_no) = 0;

#ifndef UNIV_HOTBACKUP
  /** Reads a requested page asynchronously.

  After the operation is completed, successfully or not, the @p callback
  is called with the result error code, probably in a different thread. If the
  request is successful, it will always be called from a different thread in
  context that has no latches taken.

  Before the page is returned, the page will be decrypted and decompressed,
  depending on information present in the page header.

  The size of the buffer in bytes should be at least the tablespace physical
  page size which was passed to tablespaces_nodes::open() or calculated from
  flags passed to tablespaces_nodes::create().

  @param[in]   req        IO request type, compression and encryption
                          information.
  @param[out]  buffer     A buffer where to read the data in. It must be aligned
                          in memory to physical page size. It must be able to
                          store physical page size bytes. The memory pointed is
                          managed by the caller and must remain valid until the
                          @p callback is being called.
                          TODO : buffer to be replaced with std::span.
  @param[in]   page_no    Offset from the first page in the node to read
                          from.
  @param[in]   callback   A callback to be called exactly once when the result
                          of this IO operation is known. It may be a success if
                          the read or write succeeded or a subset of `dberr_t`
                          errors if the read could not be executed or if it
                          failed. It can be called synchronously in this thread
                          before returning from this method, or can be executed
                          asynchronously from another thread, when @p sync is
                          false, before or after this call returns.
  @return Status_IO::SUCCESS if IO was successfully posted, error code otherwise
  */
  [[nodiscard]] virtual Status_IO read_page_async(IORequest req, byte *buffer,
                                                  Page_number page_no,
                                                  Callback callback) = 0;
#endif /* !UNIV_HOTBACKUP */

  /** Writes a requested page synchronously.

  The page will be encrypted and compressed, below this implementation
  depending on information present in the page header.

  If punch hole is supported and requested, this API implementation punches the
  hole depending on input buffer compression metadata (check Punch Hole
  Optimization), the capability of Implementation and settings in IORequest.

  If compression/encryption is to be done, the input buffer may be modified
  accordingly with post encryption/compression metadata (eg : Page type,
  compression info).

  The size of the buffer in bytes should be at most the tablespaces physical
  page size which was passed to tablespaces_nodes::open() or calculated from
  flags passed to tablespaces_nodes::create(). In other words,
  @p buffer_len must not be greater than the tablespace physical page size.
  In case of `punch hole`, the @p buffer_len can be less than physical page
  size but it must be divisible by 512. And the @p buffer will be written out
  and rest of the page will be hole-punched.

  @param[in]   req         IO request type, compression and encryption
                           information.
  @param[out]  buffer      A buffer containing the data to be written. Data from
                           this buffer might be copied and the copy transformed
                           before writing it. It must be aligned in memory to
                           the OS block size (UNIV_SECTOR_SIZE). The memory
                           pointed is managed by the caller and must remain
                           valid until the call returns.
  @param[in]   buffer_len  Size of the buffer to be written. It always has to be
                           a multiply of OS block size (UNIV_SECTOR_SIZE). In
                           case of write of an already compressed data, it is a
                           length of the compressed data buffer. Otherwise it
                           should be physical page size. Actual number of bytes
                           written can be smaller if the tablespace has
                           compression enabled and data was not compressed
                           already.
                           TODO : buffer and buffer_len to be replaced with
                                  std::span.
  @param[in]   page_no     Offset from the first page in the node to write
                           to.
  @return Status_IO::SUCCESS on successful write, otherwise error code */
  [[nodiscard]] virtual Status_IO write_page(IORequest req, byte *buffer,
                                             size_t buffer_len,
                                             Page_number page_no) = 0;

#ifdef UNIV_LINUX
  /** Writes group of pages synchronously concatenated together at offset
  aligned to physical_page_size.

  @param[in]  buffers            Buffers where pages to be written to storage
                                 are present. Each buffer should be of physical
                                 page size of tablespace. Each buffer must be
                                 aligned in memory to OS block size
                                 (UNIV_SECTOR_SIZE). These buffers are meant to
                                 be written unencrypted and uncompressed. The
                                 memory pointed is managed by the caller and
                                 must remain valid until the call returns.
  @param[in]  first_page_numebr  offset from the first page in the node at
                                 which the writing should start
  @return Status_IO::SUCCESS on successful write, otherwise error code */
  [[nodiscard]] virtual Status_IO write_pages(
      std::span<const byte *> buffers, Page_number first_page_numebr) = 0;
#endif /* UNIV_LINUX */

#ifndef UNIV_HOTBACKUP
  /** Writes a requested page asynchronously.

  After the operation is completed, successfully or not, the @p callback
  is called with the result error code, probably in a different thread. If the
  request is successful, it will always be called from a different thread in
  context that has no latches taken.

  The page will be encrypted and compressed, below this implementation
  depending on information present in the page header.

  If punch hole is supported and requested, this API implementation punches the
  hole depending on input buffer compression metadata (check Punch Hole
  Optimization), the capability of Implementation and settings in IORequest.

  If compression/encryption is to be done, the input buffer may be modified
  accordingly with post encryption/compression metadata (eg : Page type,
  compression info).

  The size of the buffer in bytes should be at most the tablespaces physical
  page size which was passed to tablespaces_nodes::open() or calculated from
  flags passed to tablespaces_nodes::create(). In other words,
  @p buffer_len must not be greater than the tablespace physical page size.
  In case of `punch hole`, the @p buffer_len can be less than physical page
  size but it must be divisible by 512. And the @p buffer will be written out
  and rest of the page will be hole-punched.

  @param[in]   req         IO request type, compression and encryption
                           information.
  @param[out]  buffer      A buffer containing the data to be written. Data from
                           this buffer might be copied and the copy transformed
                           before writing it. It must be aligned in memory to
                           the OS block size (UNIV_SECTOR_SIZE). The memory
                           pointed is managed by the caller and must remain
                           valid until the @p callback is being called.
  @param[out]  buffer_len  Size of the buffer to be written. It must be aligned
                           to OS block size (UNIV_SECTOR_SIZE). In case of write
                           of an already compressed data, it is a length of the
                           compressed data buffer. Otherwise it should be
                           physical page size. Actual number of bytes written
                           can be smaller if the tablespace has compression
                           enabled and data was not compressed already.
                           TODO : buffer and buffer_len to be replaced with
                                  std::span.
  @param[in]   page_no     Offset from the first page in the node to write
                           to.
  @param[in]   callback    A callback to be called exactly once when the result
                           of this IO operation is known. It may be a success if
                           the read or write succeeded or a subset of `dberr_t`
                           errors if the write could not be executed or if it
                           failed. It can be called synchronously in this thread
                           before returning from this method, or can be executed
                           asynchronously from another thread, when @p sync is
                           false, before or after this call returns.
  @return Status_IO::SUCCESS if IO was successfully posted, error code otherwise
  */
  [[nodiscard]] virtual Status_IO write_page_async(IORequest req, byte *buffer,
                                                   size_t buffer_len,
                                                   Page_number page_no,
                                                   Callback callback) = 0;
#endif /* !UNIV_HOTBACKUP */
};

} /* namespace ib::fil */
