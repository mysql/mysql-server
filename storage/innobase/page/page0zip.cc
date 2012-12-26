/*****************************************************************************

Copyright (c) 2005, 2012, Oracle and/or its affiliates. All Rights Reserved.
Copyright (c) 2012, Facebook Inc.

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

/**************************************************//**
@file page/page0zip.cc
Compressed page interface

Created June 2005 by Marko Makela
*******************************************************/

#include <map>
using namespace std;

#define THIS_MODULE
#include "page0zip.h"
#ifdef UNIV_NONINL
# include "page0zip.ic"
#endif
#undef THIS_MODULE
#include "page0page.h"
#include "mtr0log.h"
#include "ut0sort.h"
#include "dict0dict.h"
#include "btr0cur.h"
#include "page0types.h"
#include "log0recv.h"
#include "zlib.h"
#ifndef UNIV_HOTBACKUP
# include "buf0buf.h"
# include "buf0lru.h"
# include "btr0sea.h"
# include "dict0boot.h"
# include "lock0lock.h"
# include "srv0mon.h"
# include "srv0srv.h"
# include "ut0crc32.h"
#else /* !UNIV_HOTBACKUP */
# include "buf0checksum.h"
# define lock_move_reorganize_page(block, temp_block)	((void) 0)
# define buf_LRU_stat_inc_unzip()			((void) 0)
#endif /* !UNIV_HOTBACKUP */

#ifndef UNIV_HOTBACKUP
/** Statistics on compression, indexed by page_zip_des_t::ssize - 1 */
UNIV_INTERN page_zip_stat_t		page_zip_stat[PAGE_ZIP_SSIZE_MAX];
/** Statistics on compression, indexed by index->id */
UNIV_INTERN page_zip_stat_per_index_t	page_zip_stat_per_index;
/** Mutex protecting page_zip_stat_per_index */
UNIV_INTERN ib_mutex_t			page_zip_stat_per_index_mutex;
#ifdef HAVE_PSI_INTERFACE
UNIV_INTERN mysql_pfs_key_t		page_zip_stat_per_index_mutex_key;
#endif /* HAVE_PSI_INTERFACE */
#endif /* !UNIV_HOTBACKUP */

/* Compression level to be used by zlib. Settable by user. */
UNIV_INTERN ulint	page_compression_level = 6;

/* Whether or not to log compressed page images to avoid possible
compression algorithm changes in zlib. */
UNIV_INTERN bool	page_log_compressed_pages = true;

/* Please refer to ../include/page0zip.ic for a description of the
compressed page format. */

/* The infimum and supremum records are omitted from the compressed page.
On compress, we compare that the records are there, and on uncompress we
restore the records. */
/** Extra bytes of an infimum record */
static const byte infimum_extra[] = {
	0x01,			/* info_bits=0, n_owned=1 */
	0x00, 0x02		/* heap_no=0, status=2 */
	/* ?, ?	*/		/* next=(first user rec, or supremum) */
};
/** Data bytes of an infimum record */
static const byte infimum_data[] = {
	0x69, 0x6e, 0x66, 0x69,
	0x6d, 0x75, 0x6d, 0x00	/* "infimum\0" */
};
/** Extra bytes and data bytes of a supremum record */
static const byte supremum_extra_data[] = {
	/* 0x0?, */		/* info_bits=0, n_owned=1..8 */
	0x00, 0x0b,		/* heap_no=1, status=3 */
	0x00, 0x00,		/* next=0 */
	0x73, 0x75, 0x70, 0x72,
	0x65, 0x6d, 0x75, 0x6d	/* "supremum" */
};

/** Assert that a block of memory is filled with zero bytes.
Compare at most sizeof(field_ref_zero) bytes.
@param b	in: memory block
@param s	in: size of the memory block, in bytes */
#define ASSERT_ZERO(b, s) \
	ut_ad(!memcmp(b, field_ref_zero, ut_min(s, sizeof field_ref_zero)))
/** Assert that a BLOB pointer is filled with zero bytes.
@param b	in: BLOB pointer */
#define ASSERT_ZERO_BLOB(b) \
	ut_ad(!memcmp(b, field_ref_zero, sizeof field_ref_zero))

/* Enable some extra debugging output.  This code can be enabled
independently of any UNIV_ debugging conditions. */
#if defined UNIV_DEBUG || defined UNIV_ZIP_DEBUG
# include <stdarg.h>
__attribute__((format (printf, 1, 2)))
/**********************************************************************//**
Report a failure to decompress or compress.
@return	number of characters printed */
static
int
page_zip_fail_func(
/*===============*/
	const char*	fmt,	/*!< in: printf(3) format string */
	...)			/*!< in: arguments corresponding to fmt */
{
	int	res;
	va_list	ap;

	ut_print_timestamp(stderr);
	fputs("  InnoDB: ", stderr);
	va_start(ap, fmt);
	res = vfprintf(stderr, fmt, ap);
	va_end(ap);

	return(res);
}
/** Wrapper for page_zip_fail_func()
@param fmt_args	in: printf(3) format string and arguments */
# define page_zip_fail(fmt_args) page_zip_fail_func fmt_args
#else /* UNIV_DEBUG || UNIV_ZIP_DEBUG */
/** Dummy wrapper for page_zip_fail_func()
@param fmt_args	ignored: printf(3) format string and arguments */
# define page_zip_fail(fmt_args) /* empty */
#endif /* UNIV_DEBUG || UNIV_ZIP_DEBUG */

#ifndef UNIV_HOTBACKUP
/**********************************************************************//**
Determine the guaranteed free space on an empty page.
@return	minimum payload size on the page */
UNIV_INTERN
ulint
page_zip_empty_size(
/*================*/
	ulint	n_fields,	/*!< in: number of columns in the index */
	ulint	zip_size)	/*!< in: compressed page size in bytes */
{
	lint	size = zip_size
		/* subtract the page header and the longest
		uncompressed data needed for one record */
		- (PAGE_DATA
		   + PAGE_ZIP_DIR_SLOT_SIZE
		   + DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN
		   + 1/* encoded heap_no==2 in page_zip_write_rec() */
		   + 1/* end of modification log */
		   - REC_N_NEW_EXTRA_BYTES/* omitted bytes */)
		/* subtract the space for page_zip_fields_encode() */
		- compressBound(2 * (n_fields + 1));
	return(size > 0 ? (ulint) size : 0);
}
#endif /* !UNIV_HOTBACKUP */

/*************************************************************//**
Gets the number of elements in the dense page directory,
including deleted records (the free list).
@return	number of elements in the dense page directory */
UNIV_INLINE
ulint
page_zip_dir_elems(
/*===============*/
	const page_zip_des_t*	page_zip)	/*!< in: compressed page */
{
	/* Exclude the page infimum and supremum from the record count. */
	return(page_dir_get_n_heap(page_zip->data) - PAGE_HEAP_NO_USER_LOW);
}

/*************************************************************//**
Gets the size of the compressed page trailer (the dense page directory),
including deleted records (the free list).
@return	length of dense page directory, in bytes */
UNIV_INLINE
ulint
page_zip_dir_size(
/*==============*/
	const page_zip_des_t*	page_zip)	/*!< in: compressed page */
{
	return(PAGE_ZIP_DIR_SLOT_SIZE * page_zip_dir_elems(page_zip));
}

/*************************************************************//**
Gets an offset to the compressed page trailer (the dense page directory),
including deleted records (the free list).
@return	offset of the dense page directory */
UNIV_INLINE
ulint
page_zip_dir_start_offs(
/*====================*/
	const page_zip_des_t*	page_zip,	/*!< in: compressed page */
	ulint			n_dense)	/*!< in: directory size */
{
	ut_ad(n_dense * PAGE_ZIP_DIR_SLOT_SIZE < page_zip_get_size(page_zip));

	return(page_zip_get_size(page_zip) - n_dense * PAGE_ZIP_DIR_SLOT_SIZE);
}

/*************************************************************//**
Gets a pointer to the compressed page trailer (the dense page directory),
including deleted records (the free list).
@param[in] page_zip	compressed page
@param[in] n_dense	number of entries in the directory
@return	pointer to the dense page directory */
#define page_zip_dir_start_low(page_zip, n_dense)			\
	((page_zip)->data + page_zip_dir_start_offs(page_zip, n_dense))
/*************************************************************//**
Gets a pointer to the compressed page trailer (the dense page directory),
including deleted records (the free list).
@param[in] page_zip	compressed page
@return	pointer to the dense page directory */
#define page_zip_dir_start(page_zip)					\
	page_zip_dir_start_low(page_zip, page_zip_dir_elems(page_zip))

/*************************************************************//**
Gets the size of the compressed page trailer (the dense page directory),
only including user records (excluding the free list).
@return	length of dense page directory comprising existing records, in bytes */
UNIV_INLINE
ulint
page_zip_dir_user_size(
/*===================*/
	const page_zip_des_t*	page_zip)	/*!< in: compressed page */
{
	ulint	size = PAGE_ZIP_DIR_SLOT_SIZE
		* page_get_n_recs(page_zip->data);
	ut_ad(size <= page_zip_dir_size(page_zip));
	return(size);
}

/*************************************************************//**
Find the slot of the given record in the dense page directory.
@return	dense directory slot, or NULL if record not found */
UNIV_INLINE
byte*
page_zip_dir_find_low(
/*==================*/
	byte*	slot,			/*!< in: start of records */
	byte*	end,			/*!< in: end of records */
	ulint	offset)			/*!< in: offset of user record */
{
	ut_ad(slot <= end);

	for (; slot < end; slot += PAGE_ZIP_DIR_SLOT_SIZE) {
		if ((mach_read_from_2(slot) & PAGE_ZIP_DIR_SLOT_MASK)
		    == offset) {
			return(slot);
		}
	}

	return(NULL);
}

/*************************************************************//**
Find the slot of the given non-free record in the dense page directory.
@return	dense directory slot, or NULL if record not found */
UNIV_INLINE
byte*
page_zip_dir_find(
/*==============*/
	page_zip_des_t*	page_zip,		/*!< in: compressed page */
	ulint		offset)			/*!< in: offset of user record */
{
	byte*	end	= page_zip->data + page_zip_get_size(page_zip);

	ut_ad(page_zip_simple_validate(page_zip));

	return(page_zip_dir_find_low(end - page_zip_dir_user_size(page_zip),
				     end,
				     offset));
}

/*************************************************************//**
Find the slot of the given free record in the dense page directory.
@return	dense directory slot, or NULL if record not found */
UNIV_INLINE
byte*
page_zip_dir_find_free(
/*===================*/
	page_zip_des_t*	page_zip,		/*!< in: compressed page */
	ulint		offset)			/*!< in: offset of user record */
{
	byte*	end	= page_zip->data + page_zip_get_size(page_zip);

	ut_ad(page_zip_simple_validate(page_zip));

	return(page_zip_dir_find_low(end - page_zip_dir_size(page_zip),
				     end - page_zip_dir_user_size(page_zip),
				     offset));
}

/*************************************************************//**
Read a given slot in the dense page directory.
@return record offset on the uncompressed page, possibly ORed with
PAGE_ZIP_DIR_SLOT_DEL or PAGE_ZIP_DIR_SLOT_OWNED */
UNIV_INLINE
ulint
page_zip_dir_get(
/*=============*/
	const page_zip_des_t*	page_zip,	/*!< in: compressed page */
	ulint			slot)		/*!< in: slot
						(0=first user record) */
{
	ut_ad(page_zip_simple_validate(page_zip));
	ut_ad(slot < page_zip_dir_size(page_zip) / PAGE_ZIP_DIR_SLOT_SIZE);
	return(mach_read_from_2(page_zip->data + page_zip_get_size(page_zip)
				- PAGE_ZIP_DIR_SLOT_SIZE * (slot + 1)));
}

#ifndef UNIV_HOTBACKUP
/**********************************************************************//**
Write a log record of compressing an index page. */
static
void
page_zip_compress_write_log(
/*========================*/
	const page_zip_des_t*	page_zip,/*!< in: compressed page */
	const page_t*		page,	/*!< in: uncompressed page */
	dict_index_t*		index,	/*!< in: index of the B-tree node */
	mtr_t*			mtr)	/*!< in: mini-transaction */
{
	byte*	log_ptr;
	ulint	trailer_size;

	ut_ad(!dict_index_is_ibuf(index));

	log_ptr = mlog_open(mtr, 11 + 2 + 2);

	if (!log_ptr) {

		return;
	}

	/* Read the number of user records. */
	trailer_size = page_dir_get_n_heap(page_zip->data)
		- PAGE_HEAP_NO_USER_LOW;
	/* Multiply by uncompressed of size stored per record */
	if (!page_is_leaf(page)) {
		trailer_size *= PAGE_ZIP_DIR_SLOT_SIZE + REC_NODE_PTR_SIZE;
	} else if (dict_index_is_clust(index)) {
		trailer_size *= PAGE_ZIP_DIR_SLOT_SIZE
			+ DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN;
	} else {
		trailer_size *= PAGE_ZIP_DIR_SLOT_SIZE;
	}
	/* Add the space occupied by BLOB pointers. */
	trailer_size += page_zip->n_blobs * BTR_EXTERN_FIELD_REF_SIZE;
	ut_a(page_zip->m_end > PAGE_DATA);
#if FIL_PAGE_DATA > PAGE_DATA
# error "FIL_PAGE_DATA > PAGE_DATA"
#endif
	ut_a(page_zip->m_end + trailer_size <= page_zip_get_size(page_zip));

	log_ptr = mlog_write_initial_log_record_fast((page_t*) page,
						     MLOG_ZIP_PAGE_COMPRESS,
						     log_ptr, mtr);
	mach_write_to_2(log_ptr, page_zip->m_end - FIL_PAGE_TYPE);
	log_ptr += 2;
	mach_write_to_2(log_ptr, trailer_size);
	log_ptr += 2;
	mlog_close(mtr, log_ptr);

	/* Write FIL_PAGE_PREV and FIL_PAGE_NEXT */
	mlog_catenate_string(mtr, page_zip->data + FIL_PAGE_PREV, 4);
	mlog_catenate_string(mtr, page_zip->data + FIL_PAGE_NEXT, 4);
	/* Write most of the page header, the compressed stream and
	the modification log. */
	mlog_catenate_string(mtr, page_zip->data + FIL_PAGE_TYPE,
			     page_zip->m_end - FIL_PAGE_TYPE);
	/* Write the uncompressed trailer of the compressed page. */
	mlog_catenate_string(mtr, page_zip->data + page_zip_get_size(page_zip)
			     - trailer_size, trailer_size);
}
#endif /* !UNIV_HOTBACKUP */

/******************************************************//**
Determine how many externally stored columns are contained
in existing records with smaller heap_no than rec. */
static
ulint
page_zip_get_n_prev_extern(
/*=======================*/
	const page_zip_des_t*	page_zip,/*!< in: dense page directory on
					compressed page */
	const rec_t*		rec,	/*!< in: compact physical record
					on a B-tree leaf page */
	const dict_index_t*	index)	/*!< in: record descriptor */
{
	const page_t*	page	= page_align(rec);
	ulint		n_ext	= 0;
	ulint		i;
	ulint		left;
	ulint		heap_no;
	ulint		n_recs	= page_get_n_recs(page_zip->data);

	ut_ad(page_is_leaf(page));
	ut_ad(page_is_comp(page));
	ut_ad(dict_table_is_comp(index->table));
	ut_ad(dict_index_is_clust(index));
	ut_ad(!dict_index_is_ibuf(index));

	heap_no = rec_get_heap_no_new(rec);
	ut_ad(heap_no >= PAGE_HEAP_NO_USER_LOW);
	left = heap_no - PAGE_HEAP_NO_USER_LOW;
	if (UNIV_UNLIKELY(!left)) {
		return(0);
	}

	for (i = 0; i < n_recs; i++) {
		const rec_t*	r	= page + (page_zip_dir_get(page_zip, i)
						  & PAGE_ZIP_DIR_SLOT_MASK);

		if (rec_get_heap_no_new(r) < heap_no) {
			n_ext += rec_get_n_extern_new(r, index,
						      ULINT_UNDEFINED);
			if (!--left) {
				break;
			}
		}
	}

	return(n_ext);
}

/**********************************************************************//**
Encode the length of a fixed-length column.
@return	buf + length of encoded val */
static
byte*
page_zip_fixed_field_encode(
/*========================*/
	byte*	buf,	/*!< in: pointer to buffer where to write */
	ulint	val)	/*!< in: value to write */
{
	ut_ad(val >= 2);

	if (UNIV_LIKELY(val < 126)) {
		/*
		0 = nullable variable field of at most 255 bytes length;
		1 = not null variable field of at most 255 bytes length;
		126 = nullable variable field with maximum length >255;
		127 = not null variable field with maximum length >255
		*/
		*buf++ = (byte) val;
	} else {
		*buf++ = (byte) (0x80 | val >> 8);
		*buf++ = (byte) val;
	}

	return(buf);
}

/**********************************************************************//**
Write the index information for the compressed page.
@return	used size of buf */
UNIV_INTERN
ulint
page_zip_fields_encode(
/*===================*/
	ulint			n,	/*!< in: number of fields
					to compress */
	const dict_index_t*	index,	/*!< in: index comprising
					at least n fields */
	ulint			trx_id_pos,
					/*!< in: position of the trx_id column
					in the index, or ULINT_UNDEFINED if
					this is a non-leaf page */
	byte*			buf)	/*!< out: buffer of (n + 1) * 2 bytes */
{
	const byte*	buf_start	= buf;
	ulint		i;
	ulint		col;
	ulint		trx_id_col	= 0;
	/* sum of lengths of preceding non-nullable fixed fields, or 0 */
	ulint		fixed_sum	= 0;

	ut_ad(trx_id_pos == ULINT_UNDEFINED || trx_id_pos < n);

	for (i = col = 0; i < n; i++) {
		dict_field_t*	field = dict_index_get_nth_field(index, i);
		ulint		val;

		if (dict_field_get_col(field)->prtype & DATA_NOT_NULL) {
			val = 1; /* set the "not nullable" flag */
		} else {
			val = 0; /* nullable field */
		}

		if (!field->fixed_len) {
			/* variable-length field */
			const dict_col_t*	column
				= dict_field_get_col(field);

			if (UNIV_UNLIKELY(column->len > 255)
			    || UNIV_UNLIKELY(column->mtype == DATA_BLOB)) {
				val |= 0x7e; /* max > 255 bytes */
			}

			if (fixed_sum) {
				/* write out the length of any
				preceding non-nullable fields */
				buf = page_zip_fixed_field_encode(
					buf, fixed_sum << 1 | 1);
				fixed_sum = 0;
				col++;
			}

			*buf++ = (byte) val;
			col++;
		} else if (val) {
			/* fixed-length non-nullable field */

			if (fixed_sum && UNIV_UNLIKELY
			    (fixed_sum + field->fixed_len
			     > DICT_MAX_FIXED_COL_LEN)) {
				/* Write out the length of the
				preceding non-nullable fields,
				to avoid exceeding the maximum
				length of a fixed-length column. */
				buf = page_zip_fixed_field_encode(
					buf, fixed_sum << 1 | 1);
				fixed_sum = 0;
				col++;
			}

			if (i && UNIV_UNLIKELY(i == trx_id_pos)) {
				if (fixed_sum) {
					/* Write out the length of any
					preceding non-nullable fields,
					and start a new trx_id column. */
					buf = page_zip_fixed_field_encode(
						buf, fixed_sum << 1 | 1);
					col++;
				}

				trx_id_col = col;
				fixed_sum = field->fixed_len;
			} else {
				/* add to the sum */
				fixed_sum += field->fixed_len;
			}
		} else {
			/* fixed-length nullable field */

			if (fixed_sum) {
				/* write out the length of any
				preceding non-nullable fields */
				buf = page_zip_fixed_field_encode(
					buf, fixed_sum << 1 | 1);
				fixed_sum = 0;
				col++;
			}

			buf = page_zip_fixed_field_encode(
				buf, field->fixed_len << 1);
			col++;
		}
	}

	if (fixed_sum) {
		/* Write out the lengths of last fixed-length columns. */
		buf = page_zip_fixed_field_encode(buf, fixed_sum << 1 | 1);
	}

	if (trx_id_pos != ULINT_UNDEFINED) {
		/* Write out the position of the trx_id column */
		i = trx_id_col;
	} else {
		/* Write out the number of nullable fields */
		i = index->n_nullable;
	}

	if (i < 128) {
		*buf++ = (byte) i;
	} else {
		*buf++ = (byte) (0x80 | i >> 8);
		*buf++ = (byte) i;
	}

	ut_ad((ulint) (buf - buf_start) <= (n + 2) * 2);
	return((ulint) (buf - buf_start));
}

/**********************************************************************//**
Populate the dense page directory from the sparse directory. */
static
void
page_zip_dir_encode(
/*================*/
	const page_t*	page,	/*!< in: compact page */
	byte*		buf,	/*!< in: pointer to dense page directory[-1];
				out: dense directory on compressed page */
	const rec_t**	recs)	/*!< in: pointer to an array of 0, or NULL;
				out: dense page directory sorted by ascending
				address (and heap_no) */
{
	const byte*	rec;
	ulint		status;
	ulint		min_mark;
	ulint		heap_no;
	ulint		i;
	ulint		n_heap;
	ulint		offs;

	min_mark = 0;

	if (page_is_leaf(page)) {
		status = REC_STATUS_ORDINARY;
	} else {
		status = REC_STATUS_NODE_PTR;
		if (UNIV_UNLIKELY
		    (mach_read_from_4(page + FIL_PAGE_PREV) == FIL_NULL)) {
			min_mark = REC_INFO_MIN_REC_FLAG;
		}
	}

	n_heap = page_dir_get_n_heap(page);

	/* Traverse the list of stored records in the collation order,
	starting from the first user record. */

	rec = page + PAGE_NEW_INFIMUM;

	i = 0;

	for (;;) {
		ulint	info_bits;
		offs = rec_get_next_offs(rec, TRUE);
		if (UNIV_UNLIKELY(offs == PAGE_NEW_SUPREMUM)) {
			break;
		}
		rec = page + offs;
		heap_no = rec_get_heap_no_new(rec);
		ut_a(heap_no >= PAGE_HEAP_NO_USER_LOW);
		ut_a(heap_no < n_heap);
		ut_a(offs < UNIV_PAGE_SIZE - PAGE_DIR);
		ut_a(offs >= PAGE_ZIP_START);
#if PAGE_ZIP_DIR_SLOT_MASK & (PAGE_ZIP_DIR_SLOT_MASK + 1)
# error "PAGE_ZIP_DIR_SLOT_MASK is not 1 less than a power of 2"
#endif
#if PAGE_ZIP_DIR_SLOT_MASK < UNIV_PAGE_SIZE - 1
# error "PAGE_ZIP_DIR_SLOT_MASK < UNIV_PAGE_SIZE - 1"
#endif
		if (UNIV_UNLIKELY(rec_get_n_owned_new(rec))) {
			offs |= PAGE_ZIP_DIR_SLOT_OWNED;
		}

		info_bits = rec_get_info_bits(rec, TRUE);
		if (info_bits & REC_INFO_DELETED_FLAG) {
			info_bits &= ~REC_INFO_DELETED_FLAG;
			offs |= PAGE_ZIP_DIR_SLOT_DEL;
		}
		ut_a(info_bits == min_mark);
		/* Only the smallest user record can have
		REC_INFO_MIN_REC_FLAG set. */
		min_mark = 0;

		mach_write_to_2(buf - PAGE_ZIP_DIR_SLOT_SIZE * ++i, offs);

		if (UNIV_LIKELY_NULL(recs)) {
			/* Ensure that each heap_no occurs at most once. */
			ut_a(!recs[heap_no - PAGE_HEAP_NO_USER_LOW]);
			/* exclude infimum and supremum */
			recs[heap_no - PAGE_HEAP_NO_USER_LOW] = rec;
		}

		ut_a(rec_get_status(rec) == status);
	}

	offs = page_header_get_field(page, PAGE_FREE);

	/* Traverse the free list (of deleted records). */
	while (offs) {
		ut_ad(!(offs & ~PAGE_ZIP_DIR_SLOT_MASK));
		rec = page + offs;

		heap_no = rec_get_heap_no_new(rec);
		ut_a(heap_no >= PAGE_HEAP_NO_USER_LOW);
		ut_a(heap_no < n_heap);

		ut_a(!rec[-REC_N_NEW_EXTRA_BYTES]); /* info_bits and n_owned */
		ut_a(rec_get_status(rec) == status);

		mach_write_to_2(buf - PAGE_ZIP_DIR_SLOT_SIZE * ++i, offs);

		if (UNIV_LIKELY_NULL(recs)) {
			/* Ensure that each heap_no occurs at most once. */
			ut_a(!recs[heap_no - PAGE_HEAP_NO_USER_LOW]);
			/* exclude infimum and supremum */
			recs[heap_no - PAGE_HEAP_NO_USER_LOW] = rec;
		}

		offs = rec_get_next_offs(rec, TRUE);
	}

	/* Ensure that each heap no occurs at least once. */
	ut_a(i + PAGE_HEAP_NO_USER_LOW == n_heap);
}

extern "C" {

/**********************************************************************//**
Allocate memory for zlib. */
static
void*
page_zip_zalloc(
/*============*/
	void*	opaque,	/*!< in/out: memory heap */
	uInt	items,	/*!< in: number of items to allocate */
	uInt	size)	/*!< in: size of an item in bytes */
{
	return(mem_heap_zalloc(static_cast<mem_heap_t*>(opaque), items * size));
}

/**********************************************************************//**
Deallocate memory for zlib. */
static
void
page_zip_free(
/*==========*/
	void*	opaque __attribute__((unused)),	/*!< in: memory heap */
	void*	address __attribute__((unused)))/*!< in: object to free */
{
}

} /* extern "C" */

/**********************************************************************//**
Configure the zlib allocator to use the given memory heap. */
UNIV_INTERN
void
page_zip_set_alloc(
/*===============*/
	void*		stream,		/*!< in/out: zlib stream */
	mem_heap_t*	heap)		/*!< in: memory heap to use */
{
	z_stream*	strm = static_cast<z_stream*>(stream);

	strm->zalloc = page_zip_zalloc;
	strm->zfree = page_zip_free;
	strm->opaque = heap;
}

#if 0 || defined UNIV_DEBUG || defined UNIV_ZIP_DEBUG
/** Symbol for enabling compression and decompression diagnostics */
# define PAGE_ZIP_COMPRESS_DBG
#endif

#ifdef PAGE_ZIP_COMPRESS_DBG
/** Set this variable in a debugger to enable
excessive logging in page_zip_compress(). */
UNIV_INTERN ibool	page_zip_compress_dbg;
/** Set this variable in a debugger to enable
binary logging of the data passed to deflate().
When this variable is nonzero, it will act
as a log file name generator. */
UNIV_INTERN unsigned	page_zip_compress_log;

/**********************************************************************//**
Wrapper for deflate().  Log the operation if page_zip_compress_dbg is set.
@return	deflate() status: Z_OK, Z_BUF_ERROR, ... */
static
int
page_zip_compress_deflate(
/*======================*/
	FILE*		logfile,/*!< in: log file, or NULL */
	z_streamp	strm,	/*!< in/out: compressed stream for deflate() */
	int		flush)	/*!< in: deflate() flushing method */
{
	int	status;
	if (UNIV_UNLIKELY(page_zip_compress_dbg)) {
		ut_print_buf(stderr, strm->next_in, strm->avail_in);
	}
	if (UNIV_LIKELY_NULL(logfile)) {
		fwrite(strm->next_in, 1, strm->avail_in, logfile);
	}
	status = deflate(strm, flush);
	if (UNIV_UNLIKELY(page_zip_compress_dbg)) {
		fprintf(stderr, " -> %d\n", status);
	}
	return(status);
}

/* Redefine deflate(). */
# undef deflate
/** Debug wrapper for the zlib compression routine deflate().
Log the operation if page_zip_compress_dbg is set.
@param strm	in/out: compressed stream
@param flush	in: flushing method
@return		deflate() status: Z_OK, Z_BUF_ERROR, ... */
# define deflate(strm, flush) page_zip_compress_deflate(logfile, strm, flush)
/** Declaration of the logfile parameter */
# define FILE_LOGFILE FILE* logfile,
/** The logfile parameter */
# define LOGFILE logfile,
#else /* PAGE_ZIP_COMPRESS_DBG */
/** Empty declaration of the logfile parameter */
# define FILE_LOGFILE
/** Missing logfile parameter */
# define LOGFILE
#endif /* PAGE_ZIP_COMPRESS_DBG */

/**********************************************************************//**
Compress the records of a node pointer page.
@return	Z_OK, or a zlib error code */
static
int
page_zip_compress_node_ptrs(
/*========================*/
	FILE_LOGFILE
	z_stream*	c_stream,	/*!< in/out: compressed page stream */
	const rec_t**	recs,		/*!< in: dense page directory
					sorted by address */
	ulint		n_dense,	/*!< in: size of recs[] */
	dict_index_t*	index,		/*!< in: the index of the page */
	byte*		storage,	/*!< in: end of dense page directory */
	mem_heap_t*	heap)		/*!< in: temporary memory heap */
{
	int	err	= Z_OK;
	ulint*	offsets = NULL;

	do {
		const rec_t*	rec = *recs++;

		offsets = rec_get_offsets(rec, index, offsets,
					  ULINT_UNDEFINED, &heap);
		/* Only leaf nodes may contain externally stored columns. */
		ut_ad(!rec_offs_any_extern(offsets));

		UNIV_MEM_ASSERT_RW(rec, rec_offs_data_size(offsets));
		UNIV_MEM_ASSERT_RW(rec - rec_offs_extra_size(offsets),
				   rec_offs_extra_size(offsets));

		/* Compress the extra bytes. */
		c_stream->avail_in = rec - REC_N_NEW_EXTRA_BYTES
			- c_stream->next_in;

		if (c_stream->avail_in) {
			err = deflate(c_stream, Z_NO_FLUSH);
			if (UNIV_UNLIKELY(err != Z_OK)) {
				break;
			}
		}
		ut_ad(!c_stream->avail_in);

		/* Compress the data bytes, except node_ptr. */
		c_stream->next_in = (byte*) rec;
		c_stream->avail_in = rec_offs_data_size(offsets)
			- REC_NODE_PTR_SIZE;
		ut_ad(c_stream->avail_in);

		err = deflate(c_stream, Z_NO_FLUSH);
		if (UNIV_UNLIKELY(err != Z_OK)) {
			break;
		}

		ut_ad(!c_stream->avail_in);

		memcpy(storage - REC_NODE_PTR_SIZE
		       * (rec_get_heap_no_new(rec) - 1),
		       c_stream->next_in, REC_NODE_PTR_SIZE);
		c_stream->next_in += REC_NODE_PTR_SIZE;
	} while (--n_dense);

	return(err);
}

/**********************************************************************//**
Compress the records of a leaf node of a secondary index.
@return	Z_OK, or a zlib error code */
static
int
page_zip_compress_sec(
/*==================*/
	FILE_LOGFILE
	z_stream*	c_stream,	/*!< in/out: compressed page stream */
	const rec_t**	recs,		/*!< in: dense page directory
					sorted by address */
	ulint		n_dense)	/*!< in: size of recs[] */
{
	int		err	= Z_OK;

	ut_ad(n_dense > 0);

	do {
		const rec_t*	rec = *recs++;

		/* Compress everything up to this record. */
		c_stream->avail_in = rec - REC_N_NEW_EXTRA_BYTES
			- c_stream->next_in;

		if (UNIV_LIKELY(c_stream->avail_in)) {
			UNIV_MEM_ASSERT_RW(c_stream->next_in,
					   c_stream->avail_in);
			err = deflate(c_stream, Z_NO_FLUSH);
			if (UNIV_UNLIKELY(err != Z_OK)) {
				break;
			}
		}

		ut_ad(!c_stream->avail_in);
		ut_ad(c_stream->next_in == rec - REC_N_NEW_EXTRA_BYTES);

		/* Skip the REC_N_NEW_EXTRA_BYTES. */

		c_stream->next_in = (byte*) rec;
	} while (--n_dense);

	return(err);
}

/**********************************************************************//**
Compress a record of a leaf node of a clustered index that contains
externally stored columns.
@return	Z_OK, or a zlib error code */
static
int
page_zip_compress_clust_ext(
/*========================*/
	FILE_LOGFILE
	z_stream*	c_stream,	/*!< in/out: compressed page stream */
	const rec_t*	rec,		/*!< in: record */
	const ulint*	offsets,	/*!< in: rec_get_offsets(rec) */
	ulint		trx_id_col,	/*!< in: position of of DB_TRX_ID */
	byte*		deleted,	/*!< in: dense directory entry pointing
					to the head of the free list */
	byte*		storage,	/*!< in: end of dense page directory */
	byte**		externs,	/*!< in/out: pointer to the next
					available BLOB pointer */
	ulint*		n_blobs)	/*!< in/out: number of
					externally stored columns */
{
	int	err;
	ulint	i;

	UNIV_MEM_ASSERT_RW(rec, rec_offs_data_size(offsets));
	UNIV_MEM_ASSERT_RW(rec - rec_offs_extra_size(offsets),
			   rec_offs_extra_size(offsets));

	for (i = 0; i < rec_offs_n_fields(offsets); i++) {
		ulint		len;
		const byte*	src;

		if (UNIV_UNLIKELY(i == trx_id_col)) {
			ut_ad(!rec_offs_nth_extern(offsets, i));
			/* Store trx_id and roll_ptr
			in uncompressed form. */
			src = rec_get_nth_field(rec, offsets, i, &len);
			ut_ad(src + DATA_TRX_ID_LEN
			      == rec_get_nth_field(rec, offsets,
						   i + 1, &len));
			ut_ad(len == DATA_ROLL_PTR_LEN);

			/* Compress any preceding bytes. */
			c_stream->avail_in
				= src - c_stream->next_in;

			if (c_stream->avail_in) {
				err = deflate(c_stream, Z_NO_FLUSH);
				if (UNIV_UNLIKELY(err != Z_OK)) {

					return(err);
				}
			}

			ut_ad(!c_stream->avail_in);
			ut_ad(c_stream->next_in == src);

			memcpy(storage
			       - (DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN)
			       * (rec_get_heap_no_new(rec) - 1),
			       c_stream->next_in,
			       DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN);

			c_stream->next_in
				+= DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN;

			/* Skip also roll_ptr */
			i++;
		} else if (rec_offs_nth_extern(offsets, i)) {
			src = rec_get_nth_field(rec, offsets, i, &len);
			ut_ad(len >= BTR_EXTERN_FIELD_REF_SIZE);
			src += len - BTR_EXTERN_FIELD_REF_SIZE;

			c_stream->avail_in = src
				- c_stream->next_in;
			if (UNIV_LIKELY(c_stream->avail_in)) {
				err = deflate(c_stream, Z_NO_FLUSH);
				if (UNIV_UNLIKELY(err != Z_OK)) {

					return(err);
				}
			}

			ut_ad(!c_stream->avail_in);
			ut_ad(c_stream->next_in == src);

			/* Reserve space for the data at
			the end of the space reserved for
			the compressed data and the page
			modification log. */

			if (UNIV_UNLIKELY
			    (c_stream->avail_out
			     <= BTR_EXTERN_FIELD_REF_SIZE)) {
				/* out of space */
				return(Z_BUF_ERROR);
			}

			ut_ad(*externs == c_stream->next_out
			      + c_stream->avail_out
			      + 1/* end of modif. log */);

			c_stream->next_in
				+= BTR_EXTERN_FIELD_REF_SIZE;

			/* Skip deleted records. */
			if (UNIV_LIKELY_NULL
			    (page_zip_dir_find_low(
				    storage, deleted,
				    page_offset(rec)))) {
				continue;
			}

			(*n_blobs)++;
			c_stream->avail_out
				-= BTR_EXTERN_FIELD_REF_SIZE;
			*externs -= BTR_EXTERN_FIELD_REF_SIZE;

			/* Copy the BLOB pointer */
			memcpy(*externs, c_stream->next_in
			       - BTR_EXTERN_FIELD_REF_SIZE,
			       BTR_EXTERN_FIELD_REF_SIZE);
		}
	}

	return(Z_OK);
}

/**********************************************************************//**
Compress the records of a leaf node of a clustered index.
@return	Z_OK, or a zlib error code */
static
int
page_zip_compress_clust(
/*====================*/
	FILE_LOGFILE
	z_stream*	c_stream,	/*!< in/out: compressed page stream */
	const rec_t**	recs,		/*!< in: dense page directory
					sorted by address */
	ulint		n_dense,	/*!< in: size of recs[] */
	dict_index_t*	index,		/*!< in: the index of the page */
	ulint*		n_blobs,	/*!< in: 0; out: number of
					externally stored columns */
	ulint		trx_id_col,	/*!< index of the trx_id column */
	byte*		deleted,	/*!< in: dense directory entry pointing
					to the head of the free list */
	byte*		storage,	/*!< in: end of dense page directory */
	mem_heap_t*	heap)		/*!< in: temporary memory heap */
{
	int	err		= Z_OK;
	ulint*	offsets		= NULL;
	/* BTR_EXTERN_FIELD_REF storage */
	byte*	externs		= storage - n_dense
		* (DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN);

	ut_ad(*n_blobs == 0);

	do {
		const rec_t*	rec = *recs++;

		offsets = rec_get_offsets(rec, index, offsets,
					  ULINT_UNDEFINED, &heap);
		ut_ad(rec_offs_n_fields(offsets)
		      == dict_index_get_n_fields(index));
		UNIV_MEM_ASSERT_RW(rec, rec_offs_data_size(offsets));
		UNIV_MEM_ASSERT_RW(rec - rec_offs_extra_size(offsets),
				   rec_offs_extra_size(offsets));

		/* Compress the extra bytes. */
		c_stream->avail_in = rec - REC_N_NEW_EXTRA_BYTES
			- c_stream->next_in;

		if (c_stream->avail_in) {
			err = deflate(c_stream, Z_NO_FLUSH);
			if (UNIV_UNLIKELY(err != Z_OK)) {

				goto func_exit;
			}
		}
		ut_ad(!c_stream->avail_in);
		ut_ad(c_stream->next_in == rec - REC_N_NEW_EXTRA_BYTES);

		/* Compress the data bytes. */

		c_stream->next_in = (byte*) rec;

		/* Check if there are any externally stored columns.
		For each externally stored column, store the
		BTR_EXTERN_FIELD_REF separately. */
		if (rec_offs_any_extern(offsets)) {
			ut_ad(dict_index_is_clust(index));

			err = page_zip_compress_clust_ext(
				LOGFILE
				c_stream, rec, offsets, trx_id_col,
				deleted, storage, &externs, n_blobs);

			if (UNIV_UNLIKELY(err != Z_OK)) {

				goto func_exit;
			}
		} else {
			ulint		len;
			const byte*	src;

			/* Store trx_id and roll_ptr in uncompressed form. */
			src = rec_get_nth_field(rec, offsets,
						trx_id_col, &len);
			ut_ad(src + DATA_TRX_ID_LEN
			      == rec_get_nth_field(rec, offsets,
						   trx_id_col + 1, &len));
			ut_ad(len == DATA_ROLL_PTR_LEN);
			UNIV_MEM_ASSERT_RW(rec, rec_offs_data_size(offsets));
			UNIV_MEM_ASSERT_RW(rec - rec_offs_extra_size(offsets),
					   rec_offs_extra_size(offsets));

			/* Compress any preceding bytes. */
			c_stream->avail_in = src - c_stream->next_in;

			if (c_stream->avail_in) {
				err = deflate(c_stream, Z_NO_FLUSH);
				if (UNIV_UNLIKELY(err != Z_OK)) {

					return(err);
				}
			}

			ut_ad(!c_stream->avail_in);
			ut_ad(c_stream->next_in == src);

			memcpy(storage
			       - (DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN)
			       * (rec_get_heap_no_new(rec) - 1),
			       c_stream->next_in,
			       DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN);

			c_stream->next_in
				+= DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN;

			/* Skip also roll_ptr */
			ut_ad(trx_id_col + 1 < rec_offs_n_fields(offsets));
		}

		/* Compress the last bytes of the record. */
		c_stream->avail_in = rec + rec_offs_data_size(offsets)
			- c_stream->next_in;

		if (c_stream->avail_in) {
			err = deflate(c_stream, Z_NO_FLUSH);
			if (UNIV_UNLIKELY(err != Z_OK)) {

				goto func_exit;
			}
		}
		ut_ad(!c_stream->avail_in);
	} while (--n_dense);

func_exit:
	return(err);}

/**********************************************************************//**
Compress a page.
@return TRUE on success, FALSE on failure; page_zip will be left
intact on failure. */
UNIV_INTERN
ibool
page_zip_compress(
/*==============*/
	page_zip_des_t*		page_zip,	/*!< in: size; out: data,
						n_blobs, m_start, m_end,
						m_nonempty */
	const page_t*		page,		/*!< in: uncompressed page */
	dict_index_t*		index,		/*!< in: index of the B-tree
						node */
	ulint			level,		/*!< in: commpression level */
	const redo_page_compress_t* page_comp_info,
						/*!< in: used for applying
						MLOG_FILE_TRUNCATE redo log
						record during recovery */
	mtr_t*			mtr)		/*!< in/out: mini-transaction,
						or NULL */
{
	z_stream		c_stream;
	int			err;
	ulint			n_fields;	/* number of index fields
						needed */
	byte*			fields;		/*!< index field information */
	byte*			buf;		/*!< compressed payload of the
						page */
	byte*			buf_end;	/* end of buf */
	ulint			n_dense;
	ulint			slot_size;	/* amount of uncompressed bytes
						per record */
	const rec_t**		recs;		/*!< dense page directory,
						sorted by address */
	mem_heap_t*		heap;
	ulint			trx_id_col;
	ulint			n_blobs	= 0;
	byte*			storage;	/* storage of uncompressed
						columns */
	ulint			ind_id;
#ifndef UNIV_HOTBACKUP
	ullint			usec = ut_time_us(NULL);
#endif /* !UNIV_HOTBACKUP */
#ifdef PAGE_ZIP_COMPRESS_DBG
	FILE*			logfile = NULL;
#endif
	/* A local copy of srv_cmp_per_index_enabled to avoid reading that
	variable multiple times in this function since it can be changed at
	anytime. */
	my_bool			cmp_per_index_enabled;
	cmp_per_index_enabled	= srv_cmp_per_index_enabled;

	ut_a(page_is_comp(page));
	ut_a(fil_page_get_type(page) == FIL_PAGE_INDEX);
	ut_ad(page_simple_validate_new((page_t*) page));
	ut_ad(page_zip_simple_validate(page_zip));
	if (index) {
		ut_ad(dict_table_is_comp(index->table));
		ut_ad(!dict_index_is_ibuf(index));
	}

	UNIV_MEM_ASSERT_RW(page, UNIV_PAGE_SIZE);

	/* Check the data that will be omitted. */
	ut_a(!memcmp(page + (PAGE_NEW_INFIMUM - REC_N_NEW_EXTRA_BYTES),
		     infimum_extra, sizeof infimum_extra));
	ut_a(!memcmp(page + PAGE_NEW_INFIMUM,
		     infimum_data, sizeof infimum_data));
	ut_a(page[PAGE_NEW_SUPREMUM - REC_N_NEW_EXTRA_BYTES]
	     /* info_bits == 0, n_owned <= max */
	     <= PAGE_DIR_SLOT_MAX_N_OWNED);
	ut_a(!memcmp(page + (PAGE_NEW_SUPREMUM - REC_N_NEW_EXTRA_BYTES + 1),
		     supremum_extra_data, sizeof supremum_extra_data));

	if (UNIV_UNLIKELY(!page_get_n_recs(page))) {
		ut_a(rec_get_next_offs(page + PAGE_NEW_INFIMUM, TRUE)
		     == PAGE_NEW_SUPREMUM);
	}

	if (fil_space_is_truncated(page_get_space_id(page))) {
		ut_ad(page_comp_info != NULL);
		n_fields = page_comp_info->fields_num;
		ind_id = page_comp_info->index_id;
	} else {
		if (page_is_leaf(page)) {
			n_fields = dict_index_get_n_fields(index);
		} else {
			n_fields = dict_index_get_n_unique_in_tree(index);
		}
		ind_id = index->id;
	}

	/* The dense directory excludes the infimum and supremum records. */
	n_dense = page_dir_get_n_heap(page) - PAGE_HEAP_NO_USER_LOW;
#ifdef PAGE_ZIP_COMPRESS_DBG
	if (UNIV_UNLIKELY(page_zip_compress_dbg)) {
		fprintf(stderr, "compress %p %p %lu %lu %lu\n",
			(void*) page_zip, (void*) page,
			page_is_leaf(page),
			n_fields, n_dense);
	}
	if (UNIV_UNLIKELY(page_zip_compress_log)) {
		/* Create a log file for every compression attempt. */
		char	logfilename[9];
		ut_snprintf(logfilename, sizeof logfilename,
			    "%08x", page_zip_compress_log++);
		logfile = fopen(logfilename, "wb");

		if (logfile) {
			/* Write the uncompressed page to the log. */
			fwrite(page, 1, UNIV_PAGE_SIZE, logfile);
			/* Record the compressed size as zero.
			This will be overwritten at successful exit. */
			putc(0, logfile);
			putc(0, logfile);
			putc(0, logfile);
			putc(0, logfile);
		}
	}
#endif /* PAGE_ZIP_COMPRESS_DBG */
#ifndef UNIV_HOTBACKUP
	page_zip_stat[page_zip->ssize - 1].compressed++;
	if (cmp_per_index_enabled) {
		mutex_enter(&page_zip_stat_per_index_mutex);
		page_zip_stat_per_index[ind_id].compressed++;
		mutex_exit(&page_zip_stat_per_index_mutex);
	}
#endif /* !UNIV_HOTBACKUP */

	if (UNIV_UNLIKELY(n_dense * PAGE_ZIP_DIR_SLOT_SIZE
			  >= page_zip_get_size(page_zip))) {

		goto err_exit;
	}

	MONITOR_INC(MONITOR_PAGE_COMPRESS);

	heap = mem_heap_create(page_zip_get_size(page_zip)
			       + n_fields * (2 + sizeof(ulint))
			       + REC_OFFS_HEADER_SIZE
			       + n_dense * ((sizeof *recs)
					    - PAGE_ZIP_DIR_SLOT_SIZE)
			       + UNIV_PAGE_SIZE * 4
			       + (512 << MAX_MEM_LEVEL));

	recs = static_cast<const rec_t**>(
		mem_heap_zalloc(heap, n_dense * sizeof *recs));

	fields = static_cast<byte*>(mem_heap_alloc(heap, (n_fields + 1) * 2));

	buf = static_cast<byte*>(
		mem_heap_alloc(heap, page_zip_get_size(page_zip) - PAGE_DATA));

	buf_end = buf + page_zip_get_size(page_zip) - PAGE_DATA;

	/* Compress the data payload. */
	page_zip_set_alloc(&c_stream, heap);

	err = deflateInit2(&c_stream, level,
			   Z_DEFLATED, UNIV_PAGE_SIZE_SHIFT,
			   MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY);
	ut_a(err == Z_OK);

	c_stream.next_out = buf;
	/* Subtract the space reserved for uncompressed data. */
	/* Page header and the end marker of the modification log */
	c_stream.avail_out = buf_end - buf - 1;
	/* Dense page directory and uncompressed columns, if any */
	if (page_is_leaf(page)) {
		if ((index && dict_index_is_clust(index))
			|| (page_comp_info
			&& (page_comp_info->type & DICT_CLUSTERED))) {
			if (index) {
				trx_id_col = dict_index_get_sys_col_pos(
					index, DATA_TRX_ID);
				ut_ad(trx_id_col > 0);
				ut_ad(trx_id_col != ULINT_UNDEFINED);
			}
			slot_size = PAGE_ZIP_DIR_SLOT_SIZE
				+ DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN;
		} else {
			/* Signal the absence of trx_id
			in page_zip_fields_encode() */
			if (index) {
				ut_ad(dict_index_get_sys_col_pos(
					index, DATA_TRX_ID) == ULINT_UNDEFINED);
			}
			trx_id_col = 0;
			slot_size = PAGE_ZIP_DIR_SLOT_SIZE;
		}
	} else {
		slot_size = PAGE_ZIP_DIR_SLOT_SIZE + REC_NODE_PTR_SIZE;
		trx_id_col = ULINT_UNDEFINED;
	}

	if (UNIV_UNLIKELY(c_stream.avail_out <= n_dense * slot_size
			  + 6/* sizeof(zlib header and footer) */)) {
		goto zlib_error;
	}

	c_stream.avail_out -= n_dense * slot_size;
	if (fil_space_is_truncated(page_get_space_id(page))) {
		ut_ad(page_comp_info != NULL);
		c_stream.avail_in = page_comp_info->field_len;
		for (ulint i = 0; i < page_comp_info->field_len; i++) {
			fields[i] = page_comp_info->field_buf[i];
		}
	} else {
		c_stream.avail_in = page_zip_fields_encode(
			n_fields, index, trx_id_col, fields);
	}
	c_stream.next_in = fields;
	if (UNIV_LIKELY(!trx_id_col)) {
		trx_id_col = ULINT_UNDEFINED;
	}

	UNIV_MEM_ASSERT_RW(c_stream.next_in, c_stream.avail_in);
	err = deflate(&c_stream, Z_FULL_FLUSH);
	if (err != Z_OK) {
		goto zlib_error;
	}

	ut_ad(!c_stream.avail_in);

	page_zip_dir_encode(page, buf_end, recs);

	c_stream.next_in = (byte*) page + PAGE_ZIP_START;

	storage = buf_end - n_dense * PAGE_ZIP_DIR_SLOT_SIZE;

	/* Compress the records in heap_no order. */
	if (UNIV_UNLIKELY(!n_dense)) {
	} else if (!page_is_leaf(page)) {
		/* This is a node pointer page. */
		err = page_zip_compress_node_ptrs(LOGFILE
						  &c_stream, recs, n_dense,
						  index, storage, heap);
		if (UNIV_UNLIKELY(err != Z_OK)) {
			goto zlib_error;
		}
	} else if (UNIV_LIKELY(trx_id_col == ULINT_UNDEFINED)) {
		/* This is a leaf page in a secondary index. */
		err = page_zip_compress_sec(LOGFILE
					    &c_stream, recs, n_dense);
		if (UNIV_UNLIKELY(err != Z_OK)) {
			goto zlib_error;
		}
	} else {
		/* This is a leaf page in a clustered index. */
		err = page_zip_compress_clust(LOGFILE
					      &c_stream, recs, n_dense,
					      index, &n_blobs, trx_id_col,
					      buf_end - PAGE_ZIP_DIR_SLOT_SIZE
					      * page_get_n_recs(page),
					      storage, heap);
		if (UNIV_UNLIKELY(err != Z_OK)) {
			goto zlib_error;
		}
	}

	/* Finish the compression. */
	ut_ad(!c_stream.avail_in);
	/* Compress any trailing garbage, in case the last record was
	allocated from an originally longer space on the free list,
	or the data of the last record from page_zip_compress_sec(). */
	c_stream.avail_in
		= page_header_get_field(page, PAGE_HEAP_TOP)
		- (c_stream.next_in - page);
	ut_a(c_stream.avail_in <= UNIV_PAGE_SIZE - PAGE_ZIP_START - PAGE_DIR);

	UNIV_MEM_ASSERT_RW(c_stream.next_in, c_stream.avail_in);
	err = deflate(&c_stream, Z_FINISH);

	if (UNIV_UNLIKELY(err != Z_STREAM_END)) {
zlib_error:
		deflateEnd(&c_stream);
		mem_heap_free(heap);
err_exit:
#ifdef PAGE_ZIP_COMPRESS_DBG
		if (logfile) {
			fclose(logfile);
		}
#endif /* PAGE_ZIP_COMPRESS_DBG */
#ifndef UNIV_HOTBACKUP
		if (page_is_leaf(page) && index) {
			dict_index_zip_failure(index);
		}

		ullint	time_diff = ut_time_us(NULL) - usec;
		page_zip_stat[page_zip->ssize - 1].compressed_usec
			+= time_diff;
		if (cmp_per_index_enabled) {
			mutex_enter(&page_zip_stat_per_index_mutex);
			page_zip_stat_per_index[ind_id].compressed_usec
				+= time_diff;
			mutex_exit(&page_zip_stat_per_index_mutex);
		}
#endif /* !UNIV_HOTBACKUP */
		return(FALSE);
	}

	err = deflateEnd(&c_stream);
	ut_a(err == Z_OK);

	ut_ad(buf + c_stream.total_out == c_stream.next_out);
	ut_ad((ulint) (storage - c_stream.next_out) >= c_stream.avail_out);

	/* Valgrind believes that zlib does not initialize some bits
	in the last 7 or 8 bytes of the stream.  Make Valgrind happy. */
	UNIV_MEM_VALID(buf, c_stream.total_out);

	/* Zero out the area reserved for the modification log.
	Space for the end marker of the modification log is not
	included in avail_out. */
	memset(c_stream.next_out, 0, c_stream.avail_out + 1/* end marker */);

#ifdef UNIV_DEBUG
	page_zip->m_start =
#endif /* UNIV_DEBUG */
		page_zip->m_end = PAGE_DATA + c_stream.total_out;
	page_zip->m_nonempty = FALSE;
	page_zip->n_blobs = n_blobs;
	/* Copy those header fields that will not be written
	in buf_flush_init_for_writing() */
	memcpy(page_zip->data + FIL_PAGE_PREV, page + FIL_PAGE_PREV,
	       FIL_PAGE_LSN - FIL_PAGE_PREV);
	memcpy(page_zip->data + FIL_PAGE_TYPE, page + FIL_PAGE_TYPE, 2);
	memcpy(page_zip->data + FIL_PAGE_DATA, page + FIL_PAGE_DATA,
	       PAGE_DATA - FIL_PAGE_DATA);
	/* Copy the rest of the compressed page */
	memcpy(page_zip->data + PAGE_DATA, buf,
	       page_zip_get_size(page_zip) - PAGE_DATA);
	mem_heap_free(heap);
#ifdef UNIV_ZIP_DEBUG
	ut_a(page_zip_validate(page_zip, page, index));
#endif /* UNIV_ZIP_DEBUG */

	if (mtr) {
#ifndef UNIV_HOTBACKUP
		page_zip_compress_write_log(page_zip, page, index, mtr);
#endif /* !UNIV_HOTBACKUP */
	}

	UNIV_MEM_ASSERT_RW(page_zip->data, page_zip_get_size(page_zip));

#ifdef PAGE_ZIP_COMPRESS_DBG
	if (logfile) {
		/* Record the compressed size of the block. */
		byte sz[4];
		mach_write_to_4(sz, c_stream.total_out);
		fseek(logfile, UNIV_PAGE_SIZE, SEEK_SET);
		fwrite(sz, 1, sizeof sz, logfile);
		fclose(logfile);
	}
#endif /* PAGE_ZIP_COMPRESS_DBG */
#ifndef UNIV_HOTBACKUP
	ullint	time_diff = ut_time_us(NULL) - usec;
	page_zip_stat[page_zip->ssize - 1].compressed_ok++;
	page_zip_stat[page_zip->ssize - 1].compressed_usec += time_diff;
	if (cmp_per_index_enabled) {
		mutex_enter(&page_zip_stat_per_index_mutex);
		page_zip_stat_per_index[ind_id].compressed_ok++;
		page_zip_stat_per_index[ind_id].compressed_usec += time_diff;
		mutex_exit(&page_zip_stat_per_index_mutex);
	}

	if (page_is_leaf(page)
	    && !fil_space_is_truncated(page_get_space_id(page))) {
		dict_index_zip_success(index);
	}
#endif /* !UNIV_HOTBACKUP */

	return(TRUE);
}

/**********************************************************************//**
Compare two page directory entries.
@return	positive if rec1 > rec2 */
UNIV_INLINE
ibool
page_zip_dir_cmp(
/*=============*/
	const rec_t*	rec1,	/*!< in: rec1 */
	const rec_t*	rec2)	/*!< in: rec2 */
{
	return(rec1 > rec2);
}

/**********************************************************************//**
Sort the dense page directory by address (heap_no). */
static
void
page_zip_dir_sort(
/*==============*/
	rec_t**	arr,	/*!< in/out: dense page directory */
	rec_t**	aux_arr,/*!< in/out: work area */
	ulint	low,	/*!< in: lower bound of the sorting area, inclusive */
	ulint	high)	/*!< in: upper bound of the sorting area, exclusive */
{
	UT_SORT_FUNCTION_BODY(page_zip_dir_sort, arr, aux_arr, low, high,
			      page_zip_dir_cmp);
}

/**********************************************************************//**
Deallocate the index information initialized by page_zip_fields_decode(). */
static
void
page_zip_fields_free(
/*=================*/
	dict_index_t*	index)	/*!< in: dummy index to be freed */
{
	if (index) {
		dict_table_t*	table = index->table;
		os_fast_mutex_free(&index->zip_pad.mutex);
		mem_heap_free(index->heap);
		mutex_free(&(table->autoinc_mutex));
		ut_free(table->name);
		mem_heap_free(table->heap);
	}
}

/**********************************************************************//**
Read the index information for the compressed page.
@return	own: dummy index describing the page, or NULL on error */
static
dict_index_t*
page_zip_fields_decode(
/*===================*/
	const byte*	buf,	/*!< in: index information */
	const byte*	end,	/*!< in: end of buf */
	ulint*		trx_id_col)/*!< in: NULL for non-leaf pages;
				for leaf pages, pointer to where to store
				the position of the trx_id column */
{
	const byte*	b;
	ulint		n;
	ulint		i;
	ulint		val;
	dict_table_t*	table;
	dict_index_t*	index;

	/* Determine the number of fields. */
	for (b = buf, n = 0; b < end; n++) {
		if (*b++ & 0x80) {
			b++; /* skip the second byte */
		}
	}

	n--; /* n_nullable or trx_id */

	if (UNIV_UNLIKELY(n > REC_MAX_N_FIELDS)) {

		page_zip_fail(("page_zip_fields_decode: n = %lu\n",
			       (ulong) n));
		return(NULL);
	}

	if (UNIV_UNLIKELY(b > end)) {

		page_zip_fail(("page_zip_fields_decode: %p > %p\n",
			       (const void*) b, (const void*) end));
		return(NULL);
	}

	table = dict_mem_table_create("ZIP_DUMMY", DICT_HDR_SPACE, n,
				      DICT_TF_COMPACT, 0);
	index = dict_mem_index_create("ZIP_DUMMY", "ZIP_DUMMY",
				      DICT_HDR_SPACE, 0, n);
	index->table = table;
	index->n_uniq = n;
	/* avoid ut_ad(index->cached) in dict_index_get_n_unique_in_tree */
	index->cached = TRUE;

	/* Initialize the fields. */
	for (b = buf, i = 0; i < n; i++) {
		ulint	mtype;
		ulint	len;

		val = *b++;

		if (UNIV_UNLIKELY(val & 0x80)) {
			/* fixed length > 62 bytes */
			val = (val & 0x7f) << 8 | *b++;
			len = val >> 1;
			mtype = DATA_FIXBINARY;
		} else if (UNIV_UNLIKELY(val >= 126)) {
			/* variable length with max > 255 bytes */
			len = 0x7fff;
			mtype = DATA_BINARY;
		} else if (val <= 1) {
			/* variable length with max <= 255 bytes */
			len = 0;
			mtype = DATA_BINARY;
		} else {
			/* fixed length < 62 bytes */
			len = val >> 1;
			mtype = DATA_FIXBINARY;
		}

		dict_mem_table_add_col(table, NULL, NULL, mtype,
				       val & 1 ? DATA_NOT_NULL : 0, len);
		dict_index_add_col(index, table,
				   dict_table_get_nth_col(table, i), 0);
	}

	val = *b++;
	if (UNIV_UNLIKELY(val & 0x80)) {
		val = (val & 0x7f) << 8 | *b++;
	}

	/* Decode the position of the trx_id column. */
	if (trx_id_col) {
		if (!val) {
			val = ULINT_UNDEFINED;
		} else if (UNIV_UNLIKELY(val >= n)) {
			page_zip_fields_free(index);
			index = NULL;
		} else {
			index->type = DICT_CLUSTERED;
		}

		*trx_id_col = val;
	} else {
		/* Decode the number of nullable fields. */
		if (UNIV_UNLIKELY(index->n_nullable > val)) {
			page_zip_fields_free(index);
			index = NULL;
		} else {
			index->n_nullable = val;
		}
	}

	ut_ad(b == end);

	return(index);
}

/**********************************************************************//**
Populate the sparse page directory from the dense directory.
@return	TRUE on success, FALSE on failure */
static
ibool
page_zip_dir_decode(
/*================*/
	const page_zip_des_t*	page_zip,/*!< in: dense page directory on
					compressed page */
	page_t*			page,	/*!< in: compact page with valid header;
					out: trailer and sparse page directory
					filled in */
	rec_t**			recs,	/*!< out: dense page directory sorted by
					ascending address (and heap_no) */
	rec_t**			recs_aux,/*!< in/out: scratch area */
	ulint			n_dense)/*!< in: number of user records, and
					size of recs[] and recs_aux[] */
{
	ulint	i;
	ulint	n_recs;
	byte*	slot;

	n_recs = page_get_n_recs(page);

	if (UNIV_UNLIKELY(n_recs > n_dense)) {
		page_zip_fail(("page_zip_dir_decode 1: %lu > %lu\n",
			       (ulong) n_recs, (ulong) n_dense));
		return(FALSE);
	}

	/* Traverse the list of stored records in the sorting order,
	starting from the first user record. */

	slot = page + (UNIV_PAGE_SIZE - PAGE_DIR - PAGE_DIR_SLOT_SIZE);
	UNIV_PREFETCH_RW(slot);

	/* Zero out the page trailer. */
	memset(slot + PAGE_DIR_SLOT_SIZE, 0, PAGE_DIR);

	mach_write_to_2(slot, PAGE_NEW_INFIMUM);
	slot -= PAGE_DIR_SLOT_SIZE;
	UNIV_PREFETCH_RW(slot);

	/* Initialize the sparse directory and copy the dense directory. */
	for (i = 0; i < n_recs; i++) {
		ulint	offs = page_zip_dir_get(page_zip, i);

		if (offs & PAGE_ZIP_DIR_SLOT_OWNED) {
			mach_write_to_2(slot, offs & PAGE_ZIP_DIR_SLOT_MASK);
			slot -= PAGE_DIR_SLOT_SIZE;
			UNIV_PREFETCH_RW(slot);
		}

		if (UNIV_UNLIKELY((offs & PAGE_ZIP_DIR_SLOT_MASK)
				  < PAGE_ZIP_START + REC_N_NEW_EXTRA_BYTES)) {
			page_zip_fail(("page_zip_dir_decode 2: %u %u %lx\n",
				       (unsigned) i, (unsigned) n_recs,
				       (ulong) offs));
			return(FALSE);
		}

		recs[i] = page + (offs & PAGE_ZIP_DIR_SLOT_MASK);
	}

	mach_write_to_2(slot, PAGE_NEW_SUPREMUM);
	{
		const page_dir_slot_t*	last_slot = page_dir_get_nth_slot(
			page, page_dir_get_n_slots(page) - 1);

		if (UNIV_UNLIKELY(slot != last_slot)) {
			page_zip_fail(("page_zip_dir_decode 3: %p != %p\n",
				       (const void*) slot,
				       (const void*) last_slot));
			return(FALSE);
		}
	}

	/* Copy the rest of the dense directory. */
	for (; i < n_dense; i++) {
		ulint	offs = page_zip_dir_get(page_zip, i);

		if (UNIV_UNLIKELY(offs & ~PAGE_ZIP_DIR_SLOT_MASK)) {
			page_zip_fail(("page_zip_dir_decode 4: %u %u %lx\n",
				       (unsigned) i, (unsigned) n_dense,
				       (ulong) offs));
			return(FALSE);
		}

		recs[i] = page + offs;
	}

	if (UNIV_LIKELY(n_dense > 1)) {
		page_zip_dir_sort(recs, recs_aux, 0, n_dense);
	}
	return(TRUE);
}

/**********************************************************************//**
Initialize the REC_N_NEW_EXTRA_BYTES of each record.
@return	TRUE on success, FALSE on failure */
static
ibool
page_zip_set_extra_bytes(
/*=====================*/
	const page_zip_des_t*	page_zip,/*!< in: compressed page */
	page_t*			page,	/*!< in/out: uncompressed page */
	ulint			info_bits)/*!< in: REC_INFO_MIN_REC_FLAG or 0 */
{
	ulint	n;
	ulint	i;
	ulint	n_owned = 1;
	ulint	offs;
	rec_t*	rec;

	n = page_get_n_recs(page);
	rec = page + PAGE_NEW_INFIMUM;

	for (i = 0; i < n; i++) {
		offs = page_zip_dir_get(page_zip, i);

		if (offs & PAGE_ZIP_DIR_SLOT_DEL) {
			info_bits |= REC_INFO_DELETED_FLAG;
		}
		if (UNIV_UNLIKELY(offs & PAGE_ZIP_DIR_SLOT_OWNED)) {
			info_bits |= n_owned;
			n_owned = 1;
		} else {
			n_owned++;
		}
		offs &= PAGE_ZIP_DIR_SLOT_MASK;
		if (UNIV_UNLIKELY(offs < PAGE_ZIP_START
				  + REC_N_NEW_EXTRA_BYTES)) {
			page_zip_fail(("page_zip_set_extra_bytes 1:"
				       " %u %u %lx\n",
				       (unsigned) i, (unsigned) n,
				       (ulong) offs));
			return(FALSE);
		}

		rec_set_next_offs_new(rec, offs);
		rec = page + offs;
		rec[-REC_N_NEW_EXTRA_BYTES] = (byte) info_bits;
		info_bits = 0;
	}

	/* Set the next pointer of the last user record. */
	rec_set_next_offs_new(rec, PAGE_NEW_SUPREMUM);

	/* Set n_owned of the supremum record. */
	page[PAGE_NEW_SUPREMUM - REC_N_NEW_EXTRA_BYTES] = (byte) n_owned;

	/* The dense directory excludes the infimum and supremum records. */
	n = page_dir_get_n_heap(page) - PAGE_HEAP_NO_USER_LOW;

	if (i >= n) {
		if (UNIV_LIKELY(i == n)) {
			return(TRUE);
		}

		page_zip_fail(("page_zip_set_extra_bytes 2: %u != %u\n",
			       (unsigned) i, (unsigned) n));
		return(FALSE);
	}

	offs = page_zip_dir_get(page_zip, i);

	/* Set the extra bytes of deleted records on the free list. */
	for (;;) {
		if (UNIV_UNLIKELY(!offs)
		    || UNIV_UNLIKELY(offs & ~PAGE_ZIP_DIR_SLOT_MASK)) {

			page_zip_fail(("page_zip_set_extra_bytes 3: %lx\n",
				       (ulong) offs));
			return(FALSE);
		}

		rec = page + offs;
		rec[-REC_N_NEW_EXTRA_BYTES] = 0; /* info_bits and n_owned */

		if (++i == n) {
			break;
		}

		offs = page_zip_dir_get(page_zip, i);
		rec_set_next_offs_new(rec, offs);
	}

	/* Terminate the free list. */
	rec[-REC_N_NEW_EXTRA_BYTES] = 0; /* info_bits and n_owned */
	rec_set_next_offs_new(rec, 0);

	return(TRUE);
}

/**********************************************************************//**
Apply the modification log to a record containing externally stored
columns.  Do not copy the fields that are stored separately.
@return	pointer to modification log, or NULL on failure */
static
const byte*
page_zip_apply_log_ext(
/*===================*/
	rec_t*		rec,		/*!< in/out: record */
	const ulint*	offsets,	/*!< in: rec_get_offsets(rec) */
	ulint		trx_id_col,	/*!< in: position of of DB_TRX_ID */
	const byte*	data,		/*!< in: modification log */
	const byte*	end)		/*!< in: end of modification log */
{
	ulint	i;
	ulint	len;
	byte*	next_out = rec;

	/* Check if there are any externally stored columns.
	For each externally stored column, skip the
	BTR_EXTERN_FIELD_REF. */

	for (i = 0; i < rec_offs_n_fields(offsets); i++) {
		byte*	dst;

		if (UNIV_UNLIKELY(i == trx_id_col)) {
			/* Skip trx_id and roll_ptr */
			dst = rec_get_nth_field(rec, offsets,
						i, &len);
			if (UNIV_UNLIKELY(dst - next_out >= end - data)
			    || UNIV_UNLIKELY
			    (len < (DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN))
			    || rec_offs_nth_extern(offsets, i)) {
				page_zip_fail(("page_zip_apply_log_ext:"
					       " trx_id len %lu,"
					       " %p - %p >= %p - %p\n",
					       (ulong) len,
					       (const void*) dst,
					       (const void*) next_out,
					       (const void*) end,
					       (const void*) data));
				return(NULL);
			}

			memcpy(next_out, data, dst - next_out);
			data += dst - next_out;
			next_out = dst + (DATA_TRX_ID_LEN
					  + DATA_ROLL_PTR_LEN);
		} else if (rec_offs_nth_extern(offsets, i)) {
			dst = rec_get_nth_field(rec, offsets,
						i, &len);
			ut_ad(len
			      >= BTR_EXTERN_FIELD_REF_SIZE);

			len += dst - next_out
				- BTR_EXTERN_FIELD_REF_SIZE;

			if (UNIV_UNLIKELY(data + len >= end)) {
				page_zip_fail(("page_zip_apply_log_ext: "
					       "ext %p+%lu >= %p\n",
					       (const void*) data,
					       (ulong) len,
					       (const void*) end));
				return(NULL);
			}

			memcpy(next_out, data, len);
			data += len;
			next_out += len
				+ BTR_EXTERN_FIELD_REF_SIZE;
		}
	}

	/* Copy the last bytes of the record. */
	len = rec_get_end(rec, offsets) - next_out;
	if (UNIV_UNLIKELY(data + len >= end)) {
		page_zip_fail(("page_zip_apply_log_ext: "
			       "last %p+%lu >= %p\n",
			       (const void*) data,
			       (ulong) len,
			       (const void*) end));
		return(NULL);
	}
	memcpy(next_out, data, len);
	data += len;

	return(data);
}

/**********************************************************************//**
Apply the modification log to an uncompressed page.
Do not copy the fields that are stored separately.
@return	pointer to end of modification log, or NULL on failure */
static
const byte*
page_zip_apply_log(
/*===============*/
	const byte*	data,	/*!< in: modification log */
	ulint		size,	/*!< in: maximum length of the log, in bytes */
	rec_t**		recs,	/*!< in: dense page directory,
				sorted by address (indexed by
				heap_no - PAGE_HEAP_NO_USER_LOW) */
	ulint		n_dense,/*!< in: size of recs[] */
	ulint		trx_id_col,/*!< in: column number of trx_id in the index,
				or ULINT_UNDEFINED if none */
	ulint		heap_status,
				/*!< in: heap_no and status bits for
				the next record to uncompress */
	dict_index_t*	index,	/*!< in: index of the page */
	ulint*		offsets)/*!< in/out: work area for
				rec_get_offsets_reverse() */
{
	const byte* const end = data + size;

	for (;;) {
		ulint	val;
		rec_t*	rec;
		ulint	len;
		ulint	hs;

		val = *data++;
		if (UNIV_UNLIKELY(!val)) {
			return(data - 1);
		}
		if (val & 0x80) {
			val = (val & 0x7f) << 8 | *data++;
			if (UNIV_UNLIKELY(!val)) {
				page_zip_fail(("page_zip_apply_log:"
					       " invalid val %x%x\n",
					       data[-2], data[-1]));
				return(NULL);
			}
		}
		if (UNIV_UNLIKELY(data >= end)) {
			page_zip_fail(("page_zip_apply_log: %p >= %p\n",
				       (const void*) data,
				       (const void*) end));
			return(NULL);
		}
		if (UNIV_UNLIKELY((val >> 1) > n_dense)) {
			page_zip_fail(("page_zip_apply_log: %lu>>1 > %lu\n",
				       (ulong) val, (ulong) n_dense));
			return(NULL);
		}

		/* Determine the heap number and status bits of the record. */
		rec = recs[(val >> 1) - 1];

		hs = ((val >> 1) + 1) << REC_HEAP_NO_SHIFT;
		hs |= heap_status & ((1 << REC_HEAP_NO_SHIFT) - 1);

		/* This may either be an old record that is being
		overwritten (updated in place, or allocated from
		the free list), or a new record, with the next
		available_heap_no. */
		if (UNIV_UNLIKELY(hs > heap_status)) {
			page_zip_fail(("page_zip_apply_log: %lu > %lu\n",
				       (ulong) hs, (ulong) heap_status));
			return(NULL);
		} else if (hs == heap_status) {
			/* A new record was allocated from the heap. */
			if (UNIV_UNLIKELY(val & 1)) {
				/* Only existing records may be cleared. */
				page_zip_fail(("page_zip_apply_log:"
					       " attempting to create"
					       " deleted rec %lu\n",
					       (ulong) hs));
				return(NULL);
			}
			heap_status += 1 << REC_HEAP_NO_SHIFT;
		}

		mach_write_to_2(rec - REC_NEW_HEAP_NO, hs);

		if (val & 1) {
			/* Clear the data bytes of the record. */
			mem_heap_t*	heap	= NULL;
			ulint*		offs;
			offs = rec_get_offsets(rec, index, offsets,
					       ULINT_UNDEFINED, &heap);
			memset(rec, 0, rec_offs_data_size(offs));

			if (UNIV_LIKELY_NULL(heap)) {
				mem_heap_free(heap);
			}
			continue;
		}

#if REC_STATUS_NODE_PTR != TRUE
# error "REC_STATUS_NODE_PTR != TRUE"
#endif
		rec_get_offsets_reverse(data, index,
					hs & REC_STATUS_NODE_PTR,
					offsets);
		rec_offs_make_valid(rec, index, offsets);

		/* Copy the extra bytes (backwards). */
		{
			byte*	start	= rec_get_start(rec, offsets);
			byte*	b	= rec - REC_N_NEW_EXTRA_BYTES;
			while (b != start) {
				*--b = *data++;
			}
		}

		/* Copy the data bytes. */
		if (UNIV_UNLIKELY(rec_offs_any_extern(offsets))) {
			/* Non-leaf nodes should not contain any
			externally stored columns. */
			if (UNIV_UNLIKELY(hs & REC_STATUS_NODE_PTR)) {
				page_zip_fail(("page_zip_apply_log: "
					       "%lu&REC_STATUS_NODE_PTR\n",
					       (ulong) hs));
				return(NULL);
			}

			data = page_zip_apply_log_ext(
				rec, offsets, trx_id_col, data, end);

			if (UNIV_UNLIKELY(!data)) {
				return(NULL);
			}
		} else if (UNIV_UNLIKELY(hs & REC_STATUS_NODE_PTR)) {
			len = rec_offs_data_size(offsets)
				- REC_NODE_PTR_SIZE;
			/* Copy the data bytes, except node_ptr. */
			if (UNIV_UNLIKELY(data + len >= end)) {
				page_zip_fail(("page_zip_apply_log: "
					       "node_ptr %p+%lu >= %p\n",
					       (const void*) data,
					       (ulong) len,
					       (const void*) end));
				return(NULL);
			}
			memcpy(rec, data, len);
			data += len;
		} else if (UNIV_LIKELY(trx_id_col == ULINT_UNDEFINED)) {
			len = rec_offs_data_size(offsets);

			/* Copy all data bytes of
			a record in a secondary index. */
			if (UNIV_UNLIKELY(data + len >= end)) {
				page_zip_fail(("page_zip_apply_log: "
					       "sec %p+%lu >= %p\n",
					       (const void*) data,
					       (ulong) len,
					       (const void*) end));
				return(NULL);
			}

			memcpy(rec, data, len);
			data += len;
		} else {
			/* Skip DB_TRX_ID and DB_ROLL_PTR. */
			ulint	l = rec_get_nth_field_offs(offsets,
							   trx_id_col, &len);
			byte*	b;

			if (UNIV_UNLIKELY(data + l >= end)
			    || UNIV_UNLIKELY(len < (DATA_TRX_ID_LEN
						    + DATA_ROLL_PTR_LEN))) {
				page_zip_fail(("page_zip_apply_log: "
					       "trx_id %p+%lu >= %p\n",
					       (const void*) data,
					       (ulong) l,
					       (const void*) end));
				return(NULL);
			}

			/* Copy any preceding data bytes. */
			memcpy(rec, data, l);
			data += l;

			/* Copy any bytes following DB_TRX_ID, DB_ROLL_PTR. */
			b = rec + l + (DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN);
			len = rec_get_end(rec, offsets) - b;
			if (UNIV_UNLIKELY(data + len >= end)) {
				page_zip_fail(("page_zip_apply_log: "
					       "clust %p+%lu >= %p\n",
					       (const void*) data,
					       (ulong) len,
					       (const void*) end));
				return(NULL);
			}
			memcpy(b, data, len);
			data += len;
		}
	}
}

/**********************************************************************//**
Decompress the records of a node pointer page.
@return	TRUE on success, FALSE on failure */
static
ibool
page_zip_decompress_node_ptrs(
/*==========================*/
	page_zip_des_t*	page_zip,	/*!< in/out: compressed page */
	z_stream*	d_stream,	/*!< in/out: compressed page stream */
	rec_t**		recs,		/*!< in: dense page directory
					sorted by address */
	ulint		n_dense,	/*!< in: size of recs[] */
	dict_index_t*	index,		/*!< in: the index of the page */
	ulint*		offsets,	/*!< in/out: temporary offsets */
	mem_heap_t*	heap)		/*!< in: temporary memory heap */
{
	ulint		heap_status = REC_STATUS_NODE_PTR
		| PAGE_HEAP_NO_USER_LOW << REC_HEAP_NO_SHIFT;
	ulint		slot;
	const byte*	storage;

	/* Subtract the space reserved for uncompressed data. */
	d_stream->avail_in -= n_dense
		* (PAGE_ZIP_DIR_SLOT_SIZE + REC_NODE_PTR_SIZE);

	/* Decompress the records in heap_no order. */
	for (slot = 0; slot < n_dense; slot++) {
		rec_t*	rec = recs[slot];

		d_stream->avail_out = rec - REC_N_NEW_EXTRA_BYTES
			- d_stream->next_out;

		ut_ad(d_stream->avail_out < UNIV_PAGE_SIZE
		      - PAGE_ZIP_START - PAGE_DIR);
		switch (inflate(d_stream, Z_SYNC_FLUSH)) {
		case Z_STREAM_END:
			/* Apparently, n_dense has grown
			since the time the page was last compressed. */
			goto zlib_done;
		case Z_OK:
		case Z_BUF_ERROR:
			if (!d_stream->avail_out) {
				break;
			}
			/* fall through */
		default:
			page_zip_fail(("page_zip_decompress_node_ptrs:"
				       " 1 inflate(Z_SYNC_FLUSH)=%s\n",
				       d_stream->msg));
			goto zlib_error;
		}

		ut_ad(d_stream->next_out == rec - REC_N_NEW_EXTRA_BYTES);
		/* Prepare to decompress the data bytes. */
		d_stream->next_out = rec;
		/* Set heap_no and the status bits. */
		mach_write_to_2(rec - REC_NEW_HEAP_NO, heap_status);
		heap_status += 1 << REC_HEAP_NO_SHIFT;

		/* Read the offsets. The status bits are needed here. */
		offsets = rec_get_offsets(rec, index, offsets,
					  ULINT_UNDEFINED, &heap);

		/* Non-leaf nodes should not have any externally
		stored columns. */
		ut_ad(!rec_offs_any_extern(offsets));

		/* Decompress the data bytes, except node_ptr. */
		d_stream->avail_out = rec_offs_data_size(offsets)
			- REC_NODE_PTR_SIZE;

		switch (inflate(d_stream, Z_SYNC_FLUSH)) {
		case Z_STREAM_END:
			goto zlib_done;
		case Z_OK:
		case Z_BUF_ERROR:
			if (!d_stream->avail_out) {
				break;
			}
			/* fall through */
		default:
			page_zip_fail(("page_zip_decompress_node_ptrs:"
				       " 2 inflate(Z_SYNC_FLUSH)=%s\n",
				       d_stream->msg));
			goto zlib_error;
		}

		/* Clear the node pointer in case the record
		will be deleted and the space will be reallocated
		to a smaller record. */
		memset(d_stream->next_out, 0, REC_NODE_PTR_SIZE);
		d_stream->next_out += REC_NODE_PTR_SIZE;

		ut_ad(d_stream->next_out == rec_get_end(rec, offsets));
	}

	/* Decompress any trailing garbage, in case the last record was
	allocated from an originally longer space on the free list. */
	d_stream->avail_out = page_header_get_field(page_zip->data,
						    PAGE_HEAP_TOP)
		- page_offset(d_stream->next_out);
	if (UNIV_UNLIKELY(d_stream->avail_out > UNIV_PAGE_SIZE
			  - PAGE_ZIP_START - PAGE_DIR)) {

		page_zip_fail(("page_zip_decompress_node_ptrs:"
			       " avail_out = %u\n",
			       d_stream->avail_out));
		goto zlib_error;
	}

	if (UNIV_UNLIKELY(inflate(d_stream, Z_FINISH) != Z_STREAM_END)) {
		page_zip_fail(("page_zip_decompress_node_ptrs:"
			       " inflate(Z_FINISH)=%s\n",
			       d_stream->msg));
zlib_error:
		inflateEnd(d_stream);
		return(FALSE);
	}

	/* Note that d_stream->avail_out > 0 may hold here
	if the modification log is nonempty. */

zlib_done:
	if (UNIV_UNLIKELY(inflateEnd(d_stream) != Z_OK)) {
		ut_error;
	}

	{
		page_t*	page = page_align(d_stream->next_out);

		/* Clear the unused heap space on the uncompressed page. */
		memset(d_stream->next_out, 0,
		       page_dir_get_nth_slot(page,
					     page_dir_get_n_slots(page) - 1)
		       - d_stream->next_out);
	}

#ifdef UNIV_DEBUG
	page_zip->m_start = PAGE_DATA + d_stream->total_in;
#endif /* UNIV_DEBUG */

	/* Apply the modification log. */
	{
		const byte*	mod_log_ptr;
		mod_log_ptr = page_zip_apply_log(d_stream->next_in,
						 d_stream->avail_in + 1,
						 recs, n_dense,
						 ULINT_UNDEFINED, heap_status,
						 index, offsets);

		if (UNIV_UNLIKELY(!mod_log_ptr)) {
			return(FALSE);
		}
		page_zip->m_end = mod_log_ptr - page_zip->data;
		page_zip->m_nonempty = mod_log_ptr != d_stream->next_in;
	}

	if (UNIV_UNLIKELY
	    (page_zip_get_trailer_len(page_zip,
				      dict_index_is_clust(index), NULL)
	     + page_zip->m_end >= page_zip_get_size(page_zip))) {
		page_zip_fail(("page_zip_decompress_node_ptrs:"
			       " %lu + %lu >= %lu, %lu\n",
			       (ulong) page_zip_get_trailer_len(
				       page_zip, dict_index_is_clust(index),
				       NULL),
			       (ulong) page_zip->m_end,
			       (ulong) page_zip_get_size(page_zip),
			       (ulong) dict_index_is_clust(index)));
		return(FALSE);
	}

	/* Restore the uncompressed columns in heap_no order. */
	storage = page_zip_dir_start_low(page_zip, n_dense);

	for (slot = 0; slot < n_dense; slot++) {
		rec_t*		rec	= recs[slot];

		offsets = rec_get_offsets(rec, index, offsets,
					  ULINT_UNDEFINED, &heap);
		/* Non-leaf nodes should not have any externally
		stored columns. */
		ut_ad(!rec_offs_any_extern(offsets));
		storage -= REC_NODE_PTR_SIZE;

		memcpy(rec_get_end(rec, offsets) - REC_NODE_PTR_SIZE,
		       storage, REC_NODE_PTR_SIZE);
	}

	return(TRUE);
}

/**********************************************************************//**
Decompress the records of a leaf node of a secondary index.
@return	TRUE on success, FALSE on failure */
static
ibool
page_zip_decompress_sec(
/*====================*/
	page_zip_des_t*	page_zip,	/*!< in/out: compressed page */
	z_stream*	d_stream,	/*!< in/out: compressed page stream */
	rec_t**		recs,		/*!< in: dense page directory
					sorted by address */
	ulint		n_dense,	/*!< in: size of recs[] */
	dict_index_t*	index,		/*!< in: the index of the page */
	ulint*		offsets)	/*!< in/out: temporary offsets */
{
	ulint	heap_status	= REC_STATUS_ORDINARY
		| PAGE_HEAP_NO_USER_LOW << REC_HEAP_NO_SHIFT;
	ulint	slot;

	ut_a(!dict_index_is_clust(index));

	/* Subtract the space reserved for uncompressed data. */
	d_stream->avail_in -= n_dense * PAGE_ZIP_DIR_SLOT_SIZE;

	for (slot = 0; slot < n_dense; slot++) {
		rec_t*	rec = recs[slot];

		/* Decompress everything up to this record. */
		d_stream->avail_out = rec - REC_N_NEW_EXTRA_BYTES
			- d_stream->next_out;

		if (UNIV_LIKELY(d_stream->avail_out)) {
			switch (inflate(d_stream, Z_SYNC_FLUSH)) {
			case Z_STREAM_END:
				/* Apparently, n_dense has grown
				since the time the page was last compressed. */
				goto zlib_done;
			case Z_OK:
			case Z_BUF_ERROR:
				if (!d_stream->avail_out) {
					break;
				}
				/* fall through */
			default:
				page_zip_fail(("page_zip_decompress_sec:"
					       " inflate(Z_SYNC_FLUSH)=%s\n",
					       d_stream->msg));
				goto zlib_error;
			}
		}

		ut_ad(d_stream->next_out == rec - REC_N_NEW_EXTRA_BYTES);

		/* Skip the REC_N_NEW_EXTRA_BYTES. */

		d_stream->next_out = rec;

		/* Set heap_no and the status bits. */
		mach_write_to_2(rec - REC_NEW_HEAP_NO, heap_status);
		heap_status += 1 << REC_HEAP_NO_SHIFT;
	}

	/* Decompress the data of the last record and any trailing garbage,
	in case the last record was allocated from an originally longer space
	on the free list. */
	d_stream->avail_out = page_header_get_field(page_zip->data,
						    PAGE_HEAP_TOP)
		- page_offset(d_stream->next_out);
	if (UNIV_UNLIKELY(d_stream->avail_out > UNIV_PAGE_SIZE
			  - PAGE_ZIP_START - PAGE_DIR)) {

		page_zip_fail(("page_zip_decompress_sec:"
			       " avail_out = %u\n",
			       d_stream->avail_out));
		goto zlib_error;
	}

	if (UNIV_UNLIKELY(inflate(d_stream, Z_FINISH) != Z_STREAM_END)) {
		page_zip_fail(("page_zip_decompress_sec:"
			       " inflate(Z_FINISH)=%s\n",
			       d_stream->msg));
zlib_error:
		inflateEnd(d_stream);
		return(FALSE);
	}

	/* Note that d_stream->avail_out > 0 may hold here
	if the modification log is nonempty. */

zlib_done:
	if (UNIV_UNLIKELY(inflateEnd(d_stream) != Z_OK)) {
		ut_error;
	}

	{
		page_t*	page = page_align(d_stream->next_out);

		/* Clear the unused heap space on the uncompressed page. */
		memset(d_stream->next_out, 0,
		       page_dir_get_nth_slot(page,
					     page_dir_get_n_slots(page) - 1)
		       - d_stream->next_out);
	}

#ifdef UNIV_DEBUG
	page_zip->m_start = PAGE_DATA + d_stream->total_in;
#endif /* UNIV_DEBUG */

	/* Apply the modification log. */
	{
		const byte*	mod_log_ptr;
		mod_log_ptr = page_zip_apply_log(d_stream->next_in,
						 d_stream->avail_in + 1,
						 recs, n_dense,
						 ULINT_UNDEFINED, heap_status,
						 index, offsets);

		if (UNIV_UNLIKELY(!mod_log_ptr)) {
			return(FALSE);
		}
		page_zip->m_end = mod_log_ptr - page_zip->data;
		page_zip->m_nonempty = mod_log_ptr != d_stream->next_in;
	}

	if (UNIV_UNLIKELY(page_zip_get_trailer_len(page_zip, FALSE, NULL)
			  + page_zip->m_end >= page_zip_get_size(page_zip))) {

		page_zip_fail(("page_zip_decompress_sec: %lu + %lu >= %lu\n",
			       (ulong) page_zip_get_trailer_len(
				       page_zip, FALSE, NULL),
			       (ulong) page_zip->m_end,
			       (ulong) page_zip_get_size(page_zip)));
		return(FALSE);
	}

	/* There are no uncompressed columns on leaf pages of
	secondary indexes. */

	return(TRUE);
}

/**********************************************************************//**
Decompress a record of a leaf node of a clustered index that contains
externally stored columns.
@return	TRUE on success */
static
ibool
page_zip_decompress_clust_ext(
/*==========================*/
	z_stream*	d_stream,	/*!< in/out: compressed page stream */
	rec_t*		rec,		/*!< in/out: record */
	const ulint*	offsets,	/*!< in: rec_get_offsets(rec) */
	ulint		trx_id_col)	/*!< in: position of of DB_TRX_ID */
{
	ulint	i;

	for (i = 0; i < rec_offs_n_fields(offsets); i++) {
		ulint	len;
		byte*	dst;

		if (UNIV_UNLIKELY(i == trx_id_col)) {
			/* Skip trx_id and roll_ptr */
			dst = rec_get_nth_field(rec, offsets, i, &len);
			if (UNIV_UNLIKELY(len < DATA_TRX_ID_LEN
					  + DATA_ROLL_PTR_LEN)) {

				page_zip_fail(("page_zip_decompress_clust_ext:"
					       " len[%lu] = %lu\n",
					       (ulong) i, (ulong) len));
				return(FALSE);
			}

			if (rec_offs_nth_extern(offsets, i)) {

				page_zip_fail(("page_zip_decompress_clust_ext:"
					       " DB_TRX_ID at %lu is ext\n",
					       (ulong) i));
				return(FALSE);
			}

			d_stream->avail_out = dst - d_stream->next_out;

			switch (inflate(d_stream, Z_SYNC_FLUSH)) {
			case Z_STREAM_END:
			case Z_OK:
			case Z_BUF_ERROR:
				if (!d_stream->avail_out) {
					break;
				}
				/* fall through */
			default:
				page_zip_fail(("page_zip_decompress_clust_ext:"
					       " 1 inflate(Z_SYNC_FLUSH)=%s\n",
					       d_stream->msg));
				return(FALSE);
			}

			ut_ad(d_stream->next_out == dst);

			/* Clear DB_TRX_ID and DB_ROLL_PTR in order to
			avoid uninitialized bytes in case the record
			is affected by page_zip_apply_log(). */
			memset(dst, 0, DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN);

			d_stream->next_out += DATA_TRX_ID_LEN
				+ DATA_ROLL_PTR_LEN;
		} else if (rec_offs_nth_extern(offsets, i)) {
			dst = rec_get_nth_field(rec, offsets, i, &len);
			ut_ad(len >= BTR_EXTERN_FIELD_REF_SIZE);
			dst += len - BTR_EXTERN_FIELD_REF_SIZE;

			d_stream->avail_out = dst - d_stream->next_out;
			switch (inflate(d_stream, Z_SYNC_FLUSH)) {
			case Z_STREAM_END:
			case Z_OK:
			case Z_BUF_ERROR:
				if (!d_stream->avail_out) {
					break;
				}
				/* fall through */
			default:
				page_zip_fail(("page_zip_decompress_clust_ext:"
					       " 2 inflate(Z_SYNC_FLUSH)=%s\n",
					       d_stream->msg));
				return(FALSE);
			}

			ut_ad(d_stream->next_out == dst);

			/* Clear the BLOB pointer in case
			the record will be deleted and the
			space will not be reused.  Note that
			the final initialization of the BLOB
			pointers (copying from "externs"
			or clearing) will have to take place
			only after the page modification log
			has been applied.  Otherwise, we
			could end up with an uninitialized
			BLOB pointer when a record is deleted,
			reallocated and deleted. */
			memset(d_stream->next_out, 0,
			       BTR_EXTERN_FIELD_REF_SIZE);
			d_stream->next_out
				+= BTR_EXTERN_FIELD_REF_SIZE;
		}
	}

	return(TRUE);
}

/**********************************************************************//**
Compress the records of a leaf node of a clustered index.
@return	TRUE on success, FALSE on failure */
static
ibool
page_zip_decompress_clust(
/*======================*/
	page_zip_des_t*	page_zip,	/*!< in/out: compressed page */
	z_stream*	d_stream,	/*!< in/out: compressed page stream */
	rec_t**		recs,		/*!< in: dense page directory
					sorted by address */
	ulint		n_dense,	/*!< in: size of recs[] */
	dict_index_t*	index,		/*!< in: the index of the page */
	ulint		trx_id_col,	/*!< index of the trx_id column */
	ulint*		offsets,	/*!< in/out: temporary offsets */
	mem_heap_t*	heap)		/*!< in: temporary memory heap */
{
	int		err;
	ulint		slot;
	ulint		heap_status	= REC_STATUS_ORDINARY
		| PAGE_HEAP_NO_USER_LOW << REC_HEAP_NO_SHIFT;
	const byte*	storage;
	const byte*	externs;

	ut_a(dict_index_is_clust(index));

	/* Subtract the space reserved for uncompressed data. */
	d_stream->avail_in -= n_dense * (PAGE_ZIP_DIR_SLOT_SIZE
					 + DATA_TRX_ID_LEN
					 + DATA_ROLL_PTR_LEN);

	/* Decompress the records in heap_no order. */
	for (slot = 0; slot < n_dense; slot++) {
		rec_t*	rec	= recs[slot];

		d_stream->avail_out = rec - REC_N_NEW_EXTRA_BYTES
			- d_stream->next_out;

		ut_ad(d_stream->avail_out < UNIV_PAGE_SIZE
		      - PAGE_ZIP_START - PAGE_DIR);
		err = inflate(d_stream, Z_SYNC_FLUSH);
		switch (err) {
		case Z_STREAM_END:
			/* Apparently, n_dense has grown
			since the time the page was last compressed. */
			goto zlib_done;
		case Z_OK:
		case Z_BUF_ERROR:
			if (UNIV_LIKELY(!d_stream->avail_out)) {
				break;
			}
			/* fall through */
		default:
			page_zip_fail(("page_zip_decompress_clust:"
				       " 1 inflate(Z_SYNC_FLUSH)=%s\n",
				       d_stream->msg));
			goto zlib_error;
		}

		ut_ad(d_stream->next_out == rec - REC_N_NEW_EXTRA_BYTES);
		/* Prepare to decompress the data bytes. */
		d_stream->next_out = rec;
		/* Set heap_no and the status bits. */
		mach_write_to_2(rec - REC_NEW_HEAP_NO, heap_status);
		heap_status += 1 << REC_HEAP_NO_SHIFT;

		/* Read the offsets. The status bits are needed here. */
		offsets = rec_get_offsets(rec, index, offsets,
					  ULINT_UNDEFINED, &heap);

		/* This is a leaf page in a clustered index. */

		/* Check if there are any externally stored columns.
		For each externally stored column, restore the
		BTR_EXTERN_FIELD_REF separately. */

		if (rec_offs_any_extern(offsets)) {
			if (UNIV_UNLIKELY
			    (!page_zip_decompress_clust_ext(
				    d_stream, rec, offsets, trx_id_col))) {

				goto zlib_error;
			}
		} else {
			/* Skip trx_id and roll_ptr */
			ulint	len;
			byte*	dst = rec_get_nth_field(rec, offsets,
							trx_id_col, &len);
			if (UNIV_UNLIKELY(len < DATA_TRX_ID_LEN
					  + DATA_ROLL_PTR_LEN)) {

				page_zip_fail(("page_zip_decompress_clust:"
					       " len = %lu\n", (ulong) len));
				goto zlib_error;
			}

			d_stream->avail_out = dst - d_stream->next_out;

			switch (inflate(d_stream, Z_SYNC_FLUSH)) {
			case Z_STREAM_END:
			case Z_OK:
			case Z_BUF_ERROR:
				if (!d_stream->avail_out) {
					break;
				}
				/* fall through */
			default:
				page_zip_fail(("page_zip_decompress_clust:"
					       " 2 inflate(Z_SYNC_FLUSH)=%s\n",
					       d_stream->msg));
				goto zlib_error;
			}

			ut_ad(d_stream->next_out == dst);

			/* Clear DB_TRX_ID and DB_ROLL_PTR in order to
			avoid uninitialized bytes in case the record
			is affected by page_zip_apply_log(). */
			memset(dst, 0, DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN);

			d_stream->next_out += DATA_TRX_ID_LEN
				+ DATA_ROLL_PTR_LEN;
		}

		/* Decompress the last bytes of the record. */
		d_stream->avail_out = rec_get_end(rec, offsets)
			- d_stream->next_out;

		switch (inflate(d_stream, Z_SYNC_FLUSH)) {
		case Z_STREAM_END:
		case Z_OK:
		case Z_BUF_ERROR:
			if (!d_stream->avail_out) {
				break;
			}
			/* fall through */
		default:
			page_zip_fail(("page_zip_decompress_clust:"
				       " 3 inflate(Z_SYNC_FLUSH)=%s\n",
				       d_stream->msg));
			goto zlib_error;
		}
	}

	/* Decompress any trailing garbage, in case the last record was
	allocated from an originally longer space on the free list. */
	d_stream->avail_out = page_header_get_field(page_zip->data,
						    PAGE_HEAP_TOP)
		- page_offset(d_stream->next_out);
	if (UNIV_UNLIKELY(d_stream->avail_out > UNIV_PAGE_SIZE
			  - PAGE_ZIP_START - PAGE_DIR)) {

		page_zip_fail(("page_zip_decompress_clust:"
			       " avail_out = %u\n",
			       d_stream->avail_out));
		goto zlib_error;
	}

	if (UNIV_UNLIKELY(inflate(d_stream, Z_FINISH) != Z_STREAM_END)) {
		page_zip_fail(("page_zip_decompress_clust:"
			       " inflate(Z_FINISH)=%s\n",
			       d_stream->msg));
zlib_error:
		inflateEnd(d_stream);
		return(FALSE);
	}

	/* Note that d_stream->avail_out > 0 may hold here
	if the modification log is nonempty. */

zlib_done:
	if (UNIV_UNLIKELY(inflateEnd(d_stream) != Z_OK)) {
		ut_error;
	}

	{
		page_t*	page = page_align(d_stream->next_out);

		/* Clear the unused heap space on the uncompressed page. */
		memset(d_stream->next_out, 0,
		       page_dir_get_nth_slot(page,
					     page_dir_get_n_slots(page) - 1)
		       - d_stream->next_out);
	}

#ifdef UNIV_DEBUG
	page_zip->m_start = PAGE_DATA + d_stream->total_in;
#endif /* UNIV_DEBUG */

	/* Apply the modification log. */
	{
		const byte*	mod_log_ptr;
		mod_log_ptr = page_zip_apply_log(d_stream->next_in,
						 d_stream->avail_in + 1,
						 recs, n_dense,
						 trx_id_col, heap_status,
						 index, offsets);

		if (UNIV_UNLIKELY(!mod_log_ptr)) {
			return(FALSE);
		}
		page_zip->m_end = mod_log_ptr - page_zip->data;
		page_zip->m_nonempty = mod_log_ptr != d_stream->next_in;
	}

	if (UNIV_UNLIKELY(page_zip_get_trailer_len(page_zip, TRUE, NULL)
			  + page_zip->m_end >= page_zip_get_size(page_zip))) {

		page_zip_fail(("page_zip_decompress_clust: %lu + %lu >= %lu\n",
			       (ulong) page_zip_get_trailer_len(
				       page_zip, TRUE, NULL),
			       (ulong) page_zip->m_end,
			       (ulong) page_zip_get_size(page_zip)));
		return(FALSE);
	}

	storage = page_zip_dir_start_low(page_zip, n_dense);

	externs = storage - n_dense
		* (DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN);

	/* Restore the uncompressed columns in heap_no order. */

	for (slot = 0; slot < n_dense; slot++) {
		ulint	i;
		ulint	len;
		byte*	dst;
		rec_t*	rec	= recs[slot];
		ibool	exists	= !page_zip_dir_find_free(
			page_zip, page_offset(rec));
		offsets = rec_get_offsets(rec, index, offsets,
					  ULINT_UNDEFINED, &heap);

		dst = rec_get_nth_field(rec, offsets,
					trx_id_col, &len);
		ut_ad(len >= DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN);
		storage -= DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN;
		memcpy(dst, storage,
		       DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN);

		/* Check if there are any externally stored
		columns in this record.  For each externally
		stored column, restore or clear the
		BTR_EXTERN_FIELD_REF. */
		if (!rec_offs_any_extern(offsets)) {
			continue;
		}

		for (i = 0; i < rec_offs_n_fields(offsets); i++) {
			if (!rec_offs_nth_extern(offsets, i)) {
				continue;
			}
			dst = rec_get_nth_field(rec, offsets, i, &len);

			if (UNIV_UNLIKELY(len < BTR_EXTERN_FIELD_REF_SIZE)) {
				page_zip_fail(("page_zip_decompress_clust:"
					       " %lu < 20\n",
					       (ulong) len));
				return(FALSE);
			}

			dst += len - BTR_EXTERN_FIELD_REF_SIZE;

			if (UNIV_LIKELY(exists)) {
				/* Existing record:
				restore the BLOB pointer */
				externs -= BTR_EXTERN_FIELD_REF_SIZE;

				if (UNIV_UNLIKELY
				    (externs < page_zip->data
				     + page_zip->m_end)) {
					page_zip_fail(("page_zip_"
						       "decompress_clust: "
						       "%p < %p + %lu\n",
						       (const void*) externs,
						       (const void*)
						       page_zip->data,
						       (ulong)
						       page_zip->m_end));
					return(FALSE);
				}

				memcpy(dst, externs,
				       BTR_EXTERN_FIELD_REF_SIZE);

				page_zip->n_blobs++;
			} else {
				/* Deleted record:
				clear the BLOB pointer */
				memset(dst, 0,
				       BTR_EXTERN_FIELD_REF_SIZE);
			}
		}
	}

	return(TRUE);
}

/**********************************************************************//**
Decompress a page.  This function should tolerate errors on the compressed
page.  Instead of letting assertions fail, it will return FALSE if an
inconsistency is detected.
@return	TRUE on success, FALSE on failure */
UNIV_INTERN
ibool
page_zip_decompress(
/*================*/
	page_zip_des_t*	page_zip,/*!< in: data, ssize;
				out: m_start, m_end, m_nonempty, n_blobs */
	page_t*		page,	/*!< out: uncompressed page, may be trashed */
	ibool		all)	/*!< in: TRUE=decompress the whole page;
				FALSE=verify but do not copy some
				page header fields that should not change
				after page creation */
{
	z_stream	d_stream;
	dict_index_t*	index	= NULL;
	rec_t**		recs;	/*!< dense page directory, sorted by address */
	ulint		n_dense;/* number of user records on the page */
	ulint		trx_id_col = ULINT_UNDEFINED;
	mem_heap_t*	heap;
	ulint*		offsets;
#ifndef UNIV_HOTBACKUP
	ullint		usec = ut_time_us(NULL);
#endif /* !UNIV_HOTBACKUP */

	ut_ad(page_zip_simple_validate(page_zip));
	UNIV_MEM_ASSERT_W(page, UNIV_PAGE_SIZE);
	UNIV_MEM_ASSERT_RW(page_zip->data, page_zip_get_size(page_zip));

	/* The dense directory excludes the infimum and supremum records. */
	n_dense = page_dir_get_n_heap(page_zip->data) - PAGE_HEAP_NO_USER_LOW;
	if (UNIV_UNLIKELY(n_dense * PAGE_ZIP_DIR_SLOT_SIZE
			  >= page_zip_get_size(page_zip))) {
		page_zip_fail(("page_zip_decompress 1: %lu %lu\n",
			       (ulong) n_dense,
			       (ulong) page_zip_get_size(page_zip)));
		return(FALSE);
	}

	heap = mem_heap_create(n_dense * (3 * sizeof *recs) + UNIV_PAGE_SIZE);

	recs = static_cast<rec_t**>(
		mem_heap_alloc(heap, n_dense * (2 * sizeof *recs)));

	if (all) {
		/* Copy the page header. */
		memcpy(page, page_zip->data, PAGE_DATA);
	} else {
		/* Check that the bytes that we skip are identical. */
#if defined UNIV_DEBUG || defined UNIV_ZIP_DEBUG
		ut_a(!memcmp(FIL_PAGE_TYPE + page,
			     FIL_PAGE_TYPE + page_zip->data,
			     PAGE_HEADER - FIL_PAGE_TYPE));
		ut_a(!memcmp(PAGE_HEADER + PAGE_LEVEL + page,
			     PAGE_HEADER + PAGE_LEVEL + page_zip->data,
			     PAGE_DATA - (PAGE_HEADER + PAGE_LEVEL)));
#endif /* UNIV_DEBUG || UNIV_ZIP_DEBUG */

		/* Copy the mutable parts of the page header. */
		memcpy(page, page_zip->data, FIL_PAGE_TYPE);
		memcpy(PAGE_HEADER + page, PAGE_HEADER + page_zip->data,
		       PAGE_LEVEL - PAGE_N_DIR_SLOTS);

#if defined UNIV_DEBUG || defined UNIV_ZIP_DEBUG
		/* Check that the page headers match after copying. */
		ut_a(!memcmp(page, page_zip->data, PAGE_DATA));
#endif /* UNIV_DEBUG || UNIV_ZIP_DEBUG */
	}

#ifdef UNIV_ZIP_DEBUG
	/* Clear the uncompressed page, except the header. */
	memset(PAGE_DATA + page, 0x55, UNIV_PAGE_SIZE - PAGE_DATA);
#endif /* UNIV_ZIP_DEBUG */
	UNIV_MEM_INVALID(PAGE_DATA + page, UNIV_PAGE_SIZE - PAGE_DATA);

	/* Copy the page directory. */
	if (UNIV_UNLIKELY(!page_zip_dir_decode(page_zip, page, recs,
					       recs + n_dense, n_dense))) {
zlib_error:
		mem_heap_free(heap);
		return(FALSE);
	}

	/* Copy the infimum and supremum records. */
	memcpy(page + (PAGE_NEW_INFIMUM - REC_N_NEW_EXTRA_BYTES),
	       infimum_extra, sizeof infimum_extra);
	if (UNIV_UNLIKELY(!page_get_n_recs(page))) {
		rec_set_next_offs_new(page + PAGE_NEW_INFIMUM,
				      PAGE_NEW_SUPREMUM);
	} else {
		rec_set_next_offs_new(page + PAGE_NEW_INFIMUM,
				      page_zip_dir_get(page_zip, 0)
				      & PAGE_ZIP_DIR_SLOT_MASK);
	}
	memcpy(page + PAGE_NEW_INFIMUM, infimum_data, sizeof infimum_data);
	memcpy(page + (PAGE_NEW_SUPREMUM - REC_N_NEW_EXTRA_BYTES + 1),
	       supremum_extra_data, sizeof supremum_extra_data);

	page_zip_set_alloc(&d_stream, heap);

	d_stream.next_in = page_zip->data + PAGE_DATA;
	/* Subtract the space reserved for
	the page header and the end marker of the modification log. */
	d_stream.avail_in = page_zip_get_size(page_zip) - (PAGE_DATA + 1);
	d_stream.next_out = page + PAGE_ZIP_START;
	d_stream.avail_out = UNIV_PAGE_SIZE - PAGE_ZIP_START;

	if (UNIV_UNLIKELY(inflateInit2(&d_stream, UNIV_PAGE_SIZE_SHIFT)
			  != Z_OK)) {
		ut_error;
	}

	/* Decode the zlib header and the index information. */
	if (UNIV_UNLIKELY(inflate(&d_stream, Z_BLOCK) != Z_OK)) {

		page_zip_fail(("page_zip_decompress:"
			       " 1 inflate(Z_BLOCK)=%s\n", d_stream.msg));
		goto zlib_error;
	}

	if (UNIV_UNLIKELY(inflate(&d_stream, Z_BLOCK) != Z_OK)) {

		page_zip_fail(("page_zip_decompress:"
			       " 2 inflate(Z_BLOCK)=%s\n", d_stream.msg));
		goto zlib_error;
	}

	index = page_zip_fields_decode(
		page + PAGE_ZIP_START, d_stream.next_out,
		page_is_leaf(page) ? &trx_id_col : NULL);

	if (UNIV_UNLIKELY(!index)) {

		goto zlib_error;
	}

	/* Decompress the user records. */
	page_zip->n_blobs = 0;
	d_stream.next_out = page + PAGE_ZIP_START;

	{
		/* Pre-allocate the offsets for rec_get_offsets_reverse(). */
		ulint	n = 1 + 1/* node ptr */ + REC_OFFS_HEADER_SIZE
			+ dict_index_get_n_fields(index);

		offsets = static_cast<ulint*>(
			mem_heap_alloc(heap, n * sizeof(ulint)));

		*offsets = n;
	}

	/* Decompress the records in heap_no order. */
	if (!page_is_leaf(page)) {
		/* This is a node pointer page. */
		ulint	info_bits;

		if (UNIV_UNLIKELY
		    (!page_zip_decompress_node_ptrs(page_zip, &d_stream,
						    recs, n_dense, index,
						    offsets, heap))) {
			goto err_exit;
		}

		info_bits = mach_read_from_4(page + FIL_PAGE_PREV) == FIL_NULL
			? REC_INFO_MIN_REC_FLAG : 0;

		if (UNIV_UNLIKELY(!page_zip_set_extra_bytes(page_zip, page,
							    info_bits))) {
			goto err_exit;
		}
	} else if (UNIV_LIKELY(trx_id_col == ULINT_UNDEFINED)) {
		/* This is a leaf page in a secondary index. */
		if (UNIV_UNLIKELY(!page_zip_decompress_sec(page_zip, &d_stream,
							   recs, n_dense,
							   index, offsets))) {
			goto err_exit;
		}

		if (UNIV_UNLIKELY(!page_zip_set_extra_bytes(page_zip,
							    page, 0))) {
err_exit:
			page_zip_fields_free(index);
			mem_heap_free(heap);
			return(FALSE);
		}
	} else {
		/* This is a leaf page in a clustered index. */
		if (UNIV_UNLIKELY(!page_zip_decompress_clust(page_zip,
							     &d_stream, recs,
							     n_dense, index,
							     trx_id_col,
							     offsets, heap))) {
			goto err_exit;
		}

		if (UNIV_UNLIKELY(!page_zip_set_extra_bytes(page_zip,
							    page, 0))) {
			goto err_exit;
		}
	}

	ut_a(page_is_comp(page));
	UNIV_MEM_ASSERT_RW(page, UNIV_PAGE_SIZE);

	page_zip_fields_free(index);
	mem_heap_free(heap);
#ifndef UNIV_HOTBACKUP
	ullint	time_diff = ut_time_us(NULL) - usec;
	page_zip_stat[page_zip->ssize - 1].decompressed++;
	page_zip_stat[page_zip->ssize - 1].decompressed_usec += time_diff;

	index_id_t	index_id = btr_page_get_index_id(page);

	if (srv_cmp_per_index_enabled) {
		mutex_enter(&page_zip_stat_per_index_mutex);
		page_zip_stat_per_index[index_id].decompressed++;
		page_zip_stat_per_index[index_id].decompressed_usec += time_diff;
		mutex_exit(&page_zip_stat_per_index_mutex);
	}
#endif /* !UNIV_HOTBACKUP */

	/* Update the stat counter for LRU policy. */
	buf_LRU_stat_inc_unzip();

	MONITOR_INC(MONITOR_PAGE_DECOMPRESS);

	return(TRUE);
}

#ifdef UNIV_ZIP_DEBUG
/**********************************************************************//**
Dump a block of memory on the standard error stream. */
static
void
page_zip_hexdump_func(
/*==================*/
	const char*	name,	/*!< in: name of the data structure */
	const void*	buf,	/*!< in: data */
	ulint		size)	/*!< in: length of the data, in bytes */
{
	const byte*	s	= static_cast<const byte*>(buf);
	ulint		addr;
	const ulint	width	= 32; /* bytes per line */

	fprintf(stderr, "%s:\n", name);

	for (addr = 0; addr < size; addr += width) {
		ulint	i;

		fprintf(stderr, "%04lx ", (ulong) addr);

		i = ut_min(width, size - addr);

		while (i--) {
			fprintf(stderr, "%02x", *s++);
		}

		putc('\n', stderr);
	}
}

/** Dump a block of memory on the standard error stream.
@param buf	in: data
@param size	in: length of the data, in bytes */
#define page_zip_hexdump(buf, size) page_zip_hexdump_func(#buf, buf, size)

/** Flag: make page_zip_validate() compare page headers only */
UNIV_INTERN ibool	page_zip_validate_header_only = FALSE;

/**********************************************************************//**
Check that the compressed and decompressed pages match.
@return	TRUE if valid, FALSE if not */
UNIV_INTERN
ibool
page_zip_validate_low(
/*==================*/
	const page_zip_des_t*	page_zip,/*!< in: compressed page */
	const page_t*		page,	/*!< in: uncompressed page */
	const dict_index_t*	index,	/*!< in: index of the page, if known */
	ibool			sloppy)	/*!< in: FALSE=strict,
					TRUE=ignore the MIN_REC_FLAG */
{
	page_zip_des_t	temp_page_zip;
	byte*		temp_page_buf;
	page_t*		temp_page;
	ibool		valid;

	if (memcmp(page_zip->data + FIL_PAGE_PREV, page + FIL_PAGE_PREV,
		   FIL_PAGE_LSN - FIL_PAGE_PREV)
	    || memcmp(page_zip->data + FIL_PAGE_TYPE, page + FIL_PAGE_TYPE, 2)
	    || memcmp(page_zip->data + FIL_PAGE_DATA, page + FIL_PAGE_DATA,
		      PAGE_DATA - FIL_PAGE_DATA)) {
		page_zip_fail(("page_zip_validate: page header\n"));
		page_zip_hexdump(page_zip, sizeof *page_zip);
		page_zip_hexdump(page_zip->data, page_zip_get_size(page_zip));
		page_zip_hexdump(page, UNIV_PAGE_SIZE);
		return(FALSE);
	}

	ut_a(page_is_comp(page));

	if (page_zip_validate_header_only) {
		return(TRUE);
	}

	/* page_zip_decompress() expects the uncompressed page to be
	UNIV_PAGE_SIZE aligned. */
	temp_page_buf = static_cast<byte*>(ut_malloc(2 * UNIV_PAGE_SIZE));
	temp_page = static_cast<byte*>(ut_align(temp_page_buf, UNIV_PAGE_SIZE));

#ifdef UNIV_DEBUG_VALGRIND
	/* Get detailed information on the valid bits in case the
	UNIV_MEM_ASSERT_RW() checks fail.  The v-bits of page[],
	page_zip->data[] or page_zip could be viewed at temp_page[] or
	temp_page_zip in a debugger when running valgrind --db-attach. */
	(void) VALGRIND_GET_VBITS(page, temp_page, UNIV_PAGE_SIZE);
	UNIV_MEM_ASSERT_RW(page, UNIV_PAGE_SIZE);
# if UNIV_WORD_SIZE == 4
	VALGRIND_GET_VBITS(page_zip, &temp_page_zip, sizeof temp_page_zip);
	/* On 32-bit systems, there is no padding in page_zip_des_t.
	On other systems, Valgrind could complain about uninitialized
	pad bytes. */
	UNIV_MEM_ASSERT_RW(page_zip, sizeof *page_zip);
# endif
	(void) VALGRIND_GET_VBITS(page_zip->data, temp_page,
				  page_zip_get_size(page_zip));
	UNIV_MEM_ASSERT_RW(page_zip->data, page_zip_get_size(page_zip));
#endif /* UNIV_DEBUG_VALGRIND */

	temp_page_zip = *page_zip;
	valid = page_zip_decompress(&temp_page_zip, temp_page, TRUE);
	if (!valid) {
		fputs("page_zip_validate(): failed to decompress\n", stderr);
		goto func_exit;
	}
	if (page_zip->n_blobs != temp_page_zip.n_blobs) {
		page_zip_fail(("page_zip_validate: n_blobs: %u!=%u\n",
			       page_zip->n_blobs, temp_page_zip.n_blobs));
		valid = FALSE;
	}
#ifdef UNIV_DEBUG
	if (page_zip->m_start != temp_page_zip.m_start) {
		page_zip_fail(("page_zip_validate: m_start: %u!=%u\n",
			       page_zip->m_start, temp_page_zip.m_start));
		valid = FALSE;
	}
#endif /* UNIV_DEBUG */
	if (page_zip->m_end != temp_page_zip.m_end) {
		page_zip_fail(("page_zip_validate: m_end: %u!=%u\n",
			       page_zip->m_end, temp_page_zip.m_end));
		valid = FALSE;
	}
	if (page_zip->m_nonempty != temp_page_zip.m_nonempty) {
		page_zip_fail(("page_zip_validate(): m_nonempty: %u!=%u\n",
			       page_zip->m_nonempty,
			       temp_page_zip.m_nonempty));
		valid = FALSE;
	}
	if (memcmp(page + PAGE_HEADER, temp_page + PAGE_HEADER,
		   UNIV_PAGE_SIZE - PAGE_HEADER - FIL_PAGE_DATA_END)) {

		/* In crash recovery, the "minimum record" flag may be
		set incorrectly until the mini-transaction is
		committed.  Let us tolerate that difference when we
		are performing a sloppy validation. */

		ulint*		offsets;
		mem_heap_t*	heap;
		const rec_t*	rec;
		const rec_t*	trec;
		byte		info_bits_diff;
		ulint		offset
			= rec_get_next_offs(page + PAGE_NEW_INFIMUM, TRUE);
		ut_a(offset >= PAGE_NEW_SUPREMUM);
		offset -= 5/*REC_NEW_INFO_BITS*/;

		info_bits_diff = page[offset] ^ temp_page[offset];

		if (info_bits_diff == REC_INFO_MIN_REC_FLAG) {
			temp_page[offset] = page[offset];

			if (!memcmp(page + PAGE_HEADER,
				    temp_page + PAGE_HEADER,
				    UNIV_PAGE_SIZE - PAGE_HEADER
				    - FIL_PAGE_DATA_END)) {

				/* Only the minimum record flag
				differed.  Let us ignore it. */
				page_zip_fail(("page_zip_validate: "
					       "min_rec_flag "
					       "(%s"
					       "%lu,%lu,0x%02lx)\n",
					       sloppy ? "ignored, " : "",
					       page_get_space_id(page),
					       page_get_page_no(page),
					       (ulong) page[offset]));
				valid = sloppy;
				goto func_exit;
			}
		}

		/* Compare the pointers in the PAGE_FREE list. */
		rec = page_header_get_ptr(page, PAGE_FREE);
		trec = page_header_get_ptr(temp_page, PAGE_FREE);

		while (rec || trec) {
			if (page_offset(rec) != page_offset(trec)) {
				page_zip_fail(("page_zip_validate: "
					       "PAGE_FREE list: %u!=%u\n",
					       (unsigned) page_offset(rec),
					       (unsigned) page_offset(trec)));
				valid = FALSE;
				goto func_exit;
			}

			rec = page_rec_get_next_low(rec, TRUE);
			trec = page_rec_get_next_low(trec, TRUE);
		}

		/* Compare the records. */
		heap = NULL;
		offsets = NULL;
		rec = page_rec_get_next_low(
			page + PAGE_NEW_INFIMUM, TRUE);
		trec = page_rec_get_next_low(
			temp_page + PAGE_NEW_INFIMUM, TRUE);

		do {
			if (page_offset(rec) != page_offset(trec)) {
				page_zip_fail(("page_zip_validate: "
					       "record list: 0x%02x!=0x%02x\n",
					       (unsigned) page_offset(rec),
					       (unsigned) page_offset(trec)));
				valid = FALSE;
				break;
			}

			if (index) {
				/* Compare the data. */
				offsets = rec_get_offsets(
					rec, index, offsets,
					ULINT_UNDEFINED, &heap);

				if (memcmp(rec - rec_offs_extra_size(offsets),
					   trec - rec_offs_extra_size(offsets),
					   rec_offs_size(offsets))) {
					page_zip_fail(
						("page_zip_validate: "
						 "record content: 0x%02x",
						 (unsigned) page_offset(rec)));
					valid = FALSE;
					break;
				}
			}

			rec = page_rec_get_next_low(rec, TRUE);
			trec = page_rec_get_next_low(trec, TRUE);
		} while (rec || trec);

		if (heap) {
			mem_heap_free(heap);
		}
	}

func_exit:
	if (!valid) {
		page_zip_hexdump(page_zip, sizeof *page_zip);
		page_zip_hexdump(page_zip->data, page_zip_get_size(page_zip));
		page_zip_hexdump(page, UNIV_PAGE_SIZE);
		page_zip_hexdump(temp_page, UNIV_PAGE_SIZE);
	}
	ut_free(temp_page_buf);
	return(valid);
}

/**********************************************************************//**
Check that the compressed and decompressed pages match.
@return	TRUE if valid, FALSE if not */
UNIV_INTERN
ibool
page_zip_validate(
/*==============*/
	const page_zip_des_t*	page_zip,/*!< in: compressed page */
	const page_t*		page,	/*!< in: uncompressed page */
	const dict_index_t*	index)	/*!< in: index of the page, if known */
{
	return(page_zip_validate_low(page_zip, page, index,
				     recv_recovery_is_on()));
}
#endif /* UNIV_ZIP_DEBUG */

#ifdef UNIV_DEBUG
/**********************************************************************//**
Assert that the compressed and decompressed page headers match.
@return	TRUE */
static
ibool
page_zip_header_cmp(
/*================*/
	const page_zip_des_t*	page_zip,/*!< in: compressed page */
	const byte*		page)	/*!< in: uncompressed page */
{
	ut_ad(!memcmp(page_zip->data + FIL_PAGE_PREV, page + FIL_PAGE_PREV,
		      FIL_PAGE_LSN - FIL_PAGE_PREV));
	ut_ad(!memcmp(page_zip->data + FIL_PAGE_TYPE, page + FIL_PAGE_TYPE,
		      2));
	ut_ad(!memcmp(page_zip->data + FIL_PAGE_DATA, page + FIL_PAGE_DATA,
		      PAGE_DATA - FIL_PAGE_DATA));

	return(TRUE);
}
#endif /* UNIV_DEBUG */

/**********************************************************************//**
Write a record on the compressed page that contains externally stored
columns.  The data must already have been written to the uncompressed page.
@return	end of modification log */
static
byte*
page_zip_write_rec_ext(
/*===================*/
	page_zip_des_t*	page_zip,	/*!< in/out: compressed page */
	const page_t*	page,		/*!< in: page containing rec */
	const byte*	rec,		/*!< in: record being written */
	dict_index_t*	index,		/*!< in: record descriptor */
	const ulint*	offsets,	/*!< in: rec_get_offsets(rec, index) */
	ulint		create,		/*!< in: nonzero=insert, zero=update */
	ulint		trx_id_col,	/*!< in: position of DB_TRX_ID */
	ulint		heap_no,	/*!< in: heap number of rec */
	byte*		storage,	/*!< in: end of dense page directory */
	byte*		data)		/*!< in: end of modification log */
{
	const byte*	start	= rec;
	ulint		i;
	ulint		len;
	byte*		externs	= storage;
	ulint		n_ext	= rec_offs_n_extern(offsets);

	ut_ad(rec_offs_validate(rec, index, offsets));
	UNIV_MEM_ASSERT_RW(rec, rec_offs_data_size(offsets));
	UNIV_MEM_ASSERT_RW(rec - rec_offs_extra_size(offsets),
			   rec_offs_extra_size(offsets));

	externs -= (DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN)
		* (page_dir_get_n_heap(page) - PAGE_HEAP_NO_USER_LOW);

	/* Note that this will not take into account
	the BLOB columns of rec if create==TRUE. */
	ut_ad(data + rec_offs_data_size(offsets)
	      - (DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN)
	      - n_ext * BTR_EXTERN_FIELD_REF_SIZE
	      < externs - BTR_EXTERN_FIELD_REF_SIZE * page_zip->n_blobs);

	{
		ulint	blob_no = page_zip_get_n_prev_extern(
			page_zip, rec, index);
		byte*	ext_end = externs - page_zip->n_blobs
			* BTR_EXTERN_FIELD_REF_SIZE;
		ut_ad(blob_no <= page_zip->n_blobs);
		externs -= blob_no * BTR_EXTERN_FIELD_REF_SIZE;

		if (create) {
			page_zip->n_blobs += n_ext;
			ASSERT_ZERO_BLOB(ext_end - n_ext
					 * BTR_EXTERN_FIELD_REF_SIZE);
			memmove(ext_end - n_ext
				* BTR_EXTERN_FIELD_REF_SIZE,
				ext_end,
				externs - ext_end);
		}

		ut_a(blob_no + n_ext <= page_zip->n_blobs);
	}

	for (i = 0; i < rec_offs_n_fields(offsets); i++) {
		const byte*	src;

		if (UNIV_UNLIKELY(i == trx_id_col)) {
			ut_ad(!rec_offs_nth_extern(offsets,
						   i));
			ut_ad(!rec_offs_nth_extern(offsets,
						   i + 1));
			/* Locate trx_id and roll_ptr. */
			src = rec_get_nth_field(rec, offsets,
						i, &len);
			ut_ad(len == DATA_TRX_ID_LEN);
			ut_ad(src + DATA_TRX_ID_LEN
			      == rec_get_nth_field(
				      rec, offsets,
				      i + 1, &len));
			ut_ad(len == DATA_ROLL_PTR_LEN);

			/* Log the preceding fields. */
			ASSERT_ZERO(data, src - start);
			memcpy(data, start, src - start);
			data += src - start;
			start = src + (DATA_TRX_ID_LEN
				       + DATA_ROLL_PTR_LEN);

			/* Store trx_id and roll_ptr. */
			memcpy(storage - (DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN)
			       * (heap_no - 1),
			       src, DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN);
			i++; /* skip also roll_ptr */
		} else if (rec_offs_nth_extern(offsets, i)) {
			src = rec_get_nth_field(rec, offsets,
						i, &len);

			ut_ad(dict_index_is_clust(index));
			ut_ad(len
			      >= BTR_EXTERN_FIELD_REF_SIZE);
			src += len - BTR_EXTERN_FIELD_REF_SIZE;

			ASSERT_ZERO(data, src - start);
			memcpy(data, start, src - start);
			data += src - start;
			start = src + BTR_EXTERN_FIELD_REF_SIZE;

			/* Store the BLOB pointer. */
			externs -= BTR_EXTERN_FIELD_REF_SIZE;
			ut_ad(data < externs);
			memcpy(externs, src, BTR_EXTERN_FIELD_REF_SIZE);
		}
	}

	/* Log the last bytes of the record. */
	len = rec_offs_data_size(offsets) - (start - rec);

	ASSERT_ZERO(data, len);
	memcpy(data, start, len);
	data += len;

	return(data);
}

/**********************************************************************//**
Write an entire record on the compressed page.  The data must already
have been written to the uncompressed page. */
UNIV_INTERN
void
page_zip_write_rec(
/*===============*/
	page_zip_des_t*	page_zip,/*!< in/out: compressed page */
	const byte*	rec,	/*!< in: record being written */
	dict_index_t*	index,	/*!< in: the index the record belongs to */
	const ulint*	offsets,/*!< in: rec_get_offsets(rec, index) */
	ulint		create)	/*!< in: nonzero=insert, zero=update */
{
	const page_t*	page;
	byte*		data;
	byte*		storage;
	ulint		heap_no;
	byte*		slot;

	ut_ad(PAGE_ZIP_MATCH(rec, page_zip));
	ut_ad(page_zip_simple_validate(page_zip));
	ut_ad(page_zip_get_size(page_zip)
	      > PAGE_DATA + page_zip_dir_size(page_zip));
	ut_ad(rec_offs_comp(offsets));
	ut_ad(rec_offs_validate(rec, index, offsets));

	ut_ad(page_zip->m_start >= PAGE_DATA);

	page = page_align(rec);

	ut_ad(page_zip_header_cmp(page_zip, page));
	ut_ad(page_simple_validate_new((page_t*) page));

	UNIV_MEM_ASSERT_RW(page_zip->data, page_zip_get_size(page_zip));
	UNIV_MEM_ASSERT_RW(rec, rec_offs_data_size(offsets));
	UNIV_MEM_ASSERT_RW(rec - rec_offs_extra_size(offsets),
			   rec_offs_extra_size(offsets));

	slot = page_zip_dir_find(page_zip, page_offset(rec));
	ut_a(slot);
	/* Copy the delete mark. */
	if (rec_get_deleted_flag(rec, TRUE)) {
		*slot |= PAGE_ZIP_DIR_SLOT_DEL >> 8;
	} else {
		*slot &= ~(PAGE_ZIP_DIR_SLOT_DEL >> 8);
	}

	ut_ad(rec_get_start((rec_t*) rec, offsets) >= page + PAGE_ZIP_START);
	ut_ad(rec_get_end((rec_t*) rec, offsets) <= page + UNIV_PAGE_SIZE
	      - PAGE_DIR - PAGE_DIR_SLOT_SIZE
	      * page_dir_get_n_slots(page));

	heap_no = rec_get_heap_no_new(rec);
	ut_ad(heap_no >= PAGE_HEAP_NO_USER_LOW); /* not infimum or supremum */
	ut_ad(heap_no < page_dir_get_n_heap(page));

	/* Append to the modification log. */
	data = page_zip->data + page_zip->m_end;
	ut_ad(!*data);

	/* Identify the record by writing its heap number - 1.
	0 is reserved to indicate the end of the modification log. */

	if (UNIV_UNLIKELY(heap_no - 1 >= 64)) {
		*data++ = (byte) (0x80 | (heap_no - 1) >> 7);
		ut_ad(!*data);
	}
	*data++ = (byte) ((heap_no - 1) << 1);
	ut_ad(!*data);

	{
		const byte*	start	= rec - rec_offs_extra_size(offsets);
		const byte*	b	= rec - REC_N_NEW_EXTRA_BYTES;

		/* Write the extra bytes backwards, so that
		rec_offs_extra_size() can be easily computed in
		page_zip_apply_log() by invoking
		rec_get_offsets_reverse(). */

		while (b != start) {
			*data++ = *--b;
			ut_ad(!*data);
		}
	}

	/* Write the data bytes.  Store the uncompressed bytes separately. */
	storage = page_zip_dir_start(page_zip);

	if (page_is_leaf(page)) {
		ulint		len;

		if (dict_index_is_clust(index)) {
			ulint		trx_id_col;

			trx_id_col = dict_index_get_sys_col_pos(index,
								DATA_TRX_ID);
			ut_ad(trx_id_col != ULINT_UNDEFINED);

			/* Store separately trx_id, roll_ptr and
			the BTR_EXTERN_FIELD_REF of each BLOB column. */
			if (rec_offs_any_extern(offsets)) {
				data = page_zip_write_rec_ext(
					page_zip, page,
					rec, index, offsets, create,
					trx_id_col, heap_no, storage, data);
			} else {
				/* Locate trx_id and roll_ptr. */
				const byte*	src
					= rec_get_nth_field(rec, offsets,
							    trx_id_col, &len);
				ut_ad(len == DATA_TRX_ID_LEN);
				ut_ad(src + DATA_TRX_ID_LEN
				      == rec_get_nth_field(
					      rec, offsets,
					      trx_id_col + 1, &len));
				ut_ad(len == DATA_ROLL_PTR_LEN);

				/* Log the preceding fields. */
				ASSERT_ZERO(data, src - rec);
				memcpy(data, rec, src - rec);
				data += src - rec;

				/* Store trx_id and roll_ptr. */
				memcpy(storage
				       - (DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN)
				       * (heap_no - 1),
				       src,
				       DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN);

				src += DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN;

				/* Log the last bytes of the record. */
				len = rec_offs_data_size(offsets)
					- (src - rec);

				ASSERT_ZERO(data, len);
				memcpy(data, src, len);
				data += len;
			}
		} else {
			/* Leaf page of a secondary index:
			no externally stored columns */
			ut_ad(dict_index_get_sys_col_pos(index, DATA_TRX_ID)
			      == ULINT_UNDEFINED);
			ut_ad(!rec_offs_any_extern(offsets));

			/* Log the entire record. */
			len = rec_offs_data_size(offsets);

			ASSERT_ZERO(data, len);
			memcpy(data, rec, len);
			data += len;
		}
	} else {
		/* This is a node pointer page. */
		ulint	len;

		/* Non-leaf nodes should not have any externally
		stored columns. */
		ut_ad(!rec_offs_any_extern(offsets));

		/* Copy the data bytes, except node_ptr. */
		len = rec_offs_data_size(offsets) - REC_NODE_PTR_SIZE;
		ut_ad(data + len < storage - REC_NODE_PTR_SIZE
		      * (page_dir_get_n_heap(page) - PAGE_HEAP_NO_USER_LOW));
		ASSERT_ZERO(data, len);
		memcpy(data, rec, len);
		data += len;

		/* Copy the node pointer to the uncompressed area. */
		memcpy(storage - REC_NODE_PTR_SIZE
		       * (heap_no - 1),
		       rec + len,
		       REC_NODE_PTR_SIZE);
	}

	ut_a(!*data);
	ut_ad((ulint) (data - page_zip->data) < page_zip_get_size(page_zip));
	page_zip->m_end = data - page_zip->data;
	page_zip->m_nonempty = TRUE;

#ifdef UNIV_ZIP_DEBUG
	ut_a(page_zip_validate(page_zip, page_align(rec), index));
#endif /* UNIV_ZIP_DEBUG */
}

/***********************************************************//**
Parses a log record of writing a BLOB pointer of a record.
@return	end of log record or NULL */
UNIV_INTERN
byte*
page_zip_parse_write_blob_ptr(
/*==========================*/
	byte*		ptr,	/*!< in: redo log buffer */
	byte*		end_ptr,/*!< in: redo log buffer end */
	page_t*		page,	/*!< in/out: uncompressed page */
	page_zip_des_t*	page_zip)/*!< in/out: compressed page */
{
	ulint	offset;
	ulint	z_offset;

	ut_ad(!page == !page_zip);

	if (UNIV_UNLIKELY
	    (end_ptr < ptr + (2 + 2 + BTR_EXTERN_FIELD_REF_SIZE))) {

		return(NULL);
	}

	offset = mach_read_from_2(ptr);
	z_offset = mach_read_from_2(ptr + 2);

	if (UNIV_UNLIKELY(offset < PAGE_ZIP_START)
	    || UNIV_UNLIKELY(offset >= UNIV_PAGE_SIZE)
	    || UNIV_UNLIKELY(z_offset >= UNIV_PAGE_SIZE)) {
corrupt:
		recv_sys->found_corrupt_log = TRUE;

		return(NULL);
	}

	if (page) {
		if (UNIV_UNLIKELY(!page_zip)
		    || UNIV_UNLIKELY(!page_is_leaf(page))) {

			goto corrupt;
		}

#ifdef UNIV_ZIP_DEBUG
		ut_a(page_zip_validate(page_zip, page, NULL));
#endif /* UNIV_ZIP_DEBUG */

		memcpy(page + offset,
		       ptr + 4, BTR_EXTERN_FIELD_REF_SIZE);
		memcpy(page_zip->data + z_offset,
		       ptr + 4, BTR_EXTERN_FIELD_REF_SIZE);

#ifdef UNIV_ZIP_DEBUG
		ut_a(page_zip_validate(page_zip, page, NULL));
#endif /* UNIV_ZIP_DEBUG */
	}

	return(ptr + (2 + 2 + BTR_EXTERN_FIELD_REF_SIZE));
}

/**********************************************************************//**
Write a BLOB pointer of a record on the leaf page of a clustered index.
The information must already have been updated on the uncompressed page. */
UNIV_INTERN
void
page_zip_write_blob_ptr(
/*====================*/
	page_zip_des_t*	page_zip,/*!< in/out: compressed page */
	const byte*	rec,	/*!< in/out: record whose data is being
				written */
	dict_index_t*	index,	/*!< in: index of the page */
	const ulint*	offsets,/*!< in: rec_get_offsets(rec, index) */
	ulint		n,	/*!< in: column index */
	mtr_t*		mtr)	/*!< in: mini-transaction handle,
				or NULL if no logging is needed */
{
	const byte*	field;
	byte*		externs;
	const page_t*	page	= page_align(rec);
	ulint		blob_no;
	ulint		len;

	ut_ad(PAGE_ZIP_MATCH(rec, page_zip));
	ut_ad(page_simple_validate_new((page_t*) page));
	ut_ad(page_zip_simple_validate(page_zip));
	ut_ad(page_zip_get_size(page_zip)
	      > PAGE_DATA + page_zip_dir_size(page_zip));
	ut_ad(rec_offs_comp(offsets));
	ut_ad(rec_offs_validate(rec, NULL, offsets));
	ut_ad(rec_offs_any_extern(offsets));
	ut_ad(rec_offs_nth_extern(offsets, n));

	ut_ad(page_zip->m_start >= PAGE_DATA);
	ut_ad(page_zip_header_cmp(page_zip, page));

	ut_ad(page_is_leaf(page));
	ut_ad(dict_index_is_clust(index));

	UNIV_MEM_ASSERT_RW(page_zip->data, page_zip_get_size(page_zip));
	UNIV_MEM_ASSERT_RW(rec, rec_offs_data_size(offsets));
	UNIV_MEM_ASSERT_RW(rec - rec_offs_extra_size(offsets),
			   rec_offs_extra_size(offsets));

	blob_no = page_zip_get_n_prev_extern(page_zip, rec, index)
		+ rec_get_n_extern_new(rec, index, n);
	ut_a(blob_no < page_zip->n_blobs);

	externs = page_zip->data + page_zip_get_size(page_zip)
		- (page_dir_get_n_heap(page) - PAGE_HEAP_NO_USER_LOW)
		* (PAGE_ZIP_DIR_SLOT_SIZE
		   + DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN);

	field = rec_get_nth_field(rec, offsets, n, &len);

	externs -= (blob_no + 1) * BTR_EXTERN_FIELD_REF_SIZE;
	field += len - BTR_EXTERN_FIELD_REF_SIZE;

	memcpy(externs, field, BTR_EXTERN_FIELD_REF_SIZE);

#ifdef UNIV_ZIP_DEBUG
	ut_a(page_zip_validate(page_zip, page, index));
#endif /* UNIV_ZIP_DEBUG */

	if (mtr) {
#ifndef UNIV_HOTBACKUP
		byte*	log_ptr	= mlog_open(
			mtr, 11 + 2 + 2 + BTR_EXTERN_FIELD_REF_SIZE);
		if (UNIV_UNLIKELY(!log_ptr)) {
			return;
		}

		log_ptr = mlog_write_initial_log_record_fast(
			(byte*) field, MLOG_ZIP_WRITE_BLOB_PTR, log_ptr, mtr);
		mach_write_to_2(log_ptr, page_offset(field));
		log_ptr += 2;
		mach_write_to_2(log_ptr, externs - page_zip->data);
		log_ptr += 2;
		memcpy(log_ptr, externs, BTR_EXTERN_FIELD_REF_SIZE);
		log_ptr += BTR_EXTERN_FIELD_REF_SIZE;
		mlog_close(mtr, log_ptr);
#endif /* !UNIV_HOTBACKUP */
	}
}

/***********************************************************//**
Parses a log record of writing the node pointer of a record.
@return	end of log record or NULL */
UNIV_INTERN
byte*
page_zip_parse_write_node_ptr(
/*==========================*/
	byte*		ptr,	/*!< in: redo log buffer */
	byte*		end_ptr,/*!< in: redo log buffer end */
	page_t*		page,	/*!< in/out: uncompressed page */
	page_zip_des_t*	page_zip)/*!< in/out: compressed page */
{
	ulint	offset;
	ulint	z_offset;

	ut_ad(!page == !page_zip);

	if (UNIV_UNLIKELY(end_ptr < ptr + (2 + 2 + REC_NODE_PTR_SIZE))) {

		return(NULL);
	}

	offset = mach_read_from_2(ptr);
	z_offset = mach_read_from_2(ptr + 2);

	if (UNIV_UNLIKELY(offset < PAGE_ZIP_START)
	    || UNIV_UNLIKELY(offset >= UNIV_PAGE_SIZE)
	    || UNIV_UNLIKELY(z_offset >= UNIV_PAGE_SIZE)) {
corrupt:
		recv_sys->found_corrupt_log = TRUE;

		return(NULL);
	}

	if (page) {
		byte*	storage_end;
		byte*	field;
		byte*	storage;
		ulint	heap_no;

		if (UNIV_UNLIKELY(!page_zip)
		    || UNIV_UNLIKELY(page_is_leaf(page))) {

			goto corrupt;
		}

#ifdef UNIV_ZIP_DEBUG
		ut_a(page_zip_validate(page_zip, page, NULL));
#endif /* UNIV_ZIP_DEBUG */

		field = page + offset;
		storage = page_zip->data + z_offset;

		storage_end = page_zip_dir_start(page_zip);

		heap_no = 1 + (storage_end - storage) / REC_NODE_PTR_SIZE;

		if (UNIV_UNLIKELY((storage_end - storage) % REC_NODE_PTR_SIZE)
		    || UNIV_UNLIKELY(heap_no < PAGE_HEAP_NO_USER_LOW)
		    || UNIV_UNLIKELY(heap_no >= page_dir_get_n_heap(page))) {

			goto corrupt;
		}

		memcpy(field, ptr + 4, REC_NODE_PTR_SIZE);
		memcpy(storage, ptr + 4, REC_NODE_PTR_SIZE);

#ifdef UNIV_ZIP_DEBUG
		ut_a(page_zip_validate(page_zip, page, NULL));
#endif /* UNIV_ZIP_DEBUG */
	}

	return(ptr + (2 + 2 + REC_NODE_PTR_SIZE));
}

/**********************************************************************//**
Write the node pointer of a record on a non-leaf compressed page. */
UNIV_INTERN
void
page_zip_write_node_ptr(
/*====================*/
	page_zip_des_t*	page_zip,/*!< in/out: compressed page */
	byte*		rec,	/*!< in/out: record */
	ulint		size,	/*!< in: data size of rec */
	ulint		ptr,	/*!< in: node pointer */
	mtr_t*		mtr)	/*!< in: mini-transaction, or NULL */
{
	byte*	field;
	byte*	storage;
#ifdef UNIV_DEBUG
	page_t*	page	= page_align(rec);
#endif /* UNIV_DEBUG */

	ut_ad(PAGE_ZIP_MATCH(rec, page_zip));
	ut_ad(page_simple_validate_new(page));
	ut_ad(page_zip_simple_validate(page_zip));
	ut_ad(page_zip_get_size(page_zip)
	      > PAGE_DATA + page_zip_dir_size(page_zip));
	ut_ad(page_rec_is_comp(rec));

	ut_ad(page_zip->m_start >= PAGE_DATA);
	ut_ad(page_zip_header_cmp(page_zip, page));

	ut_ad(!page_is_leaf(page));

	UNIV_MEM_ASSERT_RW(page_zip->data, page_zip_get_size(page_zip));
	UNIV_MEM_ASSERT_RW(rec, size);

	storage = page_zip_dir_start(page_zip)
		- (rec_get_heap_no_new(rec) - 1) * REC_NODE_PTR_SIZE;
	field = rec + size - REC_NODE_PTR_SIZE;

#if defined UNIV_DEBUG || defined UNIV_ZIP_DEBUG
	ut_a(!memcmp(storage, field, REC_NODE_PTR_SIZE));
#endif /* UNIV_DEBUG || UNIV_ZIP_DEBUG */
#if REC_NODE_PTR_SIZE != 4
# error "REC_NODE_PTR_SIZE != 4"
#endif
	mach_write_to_4(field, ptr);
	memcpy(storage, field, REC_NODE_PTR_SIZE);

	if (mtr) {
#ifndef UNIV_HOTBACKUP
		byte*	log_ptr	= mlog_open(mtr,
					    11 + 2 + 2 + REC_NODE_PTR_SIZE);
		if (UNIV_UNLIKELY(!log_ptr)) {
			return;
		}

		log_ptr = mlog_write_initial_log_record_fast(
			field, MLOG_ZIP_WRITE_NODE_PTR, log_ptr, mtr);
		mach_write_to_2(log_ptr, page_offset(field));
		log_ptr += 2;
		mach_write_to_2(log_ptr, storage - page_zip->data);
		log_ptr += 2;
		memcpy(log_ptr, field, REC_NODE_PTR_SIZE);
		log_ptr += REC_NODE_PTR_SIZE;
		mlog_close(mtr, log_ptr);
#endif /* !UNIV_HOTBACKUP */
	}
}

/**********************************************************************//**
Write the trx_id and roll_ptr of a record on a B-tree leaf node page. */
UNIV_INTERN
void
page_zip_write_trx_id_and_roll_ptr(
/*===============================*/
	page_zip_des_t*	page_zip,/*!< in/out: compressed page */
	byte*		rec,	/*!< in/out: record */
	const ulint*	offsets,/*!< in: rec_get_offsets(rec, index) */
	ulint		trx_id_col,/*!< in: column number of TRX_ID in rec */
	trx_id_t	trx_id,	/*!< in: transaction identifier */
	roll_ptr_t	roll_ptr)/*!< in: roll_ptr */
{
	byte*	field;
	byte*	storage;
#ifdef UNIV_DEBUG
	page_t*	page	= page_align(rec);
#endif /* UNIV_DEBUG */
	ulint	len;

	ut_ad(PAGE_ZIP_MATCH(rec, page_zip));

	ut_ad(page_simple_validate_new(page));
	ut_ad(page_zip_simple_validate(page_zip));
	ut_ad(page_zip_get_size(page_zip)
	      > PAGE_DATA + page_zip_dir_size(page_zip));
	ut_ad(rec_offs_validate(rec, NULL, offsets));
	ut_ad(rec_offs_comp(offsets));

	ut_ad(page_zip->m_start >= PAGE_DATA);
	ut_ad(page_zip_header_cmp(page_zip, page));

	ut_ad(page_is_leaf(page));

	UNIV_MEM_ASSERT_RW(page_zip->data, page_zip_get_size(page_zip));

	storage = page_zip_dir_start(page_zip)
		- (rec_get_heap_no_new(rec) - 1)
		* (DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN);

#if DATA_TRX_ID + 1 != DATA_ROLL_PTR
# error "DATA_TRX_ID + 1 != DATA_ROLL_PTR"
#endif
	field = rec_get_nth_field(rec, offsets, trx_id_col, &len);
	ut_ad(len == DATA_TRX_ID_LEN);
	ut_ad(field + DATA_TRX_ID_LEN
	      == rec_get_nth_field(rec, offsets, trx_id_col + 1, &len));
	ut_ad(len == DATA_ROLL_PTR_LEN);
#if defined UNIV_DEBUG || defined UNIV_ZIP_DEBUG
	ut_a(!memcmp(storage, field, DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN));
#endif /* UNIV_DEBUG || UNIV_ZIP_DEBUG */
#if DATA_TRX_ID_LEN != 6
# error "DATA_TRX_ID_LEN != 6"
#endif
	mach_write_to_6(field, trx_id);
#if DATA_ROLL_PTR_LEN != 7
# error "DATA_ROLL_PTR_LEN != 7"
#endif
	mach_write_to_7(field + DATA_TRX_ID_LEN, roll_ptr);
	memcpy(storage, field, DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN);

	UNIV_MEM_ASSERT_RW(rec, rec_offs_data_size(offsets));
	UNIV_MEM_ASSERT_RW(rec - rec_offs_extra_size(offsets),
			   rec_offs_extra_size(offsets));
	UNIV_MEM_ASSERT_RW(page_zip->data, page_zip_get_size(page_zip));
}

/**********************************************************************//**
Clear an area on the uncompressed and compressed page.
Do not clear the data payload, as that would grow the modification log. */
static
void
page_zip_clear_rec(
/*===============*/
	page_zip_des_t*	page_zip,	/*!< in/out: compressed page */
	byte*		rec,		/*!< in: record to clear */
	const dict_index_t*	index,	/*!< in: index of rec */
	const ulint*	offsets)	/*!< in: rec_get_offsets(rec, index) */
{
	ulint	heap_no;
	page_t*	page	= page_align(rec);
	byte*	storage;
	byte*	field;
	ulint	len;
	/* page_zip_validate() would fail here if a record
	containing externally stored columns is being deleted. */
	ut_ad(rec_offs_validate(rec, index, offsets));
	ut_ad(!page_zip_dir_find(page_zip, page_offset(rec)));
	ut_ad(page_zip_dir_find_free(page_zip, page_offset(rec)));
	ut_ad(page_zip_header_cmp(page_zip, page));

	heap_no = rec_get_heap_no_new(rec);
	ut_ad(heap_no >= PAGE_HEAP_NO_USER_LOW);

	UNIV_MEM_ASSERT_RW(page_zip->data, page_zip_get_size(page_zip));
	UNIV_MEM_ASSERT_RW(rec, rec_offs_data_size(offsets));
	UNIV_MEM_ASSERT_RW(rec - rec_offs_extra_size(offsets),
			   rec_offs_extra_size(offsets));

	if (!page_is_leaf(page)) {
		/* Clear node_ptr. On the compressed page,
		there is an array of node_ptr immediately before the
		dense page directory, at the very end of the page. */
		storage	= page_zip_dir_start(page_zip);
		ut_ad(dict_index_get_n_unique_in_tree(index) ==
		      rec_offs_n_fields(offsets) - 1);
		field	= rec_get_nth_field(rec, offsets,
					    rec_offs_n_fields(offsets) - 1,
					    &len);
		ut_ad(len == REC_NODE_PTR_SIZE);

		ut_ad(!rec_offs_any_extern(offsets));
		memset(field, 0, REC_NODE_PTR_SIZE);
		memset(storage - (heap_no - 1) * REC_NODE_PTR_SIZE,
		       0, REC_NODE_PTR_SIZE);
	} else if (dict_index_is_clust(index)) {
		/* Clear trx_id and roll_ptr. On the compressed page,
		there is an array of these fields immediately before the
		dense page directory, at the very end of the page. */
		const ulint	trx_id_pos
			= dict_col_get_clust_pos(
			dict_table_get_sys_col(
				index->table, DATA_TRX_ID), index);
		storage	= page_zip_dir_start(page_zip);
		field	= rec_get_nth_field(rec, offsets, trx_id_pos, &len);
		ut_ad(len == DATA_TRX_ID_LEN);

		memset(field, 0, DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN);
		memset(storage - (heap_no - 1)
		       * (DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN),
		       0, DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN);

		if (rec_offs_any_extern(offsets)) {
			ulint	i;

			for (i = rec_offs_n_fields(offsets); i--; ) {
				/* Clear all BLOB pointers in order to make
				page_zip_validate() pass. */
				if (rec_offs_nth_extern(offsets, i)) {
					field = rec_get_nth_field(
						rec, offsets, i, &len);
					ut_ad(len
					      == BTR_EXTERN_FIELD_REF_SIZE);
					memset(field + len
					       - BTR_EXTERN_FIELD_REF_SIZE,
					       0, BTR_EXTERN_FIELD_REF_SIZE);
				}
			}
		}
	} else {
		ut_ad(!rec_offs_any_extern(offsets));
	}

#ifdef UNIV_ZIP_DEBUG
	ut_a(page_zip_validate(page_zip, page, index));
#endif /* UNIV_ZIP_DEBUG */
}

/**********************************************************************//**
Write the "deleted" flag of a record on a compressed page.  The flag must
already have been written on the uncompressed page. */
UNIV_INTERN
void
page_zip_rec_set_deleted(
/*=====================*/
	page_zip_des_t*	page_zip,/*!< in/out: compressed page */
	const byte*	rec,	/*!< in: record on the uncompressed page */
	ulint		flag)	/*!< in: the deleted flag (nonzero=TRUE) */
{
	byte*	slot = page_zip_dir_find(page_zip, page_offset(rec));
	ut_a(slot);
	UNIV_MEM_ASSERT_RW(page_zip->data, page_zip_get_size(page_zip));
	if (flag) {
		*slot |= (PAGE_ZIP_DIR_SLOT_DEL >> 8);
	} else {
		*slot &= ~(PAGE_ZIP_DIR_SLOT_DEL >> 8);
	}
#ifdef UNIV_ZIP_DEBUG
	ut_a(page_zip_validate(page_zip, page_align(rec), NULL));
#endif /* UNIV_ZIP_DEBUG */
}

/**********************************************************************//**
Write the "owned" flag of a record on a compressed page.  The n_owned field
must already have been written on the uncompressed page. */
UNIV_INTERN
void
page_zip_rec_set_owned(
/*===================*/
	page_zip_des_t*	page_zip,/*!< in/out: compressed page */
	const byte*	rec,	/*!< in: record on the uncompressed page */
	ulint		flag)	/*!< in: the owned flag (nonzero=TRUE) */
{
	byte*	slot = page_zip_dir_find(page_zip, page_offset(rec));
	ut_a(slot);
	UNIV_MEM_ASSERT_RW(page_zip->data, page_zip_get_size(page_zip));
	if (flag) {
		*slot |= (PAGE_ZIP_DIR_SLOT_OWNED >> 8);
	} else {
		*slot &= ~(PAGE_ZIP_DIR_SLOT_OWNED >> 8);
	}
}

/**********************************************************************//**
Insert a record to the dense page directory. */
UNIV_INTERN
void
page_zip_dir_insert(
/*================*/
	page_zip_des_t*	page_zip,/*!< in/out: compressed page */
	const byte*	prev_rec,/*!< in: record after which to insert */
	const byte*	free_rec,/*!< in: record from which rec was
				allocated, or NULL */
	byte*		rec)	/*!< in: record to insert */
{
	ulint	n_dense;
	byte*	slot_rec;
	byte*	slot_free;

	ut_ad(prev_rec != rec);
	ut_ad(page_rec_get_next((rec_t*) prev_rec) == rec);
	ut_ad(page_zip_simple_validate(page_zip));

	UNIV_MEM_ASSERT_RW(page_zip->data, page_zip_get_size(page_zip));

	if (page_rec_is_infimum(prev_rec)) {
		/* Use the first slot. */
		slot_rec = page_zip->data + page_zip_get_size(page_zip);
	} else {
		byte*	end	= page_zip->data + page_zip_get_size(page_zip);
		byte*	start	= end - page_zip_dir_user_size(page_zip);

		if (UNIV_LIKELY(!free_rec)) {
			/* PAGE_N_RECS was already incremented
			in page_cur_insert_rec_zip(), but the
			dense directory slot at that position
			contains garbage.  Skip it. */
			start += PAGE_ZIP_DIR_SLOT_SIZE;
		}

		slot_rec = page_zip_dir_find_low(start, end,
						 page_offset(prev_rec));
		ut_a(slot_rec);
	}

	/* Read the old n_dense (n_heap may have been incremented). */
	n_dense = page_dir_get_n_heap(page_zip->data)
		- (PAGE_HEAP_NO_USER_LOW + 1);

	if (UNIV_LIKELY_NULL(free_rec)) {
		/* The record was allocated from the free list.
		Shift the dense directory only up to that slot.
		Note that in this case, n_dense is actually
		off by one, because page_cur_insert_rec_zip()
		did not increment n_heap. */
		ut_ad(rec_get_heap_no_new(rec) < n_dense + 1
		      + PAGE_HEAP_NO_USER_LOW);
		ut_ad(rec >= free_rec);
		slot_free = page_zip_dir_find(page_zip, page_offset(free_rec));
		ut_ad(slot_free);
		slot_free += PAGE_ZIP_DIR_SLOT_SIZE;
	} else {
		/* The record was allocated from the heap.
		Shift the entire dense directory. */
		ut_ad(rec_get_heap_no_new(rec) == n_dense
		      + PAGE_HEAP_NO_USER_LOW);

		/* Shift to the end of the dense page directory. */
		slot_free = page_zip->data + page_zip_get_size(page_zip)
			- PAGE_ZIP_DIR_SLOT_SIZE * n_dense;
	}

	/* Shift the dense directory to allocate place for rec. */
	memmove(slot_free - PAGE_ZIP_DIR_SLOT_SIZE, slot_free,
		slot_rec - slot_free);

	/* Write the entry for the inserted record.
	The "owned" and "deleted" flags must be zero. */
	mach_write_to_2(slot_rec - PAGE_ZIP_DIR_SLOT_SIZE, page_offset(rec));
}

/**********************************************************************//**
Shift the dense page directory and the array of BLOB pointers
when a record is deleted. */
UNIV_INTERN
void
page_zip_dir_delete(
/*================*/
	page_zip_des_t*		page_zip,	/*!< in/out: compressed page */
	byte*			rec,		/*!< in: deleted record */
	const dict_index_t*	index,		/*!< in: index of rec */
	const ulint*		offsets,	/*!< in: rec_get_offsets(rec) */
	const byte*		free)		/*!< in: previous start of
						the free list */
{
	byte*	slot_rec;
	byte*	slot_free;
	ulint	n_ext;
	page_t*	page	= page_align(rec);

	ut_ad(rec_offs_validate(rec, index, offsets));
	ut_ad(rec_offs_comp(offsets));

	UNIV_MEM_ASSERT_RW(page_zip->data, page_zip_get_size(page_zip));
	UNIV_MEM_ASSERT_RW(rec, rec_offs_data_size(offsets));
	UNIV_MEM_ASSERT_RW(rec - rec_offs_extra_size(offsets),
			   rec_offs_extra_size(offsets));

	slot_rec = page_zip_dir_find(page_zip, page_offset(rec));

	ut_a(slot_rec);

	/* This could not be done before page_zip_dir_find(). */
	page_header_set_field(page, page_zip, PAGE_N_RECS,
			      (ulint)(page_get_n_recs(page) - 1));

	if (UNIV_UNLIKELY(!free)) {
		/* Make the last slot the start of the free list. */
		slot_free = page_zip->data + page_zip_get_size(page_zip)
			- PAGE_ZIP_DIR_SLOT_SIZE
			* (page_dir_get_n_heap(page_zip->data)
			   - PAGE_HEAP_NO_USER_LOW);
	} else {
		slot_free = page_zip_dir_find_free(page_zip,
						   page_offset(free));
		ut_a(slot_free < slot_rec);
		/* Grow the free list by one slot by moving the start. */
		slot_free += PAGE_ZIP_DIR_SLOT_SIZE;
	}

	if (UNIV_LIKELY(slot_rec > slot_free)) {
		memmove(slot_free + PAGE_ZIP_DIR_SLOT_SIZE,
			slot_free,
			slot_rec - slot_free);
	}

	/* Write the entry for the deleted record.
	The "owned" and "deleted" flags will be cleared. */
	mach_write_to_2(slot_free, page_offset(rec));

	if (!page_is_leaf(page) || !dict_index_is_clust(index)) {
		ut_ad(!rec_offs_any_extern(offsets));
		goto skip_blobs;
	}

	n_ext = rec_offs_n_extern(offsets);
	if (UNIV_UNLIKELY(n_ext)) {
		/* Shift and zero fill the array of BLOB pointers. */
		ulint	blob_no;
		byte*	externs;
		byte*	ext_end;

		blob_no = page_zip_get_n_prev_extern(page_zip, rec, index);
		ut_a(blob_no + n_ext <= page_zip->n_blobs);

		externs = page_zip->data + page_zip_get_size(page_zip)
			- (page_dir_get_n_heap(page) - PAGE_HEAP_NO_USER_LOW)
			* (PAGE_ZIP_DIR_SLOT_SIZE
			   + DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN);

		ext_end = externs - page_zip->n_blobs
			* BTR_EXTERN_FIELD_REF_SIZE;
		externs -= blob_no * BTR_EXTERN_FIELD_REF_SIZE;

		page_zip->n_blobs -= n_ext;
		/* Shift and zero fill the array. */
		memmove(ext_end + n_ext * BTR_EXTERN_FIELD_REF_SIZE, ext_end,
			(page_zip->n_blobs - blob_no)
			* BTR_EXTERN_FIELD_REF_SIZE);
		memset(ext_end, 0, n_ext * BTR_EXTERN_FIELD_REF_SIZE);
	}

skip_blobs:
	/* The compression algorithm expects info_bits and n_owned
	to be 0 for deleted records. */
	rec[-REC_N_NEW_EXTRA_BYTES] = 0; /* info_bits and n_owned */

	page_zip_clear_rec(page_zip, rec, index, offsets);
}

/**********************************************************************//**
Add a slot to the dense page directory. */
UNIV_INTERN
void
page_zip_dir_add_slot(
/*==================*/
	page_zip_des_t*	page_zip,	/*!< in/out: compressed page */
	ulint		is_clustered)	/*!< in: nonzero for clustered index,
					zero for others */
{
	ulint	n_dense;
	byte*	dir;
	byte*	stored;

	ut_ad(page_is_comp(page_zip->data));
	UNIV_MEM_ASSERT_RW(page_zip->data, page_zip_get_size(page_zip));

	/* Read the old n_dense (n_heap has already been incremented). */
	n_dense = page_dir_get_n_heap(page_zip->data)
		- (PAGE_HEAP_NO_USER_LOW + 1);

	dir = page_zip->data + page_zip_get_size(page_zip)
		- PAGE_ZIP_DIR_SLOT_SIZE * n_dense;

	if (!page_is_leaf(page_zip->data)) {
		ut_ad(!page_zip->n_blobs);
		stored = dir - n_dense * REC_NODE_PTR_SIZE;
	} else if (is_clustered) {
		/* Move the BLOB pointer array backwards to make space for the
		roll_ptr and trx_id columns and the dense directory slot. */
		byte*	externs;

		stored = dir - n_dense
			* (DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN);
		externs = stored
			- page_zip->n_blobs * BTR_EXTERN_FIELD_REF_SIZE;
		ASSERT_ZERO(externs
			    - (PAGE_ZIP_DIR_SLOT_SIZE
			       + DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN),
			    PAGE_ZIP_DIR_SLOT_SIZE
			    + DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN);
		memmove(externs - (PAGE_ZIP_DIR_SLOT_SIZE
				   + DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN),
			externs, stored - externs);
	} else {
		stored = dir
			- page_zip->n_blobs * BTR_EXTERN_FIELD_REF_SIZE;
		ASSERT_ZERO(stored - PAGE_ZIP_DIR_SLOT_SIZE,
			    PAGE_ZIP_DIR_SLOT_SIZE);
	}

	/* Move the uncompressed area backwards to make space
	for one directory slot. */
	memmove(stored - PAGE_ZIP_DIR_SLOT_SIZE, stored, dir - stored);
}

/***********************************************************//**
Parses a log record of writing to the header of a page.
@return	end of log record or NULL */
UNIV_INTERN
byte*
page_zip_parse_write_header(
/*========================*/
	byte*		ptr,	/*!< in: redo log buffer */
	byte*		end_ptr,/*!< in: redo log buffer end */
	page_t*		page,	/*!< in/out: uncompressed page */
	page_zip_des_t*	page_zip)/*!< in/out: compressed page */
{
	ulint	offset;
	ulint	len;

	ut_ad(ptr && end_ptr);
	ut_ad(!page == !page_zip);

	if (UNIV_UNLIKELY(end_ptr < ptr + (1 + 1))) {

		return(NULL);
	}

	offset = (ulint) *ptr++;
	len = (ulint) *ptr++;

	if (UNIV_UNLIKELY(!len) || UNIV_UNLIKELY(offset + len >= PAGE_DATA)) {
corrupt:
		recv_sys->found_corrupt_log = TRUE;

		return(NULL);
	}

	if (UNIV_UNLIKELY(end_ptr < ptr + len)) {

		return(NULL);
	}

	if (page) {
		if (UNIV_UNLIKELY(!page_zip)) {

			goto corrupt;
		}
#ifdef UNIV_ZIP_DEBUG
		ut_a(page_zip_validate(page_zip, page, NULL));
#endif /* UNIV_ZIP_DEBUG */

		memcpy(page + offset, ptr, len);
		memcpy(page_zip->data + offset, ptr, len);

#ifdef UNIV_ZIP_DEBUG
		ut_a(page_zip_validate(page_zip, page, NULL));
#endif /* UNIV_ZIP_DEBUG */
	}

	return(ptr + len);
}

#ifndef UNIV_HOTBACKUP
/**********************************************************************//**
Write a log record of writing to the uncompressed header portion of a page. */
UNIV_INTERN
void
page_zip_write_header_log(
/*======================*/
	const byte*	data,	/*!< in: data on the uncompressed page */
	ulint		length,	/*!< in: length of the data */
	mtr_t*		mtr)	/*!< in: mini-transaction */
{
	byte*	log_ptr	= mlog_open(mtr, 11 + 1 + 1);
	ulint	offset	= page_offset(data);

	ut_ad(offset < PAGE_DATA);
	ut_ad(offset + length < PAGE_DATA);
#if PAGE_DATA > 255
# error "PAGE_DATA > 255"
#endif
	ut_ad(length < 256);

	/* If no logging is requested, we may return now */
	if (UNIV_UNLIKELY(!log_ptr)) {

		return;
	}

	log_ptr = mlog_write_initial_log_record_fast(
		(byte*) data, MLOG_ZIP_WRITE_HEADER, log_ptr, mtr);
	*log_ptr++ = (byte) offset;
	*log_ptr++ = (byte) length;
	mlog_close(mtr, log_ptr);

	mlog_catenate_string(mtr, data, length);
}
#endif /* !UNIV_HOTBACKUP */

/**********************************************************************//**
Reorganize and compress a page.  This is a low-level operation for
compressed pages, to be used when page_zip_compress() fails.
On success, a redo log entry MLOG_ZIP_PAGE_COMPRESS will be written.
The function btr_page_reorganize() should be preferred whenever possible.
IMPORTANT: if page_zip_reorganize() is invoked on a leaf page of a
non-clustered index, the caller must update the insert buffer free
bits in the same mini-transaction in such a way that the modification
will be redo-logged.
@return TRUE on success, FALSE on failure; page_zip will be left
intact on failure, but page will be overwritten. */
UNIV_INTERN
ibool
page_zip_reorganize(
/*================*/
	buf_block_t*	block,	/*!< in/out: page with compressed page;
				on the compressed page, in: size;
				out: data, n_blobs,
				m_start, m_end, m_nonempty */
	dict_index_t*	index,	/*!< in: index of the B-tree node */
	mtr_t*		mtr)	/*!< in: mini-transaction */
{
#ifndef UNIV_HOTBACKUP
	buf_pool_t*	buf_pool	= buf_pool_from_block(block);
#endif /* !UNIV_HOTBACKUP */
	page_zip_des_t*	page_zip	= buf_block_get_page_zip(block);
	page_t*		page		= buf_block_get_frame(block);
	buf_block_t*	temp_block;
	page_t*		temp_page;
	ulint		log_mode;

	ut_ad(mtr_memo_contains(mtr, block, MTR_MEMO_PAGE_X_FIX));
	ut_ad(page_is_comp(page));
	ut_ad(!dict_index_is_ibuf(index));
	/* Note that page_zip_validate(page_zip, page, index) may fail here. */
	UNIV_MEM_ASSERT_RW(page, UNIV_PAGE_SIZE);
	UNIV_MEM_ASSERT_RW(page_zip->data, page_zip_get_size(page_zip));

	/* Disable logging */
	log_mode = mtr_set_log_mode(mtr, MTR_LOG_NONE);

#ifndef UNIV_HOTBACKUP
	temp_block = buf_block_alloc(buf_pool);
	btr_search_drop_page_hash_index(block);
	block->check_index_page_at_flush = TRUE;
#else /* !UNIV_HOTBACKUP */
	ut_ad(block == back_block1);
	temp_block = back_block2;
#endif /* !UNIV_HOTBACKUP */
	temp_page = temp_block->frame;

	/* Copy the old page to temporary space */
	buf_frame_copy(temp_page, page);

	btr_blob_dbg_remove(page, index, "zip_reorg");

	/* Recreate the page: note that global data on page (possible
	segment headers, next page-field, etc.) is preserved intact */

	page_create(block, mtr, TRUE);

	/* Copy the records from the temporary space to the recreated page;
	do not copy the lock bits yet */

	page_copy_rec_list_end_no_locks(block, temp_block,
					page_get_infimum_rec(temp_page),
					index, mtr);

	if (!dict_index_is_clust(index) && page_is_leaf(temp_page)) {
		/* Copy max trx id to recreated page */
		trx_id_t	max_trx_id = page_get_max_trx_id(temp_page);
		page_set_max_trx_id(block, NULL, max_trx_id, NULL);
		ut_ad(max_trx_id != 0);
	}

	/* Restore logging. */
	mtr_set_log_mode(mtr, log_mode);

	if (!page_zip_compress(page_zip, page, index,
			       page_compression_level, NULL, mtr)) {

#ifndef UNIV_HOTBACKUP
		buf_block_free(temp_block);
#endif /* !UNIV_HOTBACKUP */
		return(FALSE);
	}

	lock_move_reorganize_page(block, temp_block);

#ifndef UNIV_HOTBACKUP
	buf_block_free(temp_block);
#endif /* !UNIV_HOTBACKUP */
	return(TRUE);
}

#ifndef UNIV_HOTBACKUP
/**********************************************************************//**
Copy the records of a page byte for byte.  Do not copy the page header
or trailer, except those B-tree header fields that are directly
related to the storage of records.  Also copy PAGE_MAX_TRX_ID.
NOTE: The caller must update the lock table and the adaptive hash index. */
UNIV_INTERN
void
page_zip_copy_recs(
/*===============*/
	page_zip_des_t*		page_zip,	/*!< out: copy of src_zip
						(n_blobs, m_start, m_end,
						m_nonempty, data[0..size-1]) */
	page_t*			page,		/*!< out: copy of src */
	const page_zip_des_t*	src_zip,	/*!< in: compressed page */
	const page_t*		src,		/*!< in: page */
	dict_index_t*		index,		/*!< in: index of the B-tree */
	mtr_t*			mtr)		/*!< in: mini-transaction */
{
	ut_ad(mtr_memo_contains_page(mtr, page, MTR_MEMO_PAGE_X_FIX));
	ut_ad(mtr_memo_contains_page(mtr, src, MTR_MEMO_PAGE_X_FIX));
	ut_ad(!dict_index_is_ibuf(index));
#ifdef UNIV_ZIP_DEBUG
	/* The B-tree operations that call this function may set
	FIL_PAGE_PREV or PAGE_LEVEL, causing a temporary min_rec_flag
	mismatch.  A strict page_zip_validate() will be executed later
	during the B-tree operations. */
	ut_a(page_zip_validate_low(src_zip, src, index, TRUE));
#endif /* UNIV_ZIP_DEBUG */
	ut_a(page_zip_get_size(page_zip) == page_zip_get_size(src_zip));
	if (UNIV_UNLIKELY(src_zip->n_blobs)) {
		ut_a(page_is_leaf(src));
		ut_a(dict_index_is_clust(index));
	}

	/* The PAGE_MAX_TRX_ID must be set on leaf pages of secondary
	indexes.  It does not matter on other pages. */
	ut_a(dict_index_is_clust(index) || !page_is_leaf(src)
	     || page_get_max_trx_id(src));

	UNIV_MEM_ASSERT_W(page, UNIV_PAGE_SIZE);
	UNIV_MEM_ASSERT_W(page_zip->data, page_zip_get_size(page_zip));
	UNIV_MEM_ASSERT_RW(src, UNIV_PAGE_SIZE);
	UNIV_MEM_ASSERT_RW(src_zip->data, page_zip_get_size(page_zip));

	/* Copy those B-tree page header fields that are related to
	the records stored in the page.  Also copy the field
	PAGE_MAX_TRX_ID.  Skip the rest of the page header and
	trailer.  On the compressed page, there is no trailer. */
#if PAGE_MAX_TRX_ID + 8 != PAGE_HEADER_PRIV_END
# error "PAGE_MAX_TRX_ID + 8 != PAGE_HEADER_PRIV_END"
#endif
	memcpy(PAGE_HEADER + page, PAGE_HEADER + src,
	       PAGE_HEADER_PRIV_END);
	memcpy(PAGE_DATA + page, PAGE_DATA + src,
	       UNIV_PAGE_SIZE - PAGE_DATA - FIL_PAGE_DATA_END);
	memcpy(PAGE_HEADER + page_zip->data, PAGE_HEADER + src_zip->data,
	       PAGE_HEADER_PRIV_END);
	memcpy(PAGE_DATA + page_zip->data, PAGE_DATA + src_zip->data,
	       page_zip_get_size(page_zip) - PAGE_DATA);

	/* Copy all fields of src_zip to page_zip, except the pointer
	to the compressed data page. */
	{
		page_zip_t*	data = page_zip->data;
		memcpy(page_zip, src_zip, sizeof *page_zip);
		page_zip->data = data;
	}
	ut_ad(page_zip_get_trailer_len(page_zip,
				       dict_index_is_clust(index), NULL)
	      + page_zip->m_end < page_zip_get_size(page_zip));

	if (!page_is_leaf(src)
	    && UNIV_UNLIKELY(mach_read_from_4(src + FIL_PAGE_PREV) == FIL_NULL)
	    && UNIV_LIKELY(mach_read_from_4(page
					    + FIL_PAGE_PREV) != FIL_NULL)) {
		/* Clear the REC_INFO_MIN_REC_FLAG of the first user record. */
		ulint	offs = rec_get_next_offs(page + PAGE_NEW_INFIMUM,
						 TRUE);
		if (UNIV_LIKELY(offs != PAGE_NEW_SUPREMUM)) {
			rec_t*	rec = page + offs;
			ut_a(rec[-REC_N_NEW_EXTRA_BYTES]
			     & REC_INFO_MIN_REC_FLAG);
			rec[-REC_N_NEW_EXTRA_BYTES] &= ~ REC_INFO_MIN_REC_FLAG;
		}
	}

#ifdef UNIV_ZIP_DEBUG
	ut_a(page_zip_validate(page_zip, page, index));
#endif /* UNIV_ZIP_DEBUG */
	btr_blob_dbg_add(page, index, "page_zip_copy_recs");

	page_zip_compress_write_log(page_zip, page, index, mtr);
}
#endif /* !UNIV_HOTBACKUP */

/**********************************************************************//**
Parses a log record of compressing an index page.
@return	end of log record or NULL */
UNIV_INTERN
byte*
page_zip_parse_compress(
/*====================*/
	byte*		ptr,	/*!< in: buffer */
	byte*		end_ptr,/*!< in: buffer end */
	page_t*		page,	/*!< out: uncompressed page */
	page_zip_des_t*	page_zip)/*!< out: compressed page */
{
	ulint	size;
	ulint	trailer_size;

	ut_ad(ptr && end_ptr);
	ut_ad(!page == !page_zip);

	if (UNIV_UNLIKELY(ptr + (2 + 2) > end_ptr)) {

		return(NULL);
	}

	size = mach_read_from_2(ptr);
	ptr += 2;
	trailer_size = mach_read_from_2(ptr);
	ptr += 2;

	if (UNIV_UNLIKELY(ptr + 8 + size + trailer_size > end_ptr)) {

		return(NULL);
	}

	if (page) {
		if (UNIV_UNLIKELY(!page_zip)
		    || UNIV_UNLIKELY(page_zip_get_size(page_zip) < size)) {
corrupt:
			recv_sys->found_corrupt_log = TRUE;

			return(NULL);
		}

		memcpy(page_zip->data + FIL_PAGE_PREV, ptr, 4);
		memcpy(page_zip->data + FIL_PAGE_NEXT, ptr + 4, 4);
		memcpy(page_zip->data + FIL_PAGE_TYPE, ptr + 8, size);
		memset(page_zip->data + FIL_PAGE_TYPE + size, 0,
		       page_zip_get_size(page_zip) - trailer_size
		       - (FIL_PAGE_TYPE + size));
		memcpy(page_zip->data + page_zip_get_size(page_zip)
		       - trailer_size, ptr + 8 + size, trailer_size);

		if (UNIV_UNLIKELY(!page_zip_decompress(page_zip, page,
						       TRUE))) {

			goto corrupt;
		}
	}

	return(ptr + 8 + size + trailer_size);
}

/**********************************************************************//**
Calculate the compressed page checksum.
@return	page checksum */
UNIV_INTERN
ulint
page_zip_calc_checksum(
/*===================*/
	const void*	data,	/*!< in: compressed page */
	ulint		size,	/*!< in: size of compressed page */
	srv_checksum_algorithm_t algo) /*!< in: algorithm to use */
{
	uLong		adler;
	ib_uint32_t	crc32;
	const Bytef*	s = static_cast<const byte*>(data);

	/* Exclude FIL_PAGE_SPACE_OR_CHKSUM, FIL_PAGE_LSN,
	and FIL_PAGE_FILE_FLUSH_LSN from the checksum. */

	switch (algo) {
	case SRV_CHECKSUM_ALGORITHM_CRC32:
	case SRV_CHECKSUM_ALGORITHM_STRICT_CRC32:

		ut_ad(size > FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID);

		crc32 = ut_crc32(s + FIL_PAGE_OFFSET,
				 FIL_PAGE_LSN - FIL_PAGE_OFFSET)
			^ ut_crc32(s + FIL_PAGE_TYPE, 2)
			^ ut_crc32(s + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID,
				   size - FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID);

		return((ulint) crc32);
	case SRV_CHECKSUM_ALGORITHM_INNODB:
	case SRV_CHECKSUM_ALGORITHM_STRICT_INNODB:
		ut_ad(size > FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID);

		adler = adler32(0L, s + FIL_PAGE_OFFSET,
				FIL_PAGE_LSN - FIL_PAGE_OFFSET);
		adler = adler32(adler, s + FIL_PAGE_TYPE, 2);
		adler = adler32(adler, s + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID,
				size - FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID);

		return((ulint) adler);
	case SRV_CHECKSUM_ALGORITHM_NONE:
	case SRV_CHECKSUM_ALGORITHM_STRICT_NONE:
		return(BUF_NO_CHECKSUM_MAGIC);
	/* no default so the compiler will emit a warning if new enum
	is added and not handled here */
	}

	ut_error;
	return(0);
}

/**********************************************************************//**
Verify a compressed page's checksum.
@return	TRUE if the stored checksum is valid according to the value of
innodb_checksum_algorithm */
UNIV_INTERN
ibool
page_zip_verify_checksum(
/*=====================*/
	const void*	data,	/*!< in: compressed page */
	ulint		size)	/*!< in: size of compressed page */
{
	ib_uint32_t	stored;
	ib_uint32_t	calc;
	ib_uint32_t	crc32 = 0 /* silence bogus warning */;
	ib_uint32_t	innodb = 0 /* silence bogus warning */;

	stored = mach_read_from_4(
		(const unsigned char*) data + FIL_PAGE_SPACE_OR_CHKSUM);

	/* declare empty pages non-corrupted */
	if (stored == 0) {
		/* make sure that the page is really empty */
		ut_d(ulint i; for (i = 0; i < size; i++) {
		     ut_a(*((const char*) data + i) == 0); });

		return(TRUE);
	}

	calc = page_zip_calc_checksum(
		data, size, static_cast<srv_checksum_algorithm_t>(
			srv_checksum_algorithm));

	if (stored == calc) {
		return(TRUE);
	}

	switch ((srv_checksum_algorithm_t) srv_checksum_algorithm) {
	case SRV_CHECKSUM_ALGORITHM_STRICT_CRC32:
	case SRV_CHECKSUM_ALGORITHM_STRICT_INNODB:
	case SRV_CHECKSUM_ALGORITHM_STRICT_NONE:
		return(stored == calc);
	case SRV_CHECKSUM_ALGORITHM_CRC32:
		if (stored == BUF_NO_CHECKSUM_MAGIC) {
			return(TRUE);
		}
		crc32 = calc;
		innodb = page_zip_calc_checksum(
			data, size, SRV_CHECKSUM_ALGORITHM_INNODB);
		break;
	case SRV_CHECKSUM_ALGORITHM_INNODB:
		if (stored == BUF_NO_CHECKSUM_MAGIC) {
			return(TRUE);
		}
		crc32 = page_zip_calc_checksum(
			data, size, SRV_CHECKSUM_ALGORITHM_CRC32);
		innodb = calc;
		break;
	case SRV_CHECKSUM_ALGORITHM_NONE:
		return(TRUE);
	/* no default so the compiler will emit a warning if new enum
	is added and not handled here */
	}

	return(stored == crc32 || stored == innodb);
}
