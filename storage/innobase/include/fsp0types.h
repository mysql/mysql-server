/*****************************************************************************

Copyright (c) 1995, 2016, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

*****************************************************************************/

/******************************************************
@file include/fsp0types.h
File space management types

Created May 26, 2009 Vasil Dimov
*******************************************************/

#ifndef fsp0types_h
#define fsp0types_h

#ifndef UNIV_INNOCHECKSUM

#include "univ.i"

/** @name Flags for inserting records in order
If records are inserted in order, there are the following
flags to tell this (their type is made byte for the compiler
to warn if direction and hint parameters are switched in
fseg_alloc_free_page) */
/* @{ */
#define	FSP_UP		((byte)111)	/*!< alphabetically upwards */
#define	FSP_DOWN	((byte)112)	/*!< alphabetically downwards */
#define	FSP_NO_DIR	((byte)113)	/*!< no order */
/* @} */

#endif /* !UNIV_INNOCHECKSUM */
/** File space extent size in pages
page size | file space extent size
----------+-----------------------
   4 KiB  | 256 pages = 1 MiB
   8 KiB  | 128 pages = 1 MiB
  16 KiB  |  64 pages = 1 MiB
  32 KiB  |  64 pages = 2 MiB
  64 KiB  |  64 pages = 4 MiB
*/
#define FSP_EXTENT_SIZE         ((UNIV_PAGE_SIZE <= (16384) ?	\
				(1048576 / UNIV_PAGE_SIZE) :	\
				((UNIV_PAGE_SIZE <= (32768)) ?	\
				(2097152 / UNIV_PAGE_SIZE) :	\
				(4194304 / UNIV_PAGE_SIZE))))

/** File space extent size (four megabyte) in pages for MAX page size */
#define	FSP_EXTENT_SIZE_MAX	(4194304 / UNIV_PAGE_SIZE_MAX)

/** File space extent size (one megabyte) in pages for MIN page size */
#define	FSP_EXTENT_SIZE_MIN	(1048576 / UNIV_PAGE_SIZE_MIN)

/** On a page of any file segment, data may be put starting from this
offset */
#define FSEG_PAGE_DATA		FIL_PAGE_DATA

#ifndef UNIV_INNOCHECKSUM
/** @name File segment header
The file segment header points to the inode describing the file segment. */
/* @{ */
/** Data type for file segment header */
typedef	byte	fseg_header_t;

#define FSEG_HDR_SPACE		0	/*!< space id of the inode */
#define FSEG_HDR_PAGE_NO	4	/*!< page number of the inode */
#define FSEG_HDR_OFFSET		8	/*!< byte offset of the inode */

#define FSEG_HEADER_SIZE	10	/*!< Length of the file system
					header, in bytes */
/* @} */

#ifdef UNIV_DEBUG

struct mtr_t;

/** A wrapper class to print the file segment header information. */
class fseg_header
{
public:
	/** Constructor of fseg_header.
	@param[in]	header	the underlying file segment header object
	@param[in]	mtr	the mini-transaction.  No redo logs are
				generated, only latches are checked within
				mini-transaction */
	fseg_header(
		const fseg_header_t*	header,
		mtr_t*			mtr)
		:
		m_header(header),
		m_mtr(mtr)
	{}

	/** Print the file segment header to the given output stream.
	@param[in,out]	out	the output stream into which the object
				is printed.
	@retval	the output stream into which the object was printed. */
	std::ostream&
	to_stream(std::ostream&	out) const;
private:
	/** The underlying file segment header */
	const fseg_header_t*	m_header;

	/** The mini transaction, which is used mainly to check whether
	appropriate latches have been taken by the calling thread. */
	mtr_t*			m_mtr;
};

/* Overloading the global output operator to print a file segment header
@param[in,out]	out	the output stream into which object will be printed
@param[in]	header	the file segment header to be printed
@retval the output stream */
inline
std::ostream&
operator<<(
	std::ostream&		out,
	const fseg_header&	header)
{
	return(header.to_stream(out));
}
#endif /* UNIV_DEBUG */

/** Flags for fsp_reserve_free_extents */
enum fsp_reserve_t {
	FSP_NORMAL,	/* reservation during normal B-tree operations */
	FSP_UNDO,	/* reservation done for undo logging */
	FSP_CLEANING,	/* reservation done during purge operations */
	FSP_BLOB	/* reservation being done for BLOB insertion */
};

/* Number of pages described in a single descriptor page: currently each page
description takes less than 1 byte; a descriptor page is repeated every
this many file pages */
/* #define XDES_DESCRIBED_PER_PAGE		UNIV_PAGE_SIZE */
/* This has been replaced with either UNIV_PAGE_SIZE or page_zip->size. */

/** @name The space low address page map
The pages at FSP_XDES_OFFSET and FSP_IBUF_BITMAP_OFFSET are repeated
every XDES_DESCRIBED_PER_PAGE pages in every tablespace. */
/* @{ */
/*--------------------------------------*/
#define FSP_XDES_OFFSET			0	/* !< extent descriptor */
#define FSP_IBUF_BITMAP_OFFSET		1	/* !< insert buffer bitmap */
				/* The ibuf bitmap pages are the ones whose
				page number is the number above plus a
				multiple of XDES_DESCRIBED_PER_PAGE */

#define FSP_FIRST_INODE_PAGE_NO		2	/*!< in every tablespace */
				/* The following pages exist
				in the system tablespace (space 0). */
#define FSP_IBUF_HEADER_PAGE_NO		3	/*!< insert buffer
						header page, in
						tablespace 0 */
#define FSP_IBUF_TREE_ROOT_PAGE_NO	4	/*!< insert buffer
						B-tree root page in
						tablespace 0 */
				/* The ibuf tree root page number in
				tablespace 0; its fseg inode is on the page
				number FSP_FIRST_INODE_PAGE_NO */
#define FSP_TRX_SYS_PAGE_NO		5	/*!< transaction
						system header, in
						tablespace 0 */
#define	FSP_FIRST_RSEG_PAGE_NO		6	/*!< first rollback segment
						page, in tablespace 0 */
#define FSP_DICT_HDR_PAGE_NO		7	/*!< data dictionary header
						page, in tablespace 0 */
/*--------------------------------------*/
/* @} */

/** Validate the tablespace flags.
These flags are stored in the tablespace header at offset FSP_SPACE_FLAGS.
They should be 0 for ROW_FORMAT=COMPACT and ROW_FORMAT=REDUNDANT.
The newer row formats, COMPRESSED and DYNAMIC, use a file format > Antelope
so they should have a file format number plus the DICT_TF_COMPACT bit set.
@param[in]	flags	Tablespace flags
@return true if valid, false if not */
bool
fsp_flags_is_valid(
	ulint	flags)
	MY_ATTRIBUTE((warn_unused_result, const));

/** Check if tablespace is system temporary.
@param[in]      space_id        verify is checksum is enabled for given space.
@return true if tablespace is system temporary. */
bool
fsp_is_system_temporary(
	ulint	space_id);

/** Check if checksum is disabled for the given space.
@param[in]	space_id	verify is checksum is enabled for given space.
@return true if checksum is disabled for given space. */
bool
fsp_is_checksum_disabled(
	ulint	space_id);

/** Check if tablespace is file-per-table.
@param[in]	space_id	Tablespace ID
@param[in]	fsp_flags	Tablespace Flags
@return true if tablespace is file-per-table. */
bool
fsp_is_file_per_table(
	ulint	space_id,
	ulint	fsp_flags);

#ifdef UNIV_DEBUG
/** Skip some of the sanity checks that are time consuming even in debug mode
and can affect frequent verification runs that are done to ensure stability of
the product.
@return true if check should be skipped for given space. */
bool
fsp_skip_sanity_check(
	ulint	space_id);
#endif /* UNIV_DEBUG */

#endif /* !UNIV_INNOCHECKSUM */

/* @defgroup fsp_flags InnoDB Tablespace Flag Constants @{ */

/** Width of the POST_ANTELOPE flag */
#define FSP_FLAGS_WIDTH_POST_ANTELOPE	1
/** Number of flag bits used to indicate the tablespace zip page size */
#define FSP_FLAGS_WIDTH_ZIP_SSIZE	4
/** Width of the ATOMIC_BLOBS flag.  The ability to break up a long
column into an in-record prefix and an externally stored part is available
to the two Barracuda row formats COMPRESSED and DYNAMIC. */
#define FSP_FLAGS_WIDTH_ATOMIC_BLOBS	1
/** Number of flag bits used to indicate the tablespace page size */
#define FSP_FLAGS_WIDTH_PAGE_SSIZE	4
/** Width of the DATA_DIR flag.  This flag indicates that the tablespace
is found in a remote location, not the default data directory. */
#define FSP_FLAGS_WIDTH_DATA_DIR	1
/** Width of the SHARED flag.  This flag indicates that the tablespace
was created with CREATE TABLESPACE and can be shared by multiple tables. */
#define FSP_FLAGS_WIDTH_SHARED		1
/** Width of the TEMPORARY flag.  This flag indicates that the tablespace
is a temporary tablespace and everything in it is temporary, meaning that
it is for a single client and should be deleted upon startup if it exists. */
#define FSP_FLAGS_WIDTH_TEMPORARY	1
/** Width of the encryption flag.  This flag indicates that the tablespace
is a tablespace with encryption. */
#define FSP_FLAGS_WIDTH_ENCRYPTION	1
/** Width of all the currently known tablespace flags */
#define FSP_FLAGS_WIDTH		(FSP_FLAGS_WIDTH_POST_ANTELOPE	\
				+ FSP_FLAGS_WIDTH_ZIP_SSIZE	\
				+ FSP_FLAGS_WIDTH_ATOMIC_BLOBS	\
				+ FSP_FLAGS_WIDTH_PAGE_SSIZE	\
				+ FSP_FLAGS_WIDTH_DATA_DIR	\
				+ FSP_FLAGS_WIDTH_SHARED	\
				+ FSP_FLAGS_WIDTH_TEMPORARY	\
				+ FSP_FLAGS_WIDTH_ENCRYPTION)

/** A mask of all the known/used bits in tablespace flags */
#define FSP_FLAGS_MASK		(~(~0 << FSP_FLAGS_WIDTH))

/** Zero relative shift position of the POST_ANTELOPE field */
#define FSP_FLAGS_POS_POST_ANTELOPE	0
/** Zero relative shift position of the ZIP_SSIZE field */
#define FSP_FLAGS_POS_ZIP_SSIZE		(FSP_FLAGS_POS_POST_ANTELOPE	\
					+ FSP_FLAGS_WIDTH_POST_ANTELOPE)
/** Zero relative shift position of the ATOMIC_BLOBS field */
#define FSP_FLAGS_POS_ATOMIC_BLOBS	(FSP_FLAGS_POS_ZIP_SSIZE	\
					+ FSP_FLAGS_WIDTH_ZIP_SSIZE)
/** Zero relative shift position of the PAGE_SSIZE field */
#define FSP_FLAGS_POS_PAGE_SSIZE	(FSP_FLAGS_POS_ATOMIC_BLOBS	\
					+ FSP_FLAGS_WIDTH_ATOMIC_BLOBS)
/** Zero relative shift position of the start of the DATA_DIR bit */
#define FSP_FLAGS_POS_DATA_DIR		(FSP_FLAGS_POS_PAGE_SSIZE	\
					+ FSP_FLAGS_WIDTH_PAGE_SSIZE)
/** Zero relative shift position of the start of the SHARED bit */
#define FSP_FLAGS_POS_SHARED		(FSP_FLAGS_POS_DATA_DIR		\
					+ FSP_FLAGS_WIDTH_DATA_DIR)
/** Zero relative shift position of the start of the TEMPORARY bit */
#define FSP_FLAGS_POS_TEMPORARY		(FSP_FLAGS_POS_SHARED		\
					+ FSP_FLAGS_WIDTH_SHARED)
/** Zero relative shift position of the start of the ENCRYPTION bit */
#define FSP_FLAGS_POS_ENCRYPTION	(FSP_FLAGS_POS_TEMPORARY	\
					+ FSP_FLAGS_WIDTH_TEMPORARY)
/** Zero relative shift position of the start of the UNUSED bits */
#define FSP_FLAGS_POS_UNUSED		(FSP_FLAGS_POS_ENCRYPTION	\
					+ FSP_FLAGS_WIDTH_ENCRYPTION)

/** Bit mask of the POST_ANTELOPE field */
#define FSP_FLAGS_MASK_POST_ANTELOPE				\
		((~(~0U << FSP_FLAGS_WIDTH_POST_ANTELOPE))	\
		<< FSP_FLAGS_POS_POST_ANTELOPE)
/** Bit mask of the ZIP_SSIZE field */
#define FSP_FLAGS_MASK_ZIP_SSIZE				\
		((~(~0U << FSP_FLAGS_WIDTH_ZIP_SSIZE))		\
		<< FSP_FLAGS_POS_ZIP_SSIZE)
/** Bit mask of the ATOMIC_BLOBS field */
#define FSP_FLAGS_MASK_ATOMIC_BLOBS				\
		((~(~0U << FSP_FLAGS_WIDTH_ATOMIC_BLOBS))	\
		<< FSP_FLAGS_POS_ATOMIC_BLOBS)
/** Bit mask of the PAGE_SSIZE field */
#define FSP_FLAGS_MASK_PAGE_SSIZE				\
		((~(~0U << FSP_FLAGS_WIDTH_PAGE_SSIZE))		\
		<< FSP_FLAGS_POS_PAGE_SSIZE)
/** Bit mask of the DATA_DIR field */
#define FSP_FLAGS_MASK_DATA_DIR					\
		((~(~0U << FSP_FLAGS_WIDTH_DATA_DIR))		\
		<< FSP_FLAGS_POS_DATA_DIR)
/** Bit mask of the SHARED field */
#define FSP_FLAGS_MASK_SHARED					\
		((~(~0U << FSP_FLAGS_WIDTH_SHARED))		\
		<< FSP_FLAGS_POS_SHARED)
/** Bit mask of the TEMPORARY field */
#define FSP_FLAGS_MASK_TEMPORARY				\
		((~(~0U << FSP_FLAGS_WIDTH_TEMPORARY))		\
		<< FSP_FLAGS_POS_TEMPORARY)
/** Bit mask of the ENCRYPTION field */
#define FSP_FLAGS_MASK_ENCRYPTION				\
		((~(~0U << FSP_FLAGS_WIDTH_ENCRYPTION))		\
		<< FSP_FLAGS_POS_ENCRYPTION)

/** Return the value of the POST_ANTELOPE field */
#define FSP_FLAGS_GET_POST_ANTELOPE(flags)			\
		((flags & FSP_FLAGS_MASK_POST_ANTELOPE)		\
		>> FSP_FLAGS_POS_POST_ANTELOPE)
/** Return the value of the ZIP_SSIZE field */
#define FSP_FLAGS_GET_ZIP_SSIZE(flags)				\
		((flags & FSP_FLAGS_MASK_ZIP_SSIZE)		\
		>> FSP_FLAGS_POS_ZIP_SSIZE)
/** Return the value of the ATOMIC_BLOBS field */
#define FSP_FLAGS_HAS_ATOMIC_BLOBS(flags)			\
		((flags & FSP_FLAGS_MASK_ATOMIC_BLOBS)		\
		>> FSP_FLAGS_POS_ATOMIC_BLOBS)
/** Return the value of the PAGE_SSIZE field */
#define FSP_FLAGS_GET_PAGE_SSIZE(flags)				\
		((flags & FSP_FLAGS_MASK_PAGE_SSIZE)		\
		>> FSP_FLAGS_POS_PAGE_SSIZE)
/** Return the value of the DATA_DIR field */
#define FSP_FLAGS_HAS_DATA_DIR(flags)				\
		((flags & FSP_FLAGS_MASK_DATA_DIR)		\
		>> FSP_FLAGS_POS_DATA_DIR)
/** Return the contents of the SHARED field */
#define FSP_FLAGS_GET_SHARED(flags)				\
		((flags & FSP_FLAGS_MASK_SHARED)		\
		>> FSP_FLAGS_POS_SHARED)
/** Return the contents of the TEMPORARY field */
#define FSP_FLAGS_GET_TEMPORARY(flags)				\
		((flags & FSP_FLAGS_MASK_TEMPORARY)		\
		>> FSP_FLAGS_POS_TEMPORARY)
/** Return the contents of the ENCRYPTION field */
#define FSP_FLAGS_GET_ENCRYPTION(flags)				\
		((flags & FSP_FLAGS_MASK_ENCRYPTION)		\
		>> FSP_FLAGS_POS_ENCRYPTION)
/** Return the contents of the UNUSED bits */
#define FSP_FLAGS_GET_UNUSED(flags)				\
		(flags >> FSP_FLAGS_POS_UNUSED)

/** Use an alias in the code for FSP_FLAGS_GET_SHARED() */
#define fsp_is_shared_tablespace FSP_FLAGS_GET_SHARED
/* @} */

#endif /* fsp0types_h */
