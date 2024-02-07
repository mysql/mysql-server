/*****************************************************************************

Copyright (c) 1995, 2024, Oracle and/or its affiliates.

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

*****************************************************************************/

/** @file include/buf0checksum.h
 Buffer pool checksum functions, also linked from /extra/innochecksum.cc

 Created Aug 11, 2011 Vasil Dimov
 *******************************************************/

#ifndef buf0checksum_h
#define buf0checksum_h

#include "buf0types.h"
#include "page0size.h"
#include "univ.i"

/** Calculates the CRC32 checksum of a page. The value is stored to the page
when it is written to a file and also checked for a match when reading from
the file. When reading we allow both normal CRC32 and CRC-legacy-big-endian
variants. Note that we must be careful to calculate the same value on 32-bit
and 64-bit architectures.
@param[in]      page                    buffer page (UNIV_PAGE_SIZE bytes)
@param[in]      use_legacy_big_endian   if true then use big endian
byteorder when converting byte strings to integers
@return checksum */
uint32_t buf_calc_page_crc32(const byte *page,
                             bool use_legacy_big_endian = false);

/** Calculates a page checksum which is stored to the page when it is written
 to a file. Note that we must be careful to calculate the same value on
 32-bit and 64-bit architectures.
 @return checksum */
uint32_t buf_calc_page_new_checksum(const byte *page); /*!< in: buffer page */

/** In versions < 4.0.14 and < 4.1.1 there was a bug that the checksum only
 looked at the first few bytes of the page. This calculates that old
 checksum.
 NOTE: we must first store the new formula checksum to
 FIL_PAGE_SPACE_OR_CHKSUM before calculating and storing this old checksum
 because this takes that field as an input!
 @return checksum */
uint32_t buf_calc_page_old_checksum(const byte *page); /*!< in: buffer page */

/** Return a printable string describing the checksum algorithm.
 @return algorithm name */
const char *buf_checksum_algorithm_name(
    srv_checksum_algorithm_t algo); /*!< in: algorithm */

extern ulong srv_checksum_algorithm;

/** Class to print checksums to log file. */
class BlockReporter {
 public:
  /** Constructor
  @param[in]    check_lsn       check lsn of the page with the
                                  current lsn (only in recovery)
  @param[in]    read_buf        buffer holding the page
  @param[in]    page_size       page size
  @param[in]    skip_checksum   skip checksum verification */
  BlockReporter(bool check_lsn, const byte *read_buf,
                const page_size_t &page_size, bool skip_checksum)
      : m_check_lsn(check_lsn),
        m_read_buf(read_buf),
        m_page_size(page_size),
        m_skip_checksum(skip_checksum) {}

  virtual ~BlockReporter() = default;
  BlockReporter(const BlockReporter &) = default;

  /** Checks if a page is corrupt.
  @retval       true    if page is corrupt
  @retval       false   if page is not corrupt */
  [[nodiscard]] bool is_corrupted() const;

  /** Checks if a page is encrypted.
  @retval       true    if page is encrypted
  @retval       false   if page is not encrypted */
  [[nodiscard]] bool is_encrypted() const noexcept;

  /** Print message if page is empty.
  @param[in]    empty           true if page is empty */
  virtual inline void report_empty_page(bool empty [[maybe_unused]]) const {}

  /** Print crc32 checksum and the checksum fields in page.
  @param[in]    checksum_field1 Checksum in page header
  @param[in]    checksum_field2 Checksum in page trailer
  @param[in]    crc32           Calculated crc32 checksum
  @param[in]    algo            Current checksum algorithm */
  virtual inline void print_strict_crc32(uint32_t checksum_field1
                                         [[maybe_unused]],
                                         uint32_t checksum_field2
                                         [[maybe_unused]],
                                         uint32_t crc32 [[maybe_unused]],
                                         srv_checksum_algorithm_t algo
                                         [[maybe_unused]]) const {}

  /** Print innodb checksum and the checksum fields in page.
  @param[in]    checksum_field1 Checksum in page header
  @param[in]    checksum_field2 Checksum in page trailer */
  virtual inline void print_strict_innodb(uint32_t checksum_field1
                                          [[maybe_unused]],
                                          uint32_t checksum_field2
                                          [[maybe_unused]]) const {}

  /** Print none checksum and the checksum fields in page.
  @param[in]    checksum_field1 Checksum in page header
  @param[in]    checksum_field2 Checksum in page trailer
  @param[in]    algo            Current checksum algorithm */
  virtual inline void print_strict_none(uint32_t checksum_field1
                                        [[maybe_unused]],
                                        uint32_t checksum_field2
                                        [[maybe_unused]],
                                        srv_checksum_algorithm_t algo
                                        [[maybe_unused]]) const {}

  /** Print innodb checksum value stored in page trailer.
  @param[in]    old_checksum    checksum value according to old style
  @param[in]    new_checksum    checksum value according to new style
  @param[in]    checksum_field1 Checksum in page header
  @param[in]    checksum_field2 Checksum in page trailer
  @param[in]    algo            current checksum algorithm */
  virtual inline void print_innodb_checksum(
      uint32_t old_checksum [[maybe_unused]],
      uint32_t new_checksum [[maybe_unused]],
      uint32_t checksum_field1 [[maybe_unused]],
      uint32_t checksum_field2 [[maybe_unused]],
      srv_checksum_algorithm_t algo [[maybe_unused]]) const {}

  /** Print the message that checksum mismatch happened in
  page header. */
  virtual inline void print_innodb_fail() const {}

  /** Print both new-style, old-style & crc32 checksum values.
  @param[in]    checksum_field1 Checksum in page header
  @param[in]    checksum_field2 Checksum in page trailer */
  virtual inline void print_crc32_checksum(uint32_t checksum_field1
                                           [[maybe_unused]],
                                           uint32_t checksum_field2
                                           [[maybe_unused]]) const {}

  /** Print a message that crc32 check failed. */
  virtual inline void print_crc32_fail() const {}

  /** Print a message that none check failed. */
  virtual inline void print_none_fail() const {}

  /** Print checksum values on a compressed page.
  @param[in]    calc    the calculated checksum value
  @param[in]    stored  the stored checksum in header. */
  virtual inline void print_compressed_checksum(uint32_t calc [[maybe_unused]],
                                                uint32_t stored
                                                [[maybe_unused]]) const {}

  /** Verify a compressed page's checksum.
  @retval               true            if stored checksum is valid
  according to the value of srv_checksum_algorithm
  @retval               false           if stored checksum is not valid
  according to the value of srv_checksum_algorithm */
  bool verify_zip_checksum() const;

  /** Calculate the compressed page checksum. This variant
  should be used when only the page_size_t is unknown and
  only physical page_size of compressed page is available.
  @param[in]    read_buf                buffer holding the page
  @param[in]    phys_page_size          physical page size
  @param[in]    algo                    checksum algorithm to use
  @return page checksum */
  uint32_t calc_zip_checksum(const byte *read_buf, ulint phys_page_size,
                             srv_checksum_algorithm_t algo) const;

  /** Calculate the compressed page checksum.
  @param[in]    algo                    checksum algorithm to use
  @return page checksum */
  uint32_t calc_zip_checksum(srv_checksum_algorithm_t algo) const;

  [[nodiscard]] static bool is_lsn_valid(const byte *frame,
                                         uint32_t page_size) noexcept;

 private:
  /** Checks if the page is in innodb checksum format.
  @param[in]    checksum_field1 new checksum field
  @param[in]    checksum_field2 old checksum field
  @param[in]    algo            current checksum algorithm
  @return true if the page is in innodb checksum format. */
  bool is_checksum_valid_innodb(uint32_t checksum_field1,
                                uint32_t checksum_field2,
                                const srv_checksum_algorithm_t algo) const;

  /** Checks if the page is in none checksum format.
  @param[in]    checksum_field1 new checksum field
  @param[in]    checksum_field2 old checksum field
  @param[in]    algo            current checksum algorithm
  @return true if the page is in none checksum format. */
  bool is_checksum_valid_none(uint32_t checksum_field1,
                              uint32_t checksum_field2,
                              const srv_checksum_algorithm_t algo) const;

  /** Checks if the page is in crc32 checksum format.
  @param[in]    checksum_field1         new checksum field
  @param[in]    checksum_field2         old checksum field
  @param[in]    algo                    current checksum algorithm
  @param[in]    use_legacy_big_endian   big endian algorithm
  @return true if the page is in crc32 checksum format. */
  bool is_checksum_valid_crc32(uint32_t checksum_field1,
                               uint32_t checksum_field2,
                               const srv_checksum_algorithm_t algo,
                               bool use_legacy_big_endian) const;

  /** Issue a warning when the checksum that is stored in the page is
  valid, but different than the global setting innodb_checksum_algorithm.
  @param[in]    curr_algo       current checksum algorithm
  @param[in]    page_checksum   page valid checksum
  @param[in]    page_id         page identifier */
  void page_warn_strict_checksum(srv_checksum_algorithm_t curr_algo,
                                 srv_checksum_algorithm_t page_checksum,
                                 const page_id_t &page_id) const;

  [[nodiscard]] space_id_t space_id() const noexcept;
  [[nodiscard]] page_no_t page_no() const noexcept;

 protected:
  /** If true, do a LSN check during innodb recovery. */
  bool m_check_lsn;
  /** Buffer holding the page. */
  const byte *m_read_buf;
  /** Page size. */
  const page_size_t &m_page_size;
  /** Skip checksum verification but compare only data. */
  bool m_skip_checksum;
};

#endif /* buf0checksum_h */
