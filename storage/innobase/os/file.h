/***********************************************************************

Copyright (c) 1995, 2023, Oracle and/or its affiliates.
Copyright (c) 2009, Percona Inc.

Portions of this file contain modifications contributed and copyrighted
by Percona Inc.. Those modifications are
gratefully acknowledged and are described briefly in the InnoDB
documentation. The contributions by Percona Inc. are incorporated with
their permission, and subject to the conditions contained in the file
COPYING.Percona.

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
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

***********************************************************************/

/** NOTE: The functions in this file should only use functions from
other files in library. The code in this file is used to make a library for
external tools. */

/** @file os/file.h
 The interface to the operating system file io

 Created 10/21/1995 Heikki Tuuri
 *******************************************************/
#ifndef os_file_h
#define os_file_h

#include "univ.i"

/** Compression algorithm. */
struct Compression {
  /** Algorithm types supported */
  enum Type : uint8_t {
    /* Note: During recovery we don't have the compression type
    because the .frm file has not been read yet. Therefore
    we write the recovered pages out without compression. */

    /** No compression */
    NONE = 0,

    /** Use ZLib */
    ZLIB = 1,

    /** Use LZ4 faster variant, usually lower compression. */
    LZ4 = 2
  };

  /** Compressed page meta-data */
  struct meta_t {
    /** Version number */
    uint8_t m_version;

    /** Algorithm type */
    Type m_algorithm;

    /** Original page type */
    uint16_t m_original_type;

    /** Original page size, before compression */
    uint16_t m_original_size;

    /** Size after compression */
    uint16_t m_compressed_size;
  };

  /** Default constructor */
  Compression() : m_type(NONE) {}

  /** Specific constructor
  @param[in]    type            Algorithm type */
  explicit Compression(Type type) : m_type(type) {
#ifdef UNIV_DEBUG
    switch (m_type) {
      case NONE:
      case ZLIB:
      case LZ4:
        break;
      default:
        ut_error;
    }
#endif /* UNIV_DEBUG */
  }

  /** @return string representation. */
  std::string to_string() const {
    std::ostringstream os;

    os << "type: ";
    switch (m_type) {
      case NONE:
        os << "NONE";
        break;
      case ZLIB:
        os << "ZLIB";
        break;
      case LZ4:
        os << "LZ4";
        break;
      default:
        os << "<UNKNOWN>";
        break;
    }

    return (os.str());
  }

  /** Version of compressed page */
  static constexpr uint8_t FIL_PAGE_VERSION_1 = 1;
  static constexpr uint8_t FIL_PAGE_VERSION_2 = 2;

  /** Check the page header type field.
  @param[in]    page            Page contents
  @return true if it is a compressed page */
  [[nodiscard]] static bool is_compressed_page(const byte *page);

  /** Check the page header type field.
  @param[in]    page            Page contents
  @return true if it is a compressed and encrypted page */
  [[nodiscard]] static bool is_compressed_encrypted_page(const byte *page);

  /** Check if the version on page is valid.
  @param[in]    version         version
  @return true if version is valid */
  static bool is_valid_page_version(uint8_t version);

  /** Check whether the compression algorithm is supported.
  @param[in]      algorithm       Compression algorithm to check
  @param[out]     compression            The type that algorithm maps to
  @return DB_SUCCESS or error code */
  [[nodiscard]] static dberr_t check(const char *algorithm,
                                     Compression *compression);

  /** Validate the algorithm string.
  @param[in]      algorithm       Compression algorithm to check
  @return DB_SUCCESS or error code */
  [[nodiscard]] static dberr_t validate(const char *algorithm);

  /** Validate the algorithm string.
  @param[in]  type  compression type
  @return true if type is valid, else false */
  [[nodiscard]] static bool validate(const Type type);

  /** Convert to a "string".
  @param[in]      type            The compression type
  @return the string representation */
  [[nodiscard]] static const char *to_string(Type type);

  /** Convert the meta data to a std::string.
  @param[in]      meta          Page Meta data
  @return the string representation */
  [[nodiscard]] static std::string to_string(const meta_t &meta);

  /** Deserizlise the page header compression meta-data
  @param[in]    page            Pointer to the page header
  @param[out]   control         Deserialised data */
  static void deserialize_header(const byte *page, meta_t *control);

  /** Check if the string is "empty" or "none".
  @param[in]      algorithm       Compression algorithm to check
  @return true if no algorithm requested */
  [[nodiscard]] static bool is_none(const char *algorithm);

  /** Decompress the page data contents. Page type must be FIL_PAGE_COMPRESSED,
  if not then the source contents are left unchanged and DB_SUCCESS is returned.
  @param[in]    dblwr_read      true if double write recovery in progress
  @param[in,out]        src             Data read from disk, decompressed data
  will be copied to this page
  @param[in,out]        dst             Scratch area to use for decompression or
                                  nullptr.
  @param[in]    dst_len         If dst is valid, size of the scratch area in
                                  bytes.
  @return DB_SUCCESS or error code */
  [[nodiscard]] static dberr_t deserialize(bool dblwr_read, byte *src,
                                           byte *dst, ulint dst_len);

  /** Compression type */
  Type m_type;
};
#endif
