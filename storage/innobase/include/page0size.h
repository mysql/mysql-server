/*****************************************************************************

Copyright (c) 2013, 2022, Oracle and/or its affiliates.

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

/** @file include/page0size.h
 A class describing a page size.

 Created Nov 14, 2013 Vasil Dimov
 *******************************************************/

#ifndef page0size_t
#define page0size_t

#include "fsp0types.h"

constexpr size_t FIELD_REF_SIZE = 20;

/** A BLOB field reference full of zero, for use in assertions and
tests.Initially, BLOB field references are set to zero, in
dtuple_convert_big_rec(). */
extern const byte field_ref_zero[FIELD_REF_SIZE];

constexpr size_t PAGE_SIZE_T_SIZE_BITS = 17;

/** Page size descriptor. Contains the physical and logical page size, as well
as whether the page is compressed or not. */
class page_size_t {
 public:
  /** Constructor from (physical, logical, is_compressed).
  @param[in]    physical        physical (on-disk/zipped) page size
  @param[in]    logical         logical (in-memory/unzipped) page size
  @param[in]    is_compressed   whether the page is compressed */
  page_size_t(uint32_t physical, uint32_t logical, bool is_compressed) {
    if (physical == 0) {
      physical = UNIV_PAGE_SIZE_ORIG;
    }
    if (logical == 0) {
      logical = UNIV_PAGE_SIZE_ORIG;
    }

    m_physical = static_cast<unsigned>(physical);
    m_logical = static_cast<unsigned>(logical);
    m_is_compressed = static_cast<unsigned>(is_compressed);

    ut_ad(physical <= (1 << PAGE_SIZE_T_SIZE_BITS));
    ut_ad(logical <= (1 << PAGE_SIZE_T_SIZE_BITS));

    ut_ad(ut_is_2pow(physical));
    ut_ad(ut_is_2pow(logical));

    ut_ad(logical <= UNIV_PAGE_SIZE_MAX);
    ut_ad(logical >= physical);
    ut_ad(!is_compressed || physical <= UNIV_ZIP_SIZE_MAX);
  }

  /** Constructor from (fsp_flags).
  @param[in]    fsp_flags       filespace flags */
  explicit page_size_t(uint32_t fsp_flags) {
    uint32_t ssize = FSP_FLAGS_GET_PAGE_SSIZE(fsp_flags);

    /* If the logical page size is zero in fsp_flags, then use the
    legacy 16k page size. */
    ssize = (0 == ssize) ? UNIV_PAGE_SSIZE_ORIG : ssize;

    /* Convert from a 'log2 minus 9' to a page size in bytes. */
    const ulint size = ((UNIV_ZIP_SIZE_MIN >> 1) << ssize);

    ut_ad(size <= UNIV_PAGE_SIZE_MAX);
    ut_ad(size <= (1 << PAGE_SIZE_T_SIZE_BITS));

    m_logical = size;

    ssize = FSP_FLAGS_GET_ZIP_SSIZE(fsp_flags);

    /* If the fsp_flags have zero in the zip_ssize field, then
    it means that the tablespace does not have compressed pages
    and the physical page size is the same as the logical page
    size. */
    if (ssize == 0) {
      m_is_compressed = false;
      m_physical = m_logical;
    } else {
      m_is_compressed = true;

      /* Convert from a 'log2 minus 9' to a page size
      in bytes. */
      const ulint phy = ((UNIV_ZIP_SIZE_MIN >> 1) << ssize);

      ut_ad(phy <= UNIV_ZIP_SIZE_MAX);
      ut_ad(phy <= (1 << PAGE_SIZE_T_SIZE_BITS));

      m_physical = phy;
    }
  }

  /** Retrieve the physical page size (on-disk).
  @return physical page size in bytes */
  inline size_t physical() const {
    ut_ad(m_physical > 0);

    return (m_physical);
  }

  /** Retrieve the logical page size (in-memory).
  @return logical page size in bytes */
  inline size_t logical() const {
    ut_ad(m_logical > 0);
    return (m_logical);
  }

  page_no_t extent_size() const {
    page_no_t size = 0;
    switch (m_physical) {
      case 4096:
        size = 256;
        break;
      case 8192:
        size = 128;
        break;
      case 16384:
      case 32768:
      case 65536:
        size = 64;
        break;
      default:
        ut_d(ut_error);
    }
    return (size);
  }

  size_t extents_per_xdes() const { return (m_physical / extent_size()); }

  /** Check whether the page is compressed on disk.
  @return true if compressed */
  inline bool is_compressed() const { return (m_is_compressed); }

  /** Copy the values from a given page_size_t object.
  @param[in]    src     page size object whose values to fetch */
  inline void copy_from(const page_size_t &src) {
    m_physical = src.physical();
    m_logical = src.logical();
    m_is_compressed = src.is_compressed();
  }

  /** Check if a given page_size_t object is equal to the current one.
  @param[in]    a       page_size_t object to compare
  @return true if equal */
  inline bool equals_to(const page_size_t &a) const {
    return (a.physical() == m_physical && a.logical() == m_logical &&
            a.is_compressed() == (bool)m_is_compressed);
  }

  inline void set_flag(uint32_t fsp_flags) {
    uint32_t ssize = FSP_FLAGS_GET_PAGE_SSIZE(fsp_flags);

    /* If the logical page size is zero in fsp_flags, then
    use the legacy 16k page size. */
    ssize = (0 == ssize) ? UNIV_PAGE_SSIZE_ORIG : ssize;

    /* Convert from a 'log2 minus 9' to a page size in bytes. */
    const uint32_t size = ((UNIV_ZIP_SIZE_MIN >> 1) << ssize);

    ut_ad(size <= UNIV_PAGE_SIZE_MAX);
    ut_ad(size <= (1 << PAGE_SIZE_T_SIZE_BITS));

    m_logical = size;

    ssize = FSP_FLAGS_GET_ZIP_SSIZE(fsp_flags);

    /* If the fsp_flags have zero in the zip_ssize field,
    then it means that the tablespace does not have
    compressed pages and the physical page size is the same
    as the logical page size. */
    if (ssize == 0) {
      m_is_compressed = false;
      m_physical = m_logical;
    } else {
      m_is_compressed = true;

      /* Convert from a 'log2 minus 9' to a page size
      in bytes. */
      const ulint phy = ((UNIV_ZIP_SIZE_MIN >> 1) << ssize);

      ut_ad(phy <= UNIV_ZIP_SIZE_MAX);
      ut_ad(phy <= (1 << PAGE_SIZE_T_SIZE_BITS));

      m_physical = phy;
    }
  }

  page_size_t &operator=(const page_size_t &) = default;
  page_size_t(const page_size_t &) = default;

 private:
  /* For non compressed tablespaces, physical page size is equal to
  the logical page size and the data is stored in buf_page_t::frame
  (and is also always equal to univ_page_size (--innodb-page-size=)).

  For compressed tablespaces, physical page size is the compressed
  page size as stored on disk and in buf_page_t::zip::data. The logical
  page size is the uncompressed page size in memory - the size of
  buf_page_t::frame (currently also always equal to univ_page_size
  (--innodb-page-size=)). */

  /** Physical page size. */
  unsigned m_physical : PAGE_SIZE_T_SIZE_BITS;

  /** Logical page size. */
  unsigned m_logical : PAGE_SIZE_T_SIZE_BITS;

  /** Flag designating whether the physical page is compressed, which is
  true IFF the whole tablespace where the page belongs is compressed. */
  unsigned m_is_compressed : 1;
};

/* Overloading the global output operator to conveniently print an object
of type the page_size_t.
@param[in,out]  out     the output stream
@param[in]      obj     an object of type page_size_t to be printed
@retval the output stream */
inline std::ostream &operator<<(std::ostream &out, const page_size_t &obj) {
  out << "[page size: physical=" << obj.physical()
      << ", logical=" << obj.logical() << ", compressed=" << obj.is_compressed()
      << "]";
  return (out);
}

extern page_size_t univ_page_size;

#endif /* page0size_t */
