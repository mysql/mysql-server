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

/******************************************************
@file include/fsp0types.h
File space management types

Created May 26, 2009 Vasil Dimov
*******************************************************/

#ifndef fsp0types_h
#define fsp0types_h

#include "fil0types.h"
#include "univ.i"

/** @name Flags for inserting records in order
If records are inserted in order, there are the following
flags to tell this (their type is made byte for the compiler
to warn if direction and hint parameters are switched in
fseg_alloc_free_page) */
/** @{ */
/** alphabetically upwards */
constexpr byte FSP_UP = 111;
/** alphabetically downwards */
constexpr byte FSP_DOWN = 112;
/** no order */
constexpr byte FSP_NO_DIR = 113;
/** @} */

/** File space extent size in pages
page size | file space extent size
----------+-----------------------
   4 KiB  | 256 pages = 1 MiB
   8 KiB  | 128 pages = 1 MiB
  16 KiB  |  64 pages = 1 MiB
  32 KiB  |  64 pages = 2 MiB
  64 KiB  |  64 pages = 4 MiB
*/
#define FSP_EXTENT_SIZE                                                 \
  static_cast<page_no_t>(                                               \
      ((UNIV_PAGE_SIZE <= (16384)                                       \
            ? (1048576 / UNIV_PAGE_SIZE)                                \
            : ((UNIV_PAGE_SIZE <= (32768)) ? (2097152 / UNIV_PAGE_SIZE) \
                                           : (4194304 / UNIV_PAGE_SIZE)))))

/** File space extent size (four megabyte) in pages for MAX page size */
constexpr size_t FSP_EXTENT_SIZE_MAX = 4194304 / UNIV_PAGE_SIZE_MAX;

/** File space extent size (one megabyte) in pages for MIN page size */
constexpr size_t FSP_EXTENT_SIZE_MIN = 1048576 / UNIV_PAGE_SIZE_MIN;

/** On a page of any file segment, data
may be put starting from this offset */
constexpr uint32_t FSEG_PAGE_DATA = FIL_PAGE_DATA;

/** @name File segment header
The file segment header points to the inode describing the file segment. */
/** @{ */
/** Data type for file segment header */
typedef byte fseg_header_t;

/** space id of the inode */
constexpr uint32_t FSEG_HDR_SPACE = 0;
/** page number of the inode */
constexpr uint32_t FSEG_HDR_PAGE_NO = 4;
/** byte offset of the inode */
constexpr uint32_t FSEG_HDR_OFFSET = 8;
/** Length of the file system  header, in bytes */
constexpr uint32_t FSEG_HEADER_SIZE = 10;
/** @} */

#ifdef UNIV_DEBUG

struct mtr_t;

/** A wrapper class to print the file segment header information. */
class fseg_header {
 public:
  /** Constructor of fseg_header.
  @param[in]    header  Underlying file segment header object
  @param[in]    mtr     Mini-transaction.  No redo logs are
                          generated, only latches are checked within
                          mini-transaction */
  fseg_header(const fseg_header_t *header, mtr_t *mtr)
      : m_header(header), m_mtr(mtr) {}

  /** Print the file segment header to the given output stream.
  @param[in]    out     the output stream into which the object is printed.
  @retval       the output stream into which the object was printed. */
  std::ostream &to_stream(std::ostream &out) const;

 private:
  /** The underlying file segment header */
  const fseg_header_t *m_header;

  /** The mini-transaction, which is used mainly to check whether
  appropriate latches have been taken by the calling thread. */
  mtr_t *m_mtr;
};

/* Overloading the global output operator to print a file segment header
@param[in,out]  out     the output stream into which object will be printed
@param[in]      header  the file segment header to be printed
@retval the output stream */
inline std::ostream &operator<<(std::ostream &out, const fseg_header &header) {
  return (header.to_stream(out));
}
#endif /* UNIV_DEBUG */

/** Flags for fsp_reserve_free_extents */
enum fsp_reserve_t {
  FSP_NORMAL,   /* reservation during normal B-tree operations */
  FSP_UNDO,     /* reservation done for undo logging */
  FSP_CLEANING, /* reservation done during purge operations */
  FSP_BLOB      /* reservation being done for BLOB insertion */
};

/* Number of pages described in a single descriptor page: currently each page
description takes less than 1 byte; a descriptor page is repeated every
this many file pages */
/* #define XDES_DESCRIBED_PER_PAGE              UNIV_PAGE_SIZE */
/* This has been replaced with either UNIV_PAGE_SIZE or page_zip->size. */

/** @name The space low address page map
The pages at FSP_XDES_OFFSET and FSP_IBUF_BITMAP_OFFSET are repeated
every XDES_DESCRIBED_PER_PAGE pages in every tablespace. */
/** @{ */
/*--------------------------------------*/
/** extent descriptor */
constexpr uint32_t FSP_XDES_OFFSET = 0;
/** insert buffer bitmap; The ibuf bitmap pages are the ones whose page number
is the number above plus a multiple of XDES_DESCRIBED_PER_PAGE */
constexpr uint32_t FSP_IBUF_BITMAP_OFFSET = 1;
/** in every tablespace */
constexpr uint32_t FSP_FIRST_INODE_PAGE_NO = 2;

/** The following pages exist in the system tablespace (space 0). */
/** insert buffer header page, in tablespace 0 */
constexpr uint32_t FSP_IBUF_HEADER_PAGE_NO = 3;
/** insert buffer B-tree root page in tablespace 0;
The ibuf tree root page number in tablespace 0; its fseg inode is on the page
number FSP_FIRST_INODE_PAGE_NO */
constexpr uint32_t FSP_IBUF_TREE_ROOT_PAGE_NO = 4;
/** transaction system header, in tablespace 0 */
constexpr uint32_t FSP_TRX_SYS_PAGE_NO = 5;
/** first rollback segment page, in tablespace 0 */
constexpr uint32_t FSP_FIRST_RSEG_PAGE_NO = 6;
/** data dictionary header page, in tablespace 0 */
constexpr uint32_t FSP_DICT_HDR_PAGE_NO = 7;

/* The following page exists in each v8 Undo Tablespace.
(space_id = SRV_LOG_SPACE_FIRST_ID - undo_space_num)
(undo_space_num = rseg_array_slot_num + 1) */

/** rollback segment directory page number in each undo tablespace */
constexpr uint32_t FSP_RSEG_ARRAY_PAGE_NO = 3;
/*--------------------------------------*/
/** @} */

/** Validate the tablespace flags.
These flags are stored in the tablespace header at offset FSP_SPACE_FLAGS.
They should be 0 for ROW_FORMAT=COMPACT and ROW_FORMAT=REDUNDANT.
The newer row formats, COMPRESSED and DYNAMIC, will have at least
the DICT_TF_COMPACT bit set.
@param[in]      flags   Tablespace flags
@return true if valid, false if not */
[[nodiscard]] bool fsp_flags_is_valid(uint32_t flags);

/** Check if tablespace is system temporary.
@param[in]      space_id        tablespace ID
@return true if tablespace is system temporary. */
bool fsp_is_system_temporary(space_id_t space_id);

/** Check if the tablespace is session temporary.
@param[in]      space_id        tablespace ID
@return true if tablespace is a session temporary tablespace. */
bool fsp_is_session_temporary(space_id_t space_id);

/** Check if tablespace is global temporary.
@param[in]      space_id        tablespace ID
@return true if tablespace is global temporary. */
bool fsp_is_global_temporary(space_id_t space_id);

/** Check if checksum is disabled for the given space.
@param[in]      space_id        tablespace ID
@return true if checksum is disabled for given space. */
bool fsp_is_checksum_disabled(space_id_t space_id);

#ifdef UNIV_DEBUG
/** Skip some of the sanity checks that are time consuming even in debug mode
and can affect frequent verification runs that are done to ensure stability of
the product.
@return true if check should be skipped for given space. */
bool fsp_skip_sanity_check(space_id_t space_id);
#endif /* UNIV_DEBUG */

/** @defgroup fsp_flags InnoDB Tablespace Flag Constants
@{ */

/** Width of the POST_ANTELOPE flag */
constexpr uint32_t FSP_FLAGS_WIDTH_POST_ANTELOPE = 1;
/** Number of flag bits used to indicate the tablespace zip page size */
constexpr uint32_t FSP_FLAGS_WIDTH_ZIP_SSIZE = 4;
/** Width of the ATOMIC_BLOBS flag.  The ability to break up a long
column into an in-record prefix and an externally stored part is available
to ROW_FORMAT=REDUNDANT and ROW_FORMAT=COMPACT. */
constexpr uint32_t FSP_FLAGS_WIDTH_ATOMIC_BLOBS = 1;
/** Number of flag bits used to indicate the tablespace page size */
constexpr uint32_t FSP_FLAGS_WIDTH_PAGE_SSIZE = 4;
/** Width of the DATA_DIR flag.  This flag indicates that the tablespace
is found in a remote location, not the default data directory. */
constexpr uint32_t FSP_FLAGS_WIDTH_DATA_DIR = 1;
/** Width of the SHARED flag.  This flag indicates that the tablespace
was created with CREATE TABLESPACE and can be shared by multiple tables. */
constexpr uint32_t FSP_FLAGS_WIDTH_SHARED = 1;
/** Width of the TEMPORARY flag.  This flag indicates that the tablespace
is a temporary tablespace and everything in it is temporary, meaning that
it is for a single client and should be deleted upon startup if it exists. */
constexpr uint32_t FSP_FLAGS_WIDTH_TEMPORARY = 1;
/** Width of the encryption flag.  This flag indicates that the tablespace
is a tablespace with encryption. */
constexpr uint32_t FSP_FLAGS_WIDTH_ENCRYPTION = 1;
/** Width of the SDI flag.  This flag indicates the presence of
tablespace dictionary.*/
constexpr uint32_t FSP_FLAGS_WIDTH_SDI = 1;

/** Width of all the currently known tablespace flags */
constexpr uint32_t FSP_FLAGS_WIDTH =
    FSP_FLAGS_WIDTH_POST_ANTELOPE + FSP_FLAGS_WIDTH_ZIP_SSIZE +
    FSP_FLAGS_WIDTH_ATOMIC_BLOBS + FSP_FLAGS_WIDTH_PAGE_SSIZE +
    FSP_FLAGS_WIDTH_DATA_DIR + FSP_FLAGS_WIDTH_SHARED +
    FSP_FLAGS_WIDTH_TEMPORARY + FSP_FLAGS_WIDTH_ENCRYPTION +
    FSP_FLAGS_WIDTH_SDI;

/** A mask of all the known/used bits in tablespace flags */
constexpr uint32_t FSP_FLAGS_MASK = ~(~0U << FSP_FLAGS_WIDTH);

/** Zero relative shift position of the POST_ANTELOPE field */
constexpr uint32_t FSP_FLAGS_POS_POST_ANTELOPE = 0;
/** Zero relative shift position of the ZIP_SSIZE field */
constexpr uint32_t FSP_FLAGS_POS_ZIP_SSIZE =
    FSP_FLAGS_POS_POST_ANTELOPE + FSP_FLAGS_WIDTH_POST_ANTELOPE;
/** Zero relative shift position of the ATOMIC_BLOBS field */
constexpr uint32_t FSP_FLAGS_POS_ATOMIC_BLOBS =
    FSP_FLAGS_POS_ZIP_SSIZE + FSP_FLAGS_WIDTH_ZIP_SSIZE;
/** Zero relative shift position of the PAGE_SSIZE field */
constexpr uint32_t FSP_FLAGS_POS_PAGE_SSIZE =
    FSP_FLAGS_POS_ATOMIC_BLOBS + FSP_FLAGS_WIDTH_ATOMIC_BLOBS;
/** Zero relative shift position of the start of the DATA_DIR bit */
constexpr uint32_t FSP_FLAGS_POS_DATA_DIR =
    FSP_FLAGS_POS_PAGE_SSIZE + FSP_FLAGS_WIDTH_PAGE_SSIZE;
/** Zero relative shift position of the start of the SHARED bit */
constexpr uint32_t FSP_FLAGS_POS_SHARED =
    FSP_FLAGS_POS_DATA_DIR + FSP_FLAGS_WIDTH_DATA_DIR;
/** Zero relative shift position of the start of the TEMPORARY bit */
constexpr uint32_t FSP_FLAGS_POS_TEMPORARY =
    FSP_FLAGS_POS_SHARED + FSP_FLAGS_WIDTH_SHARED;
/** Zero relative shift position of the start of the ENCRYPTION bit */
constexpr uint32_t FSP_FLAGS_POS_ENCRYPTION =
    FSP_FLAGS_POS_TEMPORARY + FSP_FLAGS_WIDTH_TEMPORARY;
/** Zero relative shift position of the start of the SDI bits */
constexpr uint32_t FSP_FLAGS_POS_SDI =
    FSP_FLAGS_POS_ENCRYPTION + FSP_FLAGS_WIDTH_ENCRYPTION;

/** Zero relative shift position of the start of the UNUSED bits */
constexpr uint32_t FSP_FLAGS_POS_UNUSED =
    FSP_FLAGS_POS_SDI + FSP_FLAGS_WIDTH_SDI;

/** Bit mask of the POST_ANTELOPE field */
constexpr uint32_t FSP_FLAGS_MASK_POST_ANTELOPE =
    (~(~0U << FSP_FLAGS_WIDTH_POST_ANTELOPE)) << FSP_FLAGS_POS_POST_ANTELOPE;
/** Bit mask of the ZIP_SSIZE field */
constexpr uint32_t FSP_FLAGS_MASK_ZIP_SSIZE =
    (~(~0U << FSP_FLAGS_WIDTH_ZIP_SSIZE)) << FSP_FLAGS_POS_ZIP_SSIZE;
/** Bit mask of the ATOMIC_BLOBS field */
constexpr uint32_t FSP_FLAGS_MASK_ATOMIC_BLOBS =
    (~(~0U << FSP_FLAGS_WIDTH_ATOMIC_BLOBS)) << FSP_FLAGS_POS_ATOMIC_BLOBS;
/** Bit mask of the PAGE_SSIZE field */
constexpr uint32_t FSP_FLAGS_MASK_PAGE_SSIZE =
    (~(~0U << FSP_FLAGS_WIDTH_PAGE_SSIZE)) << FSP_FLAGS_POS_PAGE_SSIZE;
/** Bit mask of the DATA_DIR field */
constexpr uint32_t FSP_FLAGS_MASK_DATA_DIR =
    (~(~0U << FSP_FLAGS_WIDTH_DATA_DIR)) << FSP_FLAGS_POS_DATA_DIR;
/** Bit mask of the SHARED field */
constexpr uint32_t FSP_FLAGS_MASK_SHARED = (~(~0U << FSP_FLAGS_WIDTH_SHARED))
                                           << FSP_FLAGS_POS_SHARED;
/** Bit mask of the TEMPORARY field */
constexpr uint32_t FSP_FLAGS_MASK_TEMPORARY =
    (~(~0U << FSP_FLAGS_WIDTH_TEMPORARY)) << FSP_FLAGS_POS_TEMPORARY;
/** Bit mask of the ENCRYPTION field */
constexpr uint32_t FSP_FLAGS_MASK_ENCRYPTION =
    (~(~0U << FSP_FLAGS_WIDTH_ENCRYPTION)) << FSP_FLAGS_POS_ENCRYPTION;
/** Bit mask of the SDI field */
constexpr uint32_t FSP_FLAGS_MASK_SDI = (~(~0U << FSP_FLAGS_WIDTH_SDI))
                                        << FSP_FLAGS_POS_SDI;

/** Return the value of the POST_ANTELOPE field */
constexpr uint32_t FSP_FLAGS_GET_POST_ANTELOPE(uint32_t flags) {
  return (flags & FSP_FLAGS_MASK_POST_ANTELOPE) >> FSP_FLAGS_POS_POST_ANTELOPE;
}
/** Return the value of the ZIP_SSIZE field */
constexpr uint32_t FSP_FLAGS_GET_ZIP_SSIZE(uint32_t flags) {
  return (flags & FSP_FLAGS_MASK_ZIP_SSIZE) >> FSP_FLAGS_POS_ZIP_SSIZE;
}
/** Return the value of the ATOMIC_BLOBS field */
constexpr uint32_t FSP_FLAGS_HAS_ATOMIC_BLOBS(uint32_t flags) {
  return (flags & FSP_FLAGS_MASK_ATOMIC_BLOBS) >> FSP_FLAGS_POS_ATOMIC_BLOBS;
}
/** Return the value of the PAGE_SSIZE field */
constexpr uint32_t FSP_FLAGS_GET_PAGE_SSIZE(uint32_t flags) {
  return (flags & FSP_FLAGS_MASK_PAGE_SSIZE) >> FSP_FLAGS_POS_PAGE_SSIZE;
}
/** Return the value of the DATA_DIR field */
constexpr uint32_t FSP_FLAGS_HAS_DATA_DIR(uint32_t flags) {
  return (flags & FSP_FLAGS_MASK_DATA_DIR) >> FSP_FLAGS_POS_DATA_DIR;
}
/** Return the contents of the SHARED field */
constexpr uint32_t FSP_FLAGS_GET_SHARED(uint32_t flags) {
  return (flags & FSP_FLAGS_MASK_SHARED) >> FSP_FLAGS_POS_SHARED;
}
/** Return the contents of the TEMPORARY field */
constexpr uint32_t FSP_FLAGS_GET_TEMPORARY(uint32_t flags) {
  return (flags & FSP_FLAGS_MASK_TEMPORARY) >> FSP_FLAGS_POS_TEMPORARY;
}
/** Return the contents of the ENCRYPTION field */
constexpr uint32_t FSP_FLAGS_GET_ENCRYPTION(uint32_t flags) {
  return (flags & FSP_FLAGS_MASK_ENCRYPTION) >> FSP_FLAGS_POS_ENCRYPTION;
}
/** Return the value of the SDI field */
constexpr uint32_t FSP_FLAGS_HAS_SDI(uint32_t flags) {
  return (flags & FSP_FLAGS_MASK_SDI) >> FSP_FLAGS_POS_SDI;
}
/** Return the contents of the UNUSED bits */
constexpr uint32_t FSP_FLAGS_GET_UNUSED(uint32_t flags) {
  return flags >> FSP_FLAGS_POS_UNUSED;
}
/** Return true if flags are not set */
constexpr bool FSP_FLAGS_ARE_NOT_SET(uint32_t flags) {
  return (flags & FSP_FLAGS_MASK) == 0;
}

/** Set ENCRYPTION bit in tablespace flags */
constexpr void fsp_flags_set_encryption(uint32_t &flags) {
  flags |= FSP_FLAGS_MASK_ENCRYPTION;
}

/** Set ENCRYPTION bit in tablespace flags */
constexpr void fsp_flags_unset_encryption(uint32_t &flags) {
  flags &= ~FSP_FLAGS_MASK_ENCRYPTION;
}

/** Set SDI Index bit in tablespace flags */
constexpr void fsp_flags_set_sdi(uint32_t &flags) {
  flags |= FSP_FLAGS_MASK_SDI;
}

/** Set SDI Index bit in tablespace flags */
constexpr void fsp_flags_unset_sdi(uint32_t &flags) {
  flags &= ~FSP_FLAGS_MASK_SDI;
}

/** Use an alias in the code for FSP_FLAGS_GET_SHARED() */
constexpr uint32_t fsp_is_shared_tablespace(uint32_t flags) {
  return FSP_FLAGS_GET_SHARED(flags);
}
/** @} */

/** Max number of rollback segments: the number of segment specification slots
in the transaction system array; rollback segment id must fit in one (signed)
byte, therefore 128; each slot is currently 8 bytes in size. If you want
to raise the level to 256 then you will need to fix some assertions that
impose the 7 bit restriction. e.g., mach_write_to_3() */
constexpr size_t TRX_SYS_N_RSEGS = 128;

/** Minimum and Maximum number of implicit undo tablespaces.  This kind
of undo tablespace is always created and found in --innodb-undo-directory. */
constexpr size_t FSP_MIN_UNDO_TABLESPACES = 2;
constexpr size_t FSP_MAX_UNDO_TABLESPACES = TRX_SYS_N_RSEGS - 1;
constexpr size_t FSP_IMPLICIT_UNDO_TABLESPACES = 2;
constexpr size_t FSP_MAX_ROLLBACK_SEGMENTS = TRX_SYS_N_RSEGS;

#endif /* fsp0types_h */
