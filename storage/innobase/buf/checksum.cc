/*****************************************************************************

Copyright (c) 1995, 2018, Oracle and/or its affiliates. All Rights Reserved.

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

/** @file buf/checksum.cc
 Buffer pool checksum functions, also linked from /extra/innochecksum.cc

 Created Aug 11, 2011 Vasil Dimov
 *******************************************************/

#include <sys/types.h>
#include <zlib.h>

#include "buf0buf.h"
#include "buf0types.h"
#include "fil0fil.h"
#include "mach0data.h"
#include "my_dbug.h"
#include "page0size.h"
#include "srv0srv.h"
#include "univ.i"
#include "ut0crc32.h"
#include "ut0rnd.h"
#ifdef UNIV_HOTBACKUP
#include "buf0checksum.h"
#endif /* UNIV_HOTBACKUP */

/** the macro MYSQL_SYSVAR_ENUM() requires "long unsigned int" and if we
use srv_checksum_algorithm_t here then we get a compiler error:
ha_innodb.cc:12251: error: cannot convert 'srv_checksum_algorithm_t*' to
  'long unsigned int*' in initialization */
ulong srv_checksum_algorithm = SRV_CHECKSUM_ALGORITHM_INNODB;

/** set if we have found pages matching legacy big endian checksum */
static bool legacy_big_endian_checksum = false;

/** Calculates the CRC32 checksum of a page. The value is stored to the page
when it is written to a file and also checked for a match when reading from
the file. When reading we allow both normal CRC32 and CRC-legacy-big-endian
variants. Note that we must be careful to calculate the same value on 32-bit
and 64-bit architectures.
@param[in]	page			buffer page (UNIV_PAGE_SIZE bytes)
@param[in]	use_legacy_big_endian	if true then use big endian
byteorder when converting byte strings to integers
@return checksum */
uint32_t buf_calc_page_crc32(const byte *page,
                             bool use_legacy_big_endian /* = false */) {
  /* Since the field FIL_PAGE_FILE_FLUSH_LSN, and in versions <= 4.1.x
  FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID, are written outside the buffer pool
  to the first pages of data files, we have to skip them in the page
  checksum calculation.
  We must also skip the field FIL_PAGE_SPACE_OR_CHKSUM where the
  checksum is stored, and also the last 8 bytes of page because
  there we store the old formula checksum. */

  ut_crc32_func_t crc32_func =
      use_legacy_big_endian ? ut_crc32_legacy_big_endian : ut_crc32;

  const uint32_t c1 = crc32_func(page + FIL_PAGE_OFFSET,
                                 FIL_PAGE_FILE_FLUSH_LSN - FIL_PAGE_OFFSET);

  const uint32_t c2 =
      crc32_func(page + FIL_PAGE_DATA,
                 UNIV_PAGE_SIZE - FIL_PAGE_DATA - FIL_PAGE_END_LSN_OLD_CHKSUM);

  return (c1 ^ c2);
}

/** Calculates a page checksum which is stored to the page when it is written
 to a file. Note that we must be careful to calculate the same value on
 32-bit and 64-bit architectures.
 @return checksum */
ulint buf_calc_page_new_checksum(const byte *page) /*!< in: buffer page */
{
  ulint checksum;

  /* Since the field FIL_PAGE_FILE_FLUSH_LSN, and in versions <= 4.1.x
  FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID, are written outside the buffer pool
  to the first pages of data files, we have to skip them in the page
  checksum calculation.
  We must also skip the field FIL_PAGE_SPACE_OR_CHKSUM where the
  checksum is stored, and also the last 8 bytes of page because
  there we store the old formula checksum. */

  checksum =
      ut_fold_binary(page + FIL_PAGE_OFFSET,
                     FIL_PAGE_FILE_FLUSH_LSN - FIL_PAGE_OFFSET) +
      ut_fold_binary(page + FIL_PAGE_DATA, UNIV_PAGE_SIZE - FIL_PAGE_DATA -
                                               FIL_PAGE_END_LSN_OLD_CHKSUM);
  checksum = checksum & 0xFFFFFFFFUL;

  return (checksum);
}

/** In versions < 4.0.14 and < 4.1.1 there was a bug that the checksum only
 looked at the first few bytes of the page. This calculates that old
 checksum.
 NOTE: we must first store the new formula checksum to
 FIL_PAGE_SPACE_OR_CHKSUM before calculating and storing this old checksum
 because this takes that field as an input!
 @return checksum */
ulint buf_calc_page_old_checksum(const byte *page) /*!< in: buffer page */
{
  ulint checksum;

  checksum = ut_fold_binary(page, FIL_PAGE_FILE_FLUSH_LSN);

  checksum = checksum & 0xFFFFFFFFUL;

  return (checksum);
}

/** Return a printable string describing the checksum algorithm.
 @return algorithm name */
const char *buf_checksum_algorithm_name(
    srv_checksum_algorithm_t algo) /*!< in: algorithm */
{
  switch (algo) {
    case SRV_CHECKSUM_ALGORITHM_CRC32:
      return ("crc32");
    case SRV_CHECKSUM_ALGORITHM_STRICT_CRC32:
      return ("strict_crc32");
    case SRV_CHECKSUM_ALGORITHM_INNODB:
      return ("innodb");
    case SRV_CHECKSUM_ALGORITHM_STRICT_INNODB:
      return ("strict_innodb");
    case SRV_CHECKSUM_ALGORITHM_NONE:
      return ("none");
    case SRV_CHECKSUM_ALGORITHM_STRICT_NONE:
      return ("strict_none");
  }

  ut_error;
}

/** Do lsn checks on a page during innodb recovery.
@param[in]	check_lsn	if recv_lsn_checks_on & check_lsn
                                perform lsn check
@param[in]	read_buf	buffer containing the page. */
inline void buf_page_lsn_check(bool check_lsn, const byte *read_buf) {
#if !defined(UNIV_HOTBACKUP) && !defined(UNIV_LIBRARY)
  if (check_lsn && recv_lsn_checks_on) {
    lsn_t current_lsn;
    const lsn_t page_lsn = mach_read_from_8(read_buf + FIL_PAGE_LSN);

    /* Since we are going to reset the page LSN during the import
    phase it makes no sense to spam the log with error messages. */
    current_lsn = log_get_lsn(*log_sys);

    if (current_lsn < page_lsn) {
      const space_id_t space_id =
          mach_read_from_4(read_buf + FIL_PAGE_SPACE_ID);
      const page_no_t page_no = mach_read_from_4(read_buf + FIL_PAGE_OFFSET);

      auto space = fil_space_get(space_id);

#ifdef UNIV_NO_ERR_MSGS
      ib::error()
#else
      ib::error(ER_IB_MSG_146)
#endif /* UNIV_NO_ERR_MSGS */
          << "Tablespace '" << space->name << "'"
          << " Page " << page_id_t(space_id, page_no) << " log sequence number "
          << page_lsn << " is in the future! Current system"
          << " log sequence number " << current_lsn << ".";

#ifdef UNIV_NO_ERR_MSGS
      ib::error()
#else
      ib::error(ER_IB_MSG_147)
#endif /* UNIV_NO_ERR_MSGS */
          << "Your database may be corrupt or you may have copied the InnoDB"
          << " tablespace but not the InnoDB log files. " << FORCE_RECOVERY_MSG;
    }
  }
#endif /* !UNIV_HOTBACKUP && !UNIV_LIBRARY */
}

/** Checks if the page is in innodb checksum format.
@param[in]	checksum_field1	new checksum field
@param[in]	checksum_field2	old checksum field
@param[in]	algo		current checksum algorithm
@return true if the page is in innodb checksum format. */
bool BlockReporter::is_checksum_valid_innodb(
    ulint checksum_field1, ulint checksum_field2,
    const srv_checksum_algorithm_t algo) const {
  /* There are 2 valid formulas for
  checksum_field2 (old checksum field) which algo=innodb could have
  written to the page:

  1. Very old versions of InnoDB only stored 8 byte lsn to the
  start and the end of the page.

  2. Newer InnoDB versions store the old formula checksum
  (buf_calc_page_old_checksum()). */

  ulint old_checksum = buf_calc_page_old_checksum(m_read_buf);
  ulint new_checksum = buf_calc_page_new_checksum(m_read_buf);

  print_innodb_checksum(old_checksum, new_checksum, checksum_field1,
                        checksum_field2, algo);

  if (checksum_field2 != mach_read_from_4(m_read_buf + FIL_PAGE_LSN) &&
      checksum_field2 != old_checksum) {
    return (false);
  }

  /* old field is fine, check the new field */

  /* InnoDB versions < 4.0.14 and < 4.1.1 stored the space id
  (always equal to 0), to FIL_PAGE_SPACE_OR_CHKSUM */

  return (checksum_field1 == 0 || checksum_field1 == new_checksum);
}

/** Checks if the page is in none checksum format.
@param[in]	checksum_field1	new checksum field
@param[in]	checksum_field2	old checksum field
@param[in]	algo		current checksum algorithm
@return true if the page is in none checksum format. */
bool BlockReporter::is_checksum_valid_none(
    ulint checksum_field1, ulint checksum_field2,
    const srv_checksum_algorithm_t algo) const {
  print_strict_none(checksum_field1, checksum_field2, algo);

  return (checksum_field1 == checksum_field2 &&
          checksum_field1 == BUF_NO_CHECKSUM_MAGIC);
}

/** Checks if the page is in crc32 checksum format.
@param[in]	checksum_field1		new checksum field
@param[in]	checksum_field2		old checksum field
@param[in]	algo			current checksum algorithm
@param[in]	use_legacy_big_endian	big endian algorithm
@return true if the page is in crc32 checksum format. */
bool BlockReporter::is_checksum_valid_crc32(ulint checksum_field1,
                                            ulint checksum_field2,
                                            const srv_checksum_algorithm_t algo,
                                            bool use_legacy_big_endian) const {
  if (checksum_field1 != checksum_field2) {
    return (false);
  }

  uint32_t crc32 = buf_calc_page_crc32(m_read_buf, use_legacy_big_endian);

  print_strict_crc32(checksum_field1, checksum_field2, crc32, algo);

  return (checksum_field1 == crc32);
}

/** Checks if a page is corrupt.
@retval	true	if page is corrupt
@retval	false	if page is not corrupt */
bool BlockReporter::is_corrupted() const {
  ulint checksum_field1;
  ulint checksum_field2;

  if (!m_page_size.is_compressed() &&
      memcmp(
          m_read_buf + FIL_PAGE_LSN + 4,
          m_read_buf + m_page_size.logical() - FIL_PAGE_END_LSN_OLD_CHKSUM + 4,
          4)) {
    /* Stored log sequence numbers at the start and the end
    of page do not match */

    return (true);
  }

  buf_page_lsn_check(m_check_lsn, m_read_buf);

  /* Check whether the checksum fields have correct values */

  if (srv_checksum_algorithm == SRV_CHECKSUM_ALGORITHM_NONE ||
      m_skip_checksum) {
    return (false);
  }

  if (m_page_size.is_compressed()) {
    return (!verify_zip_checksum());
  }

  checksum_field1 = mach_read_from_4(m_read_buf + FIL_PAGE_SPACE_OR_CHKSUM);

  checksum_field2 = mach_read_from_4(m_read_buf + m_page_size.logical() -
                                     FIL_PAGE_END_LSN_OLD_CHKSUM);

#if FIL_PAGE_LSN % 8
#error "FIL_PAGE_LSN must be 64 bit aligned"
#endif

  /* declare empty pages non-corrupted */
  if (checksum_field1 == 0 && checksum_field2 == 0 &&
      *reinterpret_cast<const ib_uint64_t *>(m_read_buf + FIL_PAGE_LSN) == 0) {
    /* make sure that the page is really empty */

    bool empty = true;
#ifndef UNIV_HOTBACKUP
    for (ulint i = 0; i < m_page_size.logical(); i++) {
      /* The FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID has been
      repurposed for page compression. It can be
      set for uncompressed empty pages. */

      if ((i < FIL_PAGE_FILE_FLUSH_LSN ||
           i >= FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID) &&
          m_read_buf[i] != 0) {
        empty = false;
        break;
      }
    }

    report_empty_page(empty);

#endif /* !UNIV_HOTBACKUP */
    return (!empty);
  }

  const page_id_t page_id(mach_read_from_4(m_read_buf + FIL_PAGE_SPACE_ID),
                          mach_read_from_4(m_read_buf + FIL_PAGE_OFFSET));

  DBUG_EXECUTE_IF("buf_page_import_corrupt_failure", return (true););
  const srv_checksum_algorithm_t curr_algo =
      static_cast<srv_checksum_algorithm_t>(srv_checksum_algorithm);

  bool legacy_checksum_checked = false;

  switch (curr_algo) {
    case SRV_CHECKSUM_ALGORITHM_CRC32:
    case SRV_CHECKSUM_ALGORITHM_STRICT_CRC32:

      if (is_checksum_valid_crc32(checksum_field1, checksum_field2, curr_algo,
                                  false)) {
        return (false);
      }

      if (is_checksum_valid_none(checksum_field1, checksum_field2, curr_algo)) {
#ifndef UNIV_HOTBACKUP
        if (curr_algo == SRV_CHECKSUM_ALGORITHM_STRICT_CRC32) {
          page_warn_strict_checksum(curr_algo, SRV_CHECKSUM_ALGORITHM_NONE,
                                    page_id);
        }

        print_crc32_checksum(checksum_field1, checksum_field2);
#endif /* !UNIV_HOTBACKUP */
        return (false);
      }

      /* We need to check whether the stored checksum matches legacy
      big endian checksum or Innodb checksum. We optimize the order
      based on earlier results. if earlier we have found pages
      matching legacy big endian checksum, we try to match it first.
      Otherwise we check innodb checksum first. */
      if (legacy_big_endian_checksum) {
        if (is_checksum_valid_crc32(checksum_field1, checksum_field2, curr_algo,
                                    true)) {
          return (false);
        }
        legacy_checksum_checked = true;
      }

      if (is_checksum_valid_innodb(checksum_field1, checksum_field2,
                                   curr_algo)) {
#ifndef UNIV_HOTBACKUP
        if (curr_algo == SRV_CHECKSUM_ALGORITHM_STRICT_CRC32) {
          page_warn_strict_checksum(curr_algo, SRV_CHECKSUM_ALGORITHM_INNODB,
                                    page_id);
        }
#endif /* !UNIV_HOTBACKUP */
        return (false);
      }

      /* If legacy checksum is not checked, do it now. */
      if (!legacy_checksum_checked &&
          is_checksum_valid_crc32(checksum_field1, checksum_field2, curr_algo,
                                  true)) {
        legacy_big_endian_checksum = true;
        return (false);
      }

      print_crc32_fail();
      return (true);

    case SRV_CHECKSUM_ALGORITHM_INNODB:
    case SRV_CHECKSUM_ALGORITHM_STRICT_INNODB:

      if (is_checksum_valid_innodb(checksum_field1, checksum_field2,
                                   curr_algo)) {
        return (false);
      }

      if (is_checksum_valid_none(checksum_field1, checksum_field2, curr_algo)) {
#ifndef UNIV_HOTBACKUP
        if (curr_algo == SRV_CHECKSUM_ALGORITHM_STRICT_INNODB) {
          page_warn_strict_checksum(curr_algo, SRV_CHECKSUM_ALGORITHM_NONE,
                                    page_id);
        }

        print_strict_innodb(checksum_field1, checksum_field2);
#endif /* !UNIV_HOTBACKUP */
        return (false);
      }

      if (is_checksum_valid_crc32(checksum_field1, checksum_field2, curr_algo,
                                  false) ||
          is_checksum_valid_crc32(checksum_field1, checksum_field2, curr_algo,
                                  true)) {
#ifndef UNIV_HOTBACKUP
        if (curr_algo == SRV_CHECKSUM_ALGORITHM_STRICT_INNODB) {
          page_warn_strict_checksum(curr_algo, SRV_CHECKSUM_ALGORITHM_CRC32,
                                    page_id);
        }
#endif /* !UNIV_HOTBACKUP */

        return (false);
      }

      print_innodb_fail();
      return (true);

    case SRV_CHECKSUM_ALGORITHM_STRICT_NONE:

      if (is_checksum_valid_none(checksum_field1, checksum_field2, curr_algo)) {
        return (false);
      }

      if (is_checksum_valid_crc32(checksum_field1, checksum_field2, curr_algo,
                                  false) ||
          is_checksum_valid_crc32(checksum_field1, checksum_field2, curr_algo,
                                  true)) {
#ifndef UNIV_HOTBACKUP
        page_warn_strict_checksum(curr_algo, SRV_CHECKSUM_ALGORITHM_CRC32,
                                  page_id);
#endif /* !UNIV_HOTBACKUP */
        return (false);
      }

      if (is_checksum_valid_innodb(checksum_field1, checksum_field2,
                                   curr_algo)) {
#ifndef UNIV_HOTBACKUP
        page_warn_strict_checksum(curr_algo, SRV_CHECKSUM_ALGORITHM_INNODB,
                                  page_id);
#endif /* !UNIV_HOTBACKUP */
        return (false);
      }

      print_none_fail();
      return (true);

    case SRV_CHECKSUM_ALGORITHM_NONE:
      /* should have returned FALSE earlier */
      break;
      /* no default so the compiler will emit a warning if new enum
      is added and not handled here */
  }

  ut_error;
  return (false);
}

/** Calculate the compressed page checksum.
@param[in]	algo			checksum algorithm to use
@param[in]	use_legacy_big_endian	only used if algo is
SRV_CHECKSUM_ALGORITHM_CRC32 or SRV_CHECKSUM_ALGORITHM_STRICT_CRC32 -
if true then use big endian byteorder when converting byte strings to
integers.
@return page checksum */
uint32_t BlockReporter::calc_zip_checksum(
    srv_checksum_algorithm_t algo,
    bool use_legacy_big_endian /* = false */) const {
  return (calc_zip_checksum(m_read_buf, m_page_size.physical(), algo,
                            use_legacy_big_endian));
}

/** Calculate the compressed page checksum. This variant
should be used when only the page_size_t is unknown and
only physical page_size of compressed page is available
@param[in]	read_buf		buffer holding the page
@param[in]	phys_page_size		physical page size
@param[in]	algo			checksum algorithm to use
@param[in]	use_legacy_big_endian	only used if algo is
SRV_CHECKSUM_ALGORITHM_CRC32 or SRV_CHECKSUM_ALGORITHM_STRICT_CRC32 -
if true then use big endian byteorder when converting byte strings to
integers.
@return page checksum */
uint32_t BlockReporter::calc_zip_checksum(
    const byte *read_buf, ulint phys_page_size, srv_checksum_algorithm_t algo,
    bool use_legacy_big_endian /* = false */) const {
  uLong adler;
  ib_uint32_t crc32;
  const Bytef *s = read_buf;
  const ulint size = phys_page_size;

  /* Exclude FIL_PAGE_SPACE_OR_CHKSUM, FIL_PAGE_LSN,
  and FIL_PAGE_FILE_FLUSH_LSN from the checksum. */

  switch (algo) {
    case SRV_CHECKSUM_ALGORITHM_CRC32:
    case SRV_CHECKSUM_ALGORITHM_STRICT_CRC32:

      ut_ad(size > FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID);

      crc32 = ut_crc32(s + FIL_PAGE_OFFSET, FIL_PAGE_LSN - FIL_PAGE_OFFSET) ^
              ut_crc32(s + FIL_PAGE_TYPE, 2) ^
              ut_crc32(s + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID,
                       size - FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID);

      return (crc32);
    case SRV_CHECKSUM_ALGORITHM_INNODB:
    case SRV_CHECKSUM_ALGORITHM_STRICT_INNODB:
      ut_ad(size > FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID);

      adler = adler32(0L, s + FIL_PAGE_OFFSET, FIL_PAGE_LSN - FIL_PAGE_OFFSET);
      adler = adler32(adler, s + FIL_PAGE_TYPE, 2);
      adler =
          adler32(adler, s + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID,
                  static_cast<uInt>(size) - FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID);

      return ((ib_uint32_t)adler);
    case SRV_CHECKSUM_ALGORITHM_NONE:
    case SRV_CHECKSUM_ALGORITHM_STRICT_NONE:
      return (BUF_NO_CHECKSUM_MAGIC);
      /* no default so the compiler will emit a warning if new enum
      is added and not handled here */
  }

  ut_error;
}

/** Verify a compressed page's checksum.
@retval		true		if stored checksum is valid according
to the value of srv_checksum_algorithm
@retval		false		if stored schecksum is not valid according
to the value of srv_checksum_algorithm */
bool BlockReporter::verify_zip_checksum() const {
  const uint32_t stored = static_cast<uint32_t>(
      mach_read_from_4(m_read_buf + FIL_PAGE_SPACE_OR_CHKSUM));

#if FIL_PAGE_LSN % 8
#error "FIL_PAGE_LSN must be 64 bit aligned"
#endif

  /* Check if page is empty */
  if (stored == 0 &&
      *reinterpret_cast<const ib_uint64_t *>(m_read_buf + FIL_PAGE_LSN) == 0) {
    /* make sure that the page is really empty */

    ulint i;
    bool empty = true;
    for (i = 0; i < m_page_size.physical(); i++) {
      if (*((const char *)m_read_buf + i) != 0) {
        empty = false;
        break;
      }
    }

    report_empty_page(empty);

    /* Empty page */
    return (empty);
  }

  const srv_checksum_algorithm_t curr_algo =
      static_cast<srv_checksum_algorithm_t>(srv_checksum_algorithm);
  if (curr_algo == SRV_CHECKSUM_ALGORITHM_NONE) {
    return (true);
  }

  page_no_t page_no = mach_read_from_4(m_read_buf + FIL_PAGE_OFFSET);
  space_id_t space_id = mach_read_from_4(m_read_buf + FIL_PAGE_SPACE_ID);
  const page_id_t page_id(space_id, page_no);

  const uint32_t calc = calc_zip_checksum(curr_algo);

  print_compressed_checksum(calc, stored);

  if (stored == calc) {
    return (true);
  }

  bool legacy_checksum_checked = false;

  switch (curr_algo) {
    case SRV_CHECKSUM_ALGORITHM_STRICT_CRC32:
    case SRV_CHECKSUM_ALGORITHM_CRC32:

      if (stored == BUF_NO_CHECKSUM_MAGIC) {
        if (curr_algo == SRV_CHECKSUM_ALGORITHM_STRICT_CRC32) {
          page_warn_strict_checksum(curr_algo, SRV_CHECKSUM_ALGORITHM_NONE,
                                    page_id);
        }

        return (true);
      }

      /* We need to check whether the stored checksum matches legacy
      big endian checksum or Innodb checksum. We optimize the order
      based on earlier results. if earlier we have found pages
      matching legacy big endian checksum, we try to match it first.
      Otherwise we check innodb checksum first. */
      if (legacy_big_endian_checksum) {
        if (stored == calc_zip_checksum(SRV_CHECKSUM_ALGORITHM_CRC32, true)) {
          return (true);
        }
        legacy_checksum_checked = true;
      }

      if (stored == calc_zip_checksum(SRV_CHECKSUM_ALGORITHM_INNODB)) {
        if (curr_algo == SRV_CHECKSUM_ALGORITHM_STRICT_CRC32) {
          page_warn_strict_checksum(curr_algo, SRV_CHECKSUM_ALGORITHM_INNODB,
                                    page_id);
        }

        return (true);
      }

      /* If legacy checksum is not checked, do it now. */
      if (!legacy_checksum_checked &&
          stored == calc_zip_checksum(SRV_CHECKSUM_ALGORITHM_CRC32, true)) {
        /* This page's checksum has been created by the
        legacy software CRC32 implementation on big endian
        CPUs which generates a different result than the
        normal CRC32. */
        legacy_big_endian_checksum = true;
        return (true);
      }

      break;
    case SRV_CHECKSUM_ALGORITHM_STRICT_INNODB:
    case SRV_CHECKSUM_ALGORITHM_INNODB:

      if (stored == BUF_NO_CHECKSUM_MAGIC) {
        if (curr_algo == SRV_CHECKSUM_ALGORITHM_STRICT_INNODB) {
          page_warn_strict_checksum(curr_algo, SRV_CHECKSUM_ALGORITHM_NONE,
                                    page_id);
        }
        return (true);
      }

      if (stored == calc_zip_checksum(SRV_CHECKSUM_ALGORITHM_CRC32) ||
          stored == calc_zip_checksum(SRV_CHECKSUM_ALGORITHM_CRC32, true)) {
        if (curr_algo == SRV_CHECKSUM_ALGORITHM_STRICT_INNODB) {
          page_warn_strict_checksum(curr_algo, SRV_CHECKSUM_ALGORITHM_CRC32,
                                    page_id);
        }
        return (true);
      }

      break;
    case SRV_CHECKSUM_ALGORITHM_STRICT_NONE:

      if (stored == calc_zip_checksum(SRV_CHECKSUM_ALGORITHM_CRC32) ||
          stored == calc_zip_checksum(SRV_CHECKSUM_ALGORITHM_CRC32, true)) {
        page_warn_strict_checksum(curr_algo, SRV_CHECKSUM_ALGORITHM_CRC32,
                                  page_id);
        return (true);
      }

      if (stored == calc_zip_checksum(SRV_CHECKSUM_ALGORITHM_INNODB)) {
        page_warn_strict_checksum(curr_algo, SRV_CHECKSUM_ALGORITHM_INNODB,
                                  page_id);
        return (true);
      }

      break;
    case SRV_CHECKSUM_ALGORITHM_NONE:
      ut_error;
      /* no default so the compiler will emit a warning if new enum
      is added and not handled here */
  }

  return (false);
}

/** Issue a warning when the checksum that is stored in the page is valid,
but different than the global setting innodb_checksum_algorithm.
@param[in]	curr_algo	current checksum algorithm
@param[in]	page_checksum	page valid checksum
@param[in]	page_id		page identifier */
void BlockReporter::page_warn_strict_checksum(
    srv_checksum_algorithm_t curr_algo, srv_checksum_algorithm_t page_checksum,
    const page_id_t &page_id) const {
  srv_checksum_algorithm_t curr_algo_nonstrict;
  switch (curr_algo) {
    case SRV_CHECKSUM_ALGORITHM_STRICT_CRC32:
      curr_algo_nonstrict = SRV_CHECKSUM_ALGORITHM_CRC32;
      break;
    case SRV_CHECKSUM_ALGORITHM_STRICT_INNODB:
      curr_algo_nonstrict = SRV_CHECKSUM_ALGORITHM_INNODB;
      break;
    case SRV_CHECKSUM_ALGORITHM_STRICT_NONE:
      curr_algo_nonstrict = SRV_CHECKSUM_ALGORITHM_NONE;
      break;
    default:
      ut_error;
  }

#ifdef UNIV_NO_ERR_MSGS
  ib::warn()
#else
  ib::warn(ER_IB_MSG_148)
#endif /* UNIV_NO_ERR_MSGS */

      << "innodb_checksum_algorithm is set to \""
      << buf_checksum_algorithm_name(curr_algo) << "\""
      << " but the page " << page_id << " contains a valid checksum \""
      << buf_checksum_algorithm_name(page_checksum) << "\". "
      << " Accepting the page as valid. Change"
      << " innodb_checksum_algorithm to \""
      << buf_checksum_algorithm_name(curr_algo_nonstrict)
      << "\" to silently accept such pages or rewrite all pages"
      << " so that they contain \""
      << buf_checksum_algorithm_name(curr_algo_nonstrict) << "\" checksum.";
}

/** Print the given page_id_t object.
@param[in,out]	out	the output stream
@param[in]	page_id	the page_id_t object to be printed
@return the output stream */
std::ostream &operator<<(std::ostream &out, const page_id_t &page_id) {
  out << "[page id: space=" << page_id.m_space
      << ", page number=" << page_id.m_page_no << "]";
  return (out);
}
