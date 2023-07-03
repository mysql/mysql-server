/*****************************************************************************

Copyright (c) 1995, 2022, Oracle and/or its affiliates.

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

/** @file include/fil0types.h
 The low-level file system page header & trailer offsets

 Created 10/25/1995 Heikki Tuuri
 *******************************************************/

#ifndef fil0types_h
#define fil0types_h

#include "univ.i"

/** The byte offsets on a file page for various variables. */

/** MySQL-4.0.14 space id the page belongs to (== 0) but in later
versions the 'new' checksum of the page */
constexpr uint32_t FIL_PAGE_SPACE_OR_CHKSUM = 0;

/** page offset inside space */
constexpr uint32_t FIL_PAGE_OFFSET = 4;

/** if there is a 'natural' predecessor of the page, its offset.
Otherwise FIL_NULL. This field is not set on BLOB pages, which are stored as a
singly-linked list. See also FIL_PAGE_NEXT. */
constexpr uint32_t FIL_PAGE_PREV = 8;

/** On page 0 of the tablespace, this is the server version ID */
constexpr uint32_t FIL_PAGE_SRV_VERSION = 8;

/** if there is a 'natural' successor of the page, its offset. Otherwise
FIL_NULL. B-tree index pages(FIL_PAGE_TYPE contains FIL_PAGE_INDEX) on the
same PAGE_LEVEL are maintained as a doubly linked list via FIL_PAGE_PREV and
FIL_PAGE_NEXT in the collation order of the smallest user record on each
page. */
constexpr uint32_t FIL_PAGE_NEXT = 12;

/** On page 0 of the tablespace, this is the server version ID */
constexpr uint32_t FIL_PAGE_SPACE_VERSION = 12;

/** lsn of the end of the newest modification log record to the page */
constexpr uint32_t FIL_PAGE_LSN = 16;

/** file page type: FIL_PAGE_INDEX,..., 2 bytes. The contents of this field
can only be trusted in the following case: if the page is an uncompressed
B-tree index page, then it is guaranteed that the value is FIL_PAGE_INDEX.
The opposite does not hold.

In tablespaces created by MySQL/InnoDB 5.1.7 or later, the contents of this
field is valid for all uncompressed pages. */
constexpr uint32_t FIL_PAGE_TYPE = 24;

/** this is only defined for the first page of the system tablespace: the file
has been flushed to disk at least up to this LSN. For FIL_PAGE_COMPRESSED
pages, we store the compressed page control information in these 8 bytes. */
constexpr uint32_t FIL_PAGE_FILE_FLUSH_LSN = 26;

/** If page type is FIL_PAGE_COMPRESSED then the 8 bytes starting at
FIL_PAGE_FILE_FLUSH_LSN are broken down as follows: */

/** Control information version format (u8) */
constexpr uint32_t FIL_PAGE_VERSION = FIL_PAGE_FILE_FLUSH_LSN;

/** Compression algorithm (u8) */
constexpr uint32_t FIL_PAGE_ALGORITHM_V1 = FIL_PAGE_VERSION + 1;

/** Original page type (u16) */
constexpr uint32_t FIL_PAGE_ORIGINAL_TYPE_V1 = FIL_PAGE_ALGORITHM_V1 + 1;

/** Original data size in bytes (u16)*/
constexpr uint32_t FIL_PAGE_ORIGINAL_SIZE_V1 = FIL_PAGE_ORIGINAL_TYPE_V1 + 2;

/** Size after compression (u16) */
constexpr uint32_t FIL_PAGE_COMPRESS_SIZE_V1 = FIL_PAGE_ORIGINAL_SIZE_V1 + 2;

/** This overloads FIL_PAGE_FILE_FLUSH_LSN for RTREE Split Sequence Number */
constexpr uint32_t FIL_RTREE_SPLIT_SEQ_NUM = FIL_PAGE_FILE_FLUSH_LSN;

/** starting from 4.1.x this contains the space id of the page */
constexpr uint32_t FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID = 34;

/** alias for space id */
constexpr uint32_t FIL_PAGE_SPACE_ID = FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID;

/** start of the data on the page */
constexpr uint32_t FIL_PAGE_DATA = 38;

/** File page trailer */
/** the low 4 bytes of this are used to store the page checksum, the
last 4 bytes should be identical to the last 4 bytes of FIL_PAGE_LSN */
constexpr uint32_t FIL_PAGE_END_LSN_OLD_CHKSUM = 8;

/** size of the page trailer */
constexpr uint32_t FIL_PAGE_DATA_END = 8;

/** First in address is the page offset. */
constexpr size_t FIL_ADDR_PAGE = 0;

/** Then comes 2-byte byte offset within page.*/
constexpr size_t FIL_ADDR_BYTE = 4;

/** Address size is 6 bytes. */
constexpr size_t FIL_ADDR_SIZE = 6;

/** Path separator e.g., 'dir;...;dirN' */
constexpr char FIL_PATH_SEPARATOR = ';';

/** A wrapper class to help print and inspect the file page header. */
struct Fil_page_header {
  /** The constructor that takes a pointer to page header as argument.
  @param[in]  frame  the pointer to the page header. */
  explicit Fil_page_header(const byte *frame) : m_frame(frame) {}

  /** Get the space id from the page header.
  @return the space identifier. */
  [[nodiscard]] space_id_t get_space_id() const noexcept;

  /** Get the page number from the page header.
  @return the page number. */
  [[nodiscard]] page_no_t get_page_no() const noexcept;

  /** Get the page type from the page header.
  @return the page type. */
  [[nodiscard]] uint16_t get_page_type() const noexcept;

  /** Print the page header to the given output stream.
  @param[in]  out  the output stream.
  @return the output stream. */
  std::ostream &print(std::ostream &out) const noexcept;

 private:
  /** Pointer to the page header. */
  const byte *m_frame{};
};

/** Overload the global output operator to handle an object of type
Fil_page_header.
@param[in]  out      the output stream.
@param[in]  header   an object of type Fil_page_header.
@return the output stream. */
inline std::ostream &operator<<(std::ostream &out,
                                const Fil_page_header &header) noexcept {
  return (header.print(out));
}

#endif /* fil0types_h */
