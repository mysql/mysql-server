/*****************************************************************************
Copyright (c) 1995, 2012, Oracle and/or its affiliates. All Rights Reserved.

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
@file include/os0vio.h
Support for vectored IO

Created 2012/02/15 Inaam Rana
*******************************************************/

#ifndef os0vio_h
#define os0vio_h

#include "univ.i"
#include "os0file.h"

/**********************************************************************//**
Vectored IO support.
Currently the only consumer of this is dblwr.
IMPORTANT: We support Vectored IO with following conditions:
* Number of bytes to write are multiple of UNIV_PAGE_SIZE
* On Windows we have currently disabled vectored IO. To enable it
define HAVE_WIN_SCATTER_GATHER_IO in univ.i. The code for windows
is already present and is working but we have to sort out pagesize
restrictions before we enable it.

The fallback mechanism:
* On Windows currently use buffered read/write
* On non-windows platforms
  * use preadv/pwritev if available (modern linux kernels)
  * else use readv/writev
  * else use pread/pwrite
  * else use read/write */

/** define iovec struct if it is not defined. */
#if !defined(HAVE_READV) && !defined(HAVE_WIN_SCATTER_GATHER_IO)
struct iovec {
	void*	iov_base;	/*!< address of buffer. */
	ulint	iov_len;	/*!< length. */
};
#endif /* !defined(HAVE_READV) && !defined(HAVE_WIN_SCATTER_GATHER_IO) */

/** Vectored IO control block */
struct os_vio_struct{
	ibool		init;	/*!< TRUE if it has been inited */
	ulint		type;	/*!< type of operation OS_FILE_READV
				or OS_FILE_WRITEV */
	os_file_t	fh;	/*!< file handle */
	os_offset_t	offset;	/*!< offset where to perform IO */
	ulint		size;	/*!< total size in bytes */
	ulint		n_elems;/*!< number of elements this struct
				can handle. */
	ulint		cur_size;/*!< current size in bytes */
	ulint		cnt;	/*!< current number of elements */
#if defined(HAVE_WIN_SCATTER_GATHER_IO)
	FILE_SEGMENT_ELEMENT*	iov;/*!< IO vector */
	OVERLAPPED	ol;	/*!< overlap structure */
#else /* __WIN__ */
	struct iovec*	iov;	/*!< array of IO request vector */

	byte*		buf_ua;	/*!< unaligned buffer. To be used only
				if vectored IO is not supported. */
	byte*		buf;	/*!< buffer to use in rare condition
				where readv/writev are not supported */
#endif /* HAVE_WIN_SCATTER_GATHER_IO */
};

typedef struct os_vio_struct	os_vio_t;

/***********************************************************************//**
Return available space in bytes for a vectored IO struct.
@return: available bytes. */
UNIV_INLINE
ulint
os_vio_get_free_space(
/*==================*/
	const os_vio_t*	vio)	/*!< in: vio struct. */
	__attribute__((nonnull, warn_unused_result));
/**********************************************************************//**
Resets a vio array. */
UNIV_INTERN
void
os_vio_reset(
/*=========*/
	os_vio_t*	vio,	/*!< in/out: the vio struct to reset */
	ulint		type,	/*!< in: OS_FILE_READV or
				OS_FILE_WRITEV */
	os_file_t	fh,	/*!< in: file handle */
	os_offset_t	offset)	/*!< in: offset where to perform IO */
	__attribute__((nonnull));
/**********************************************************************//**
Initializes a vio array. The returned os_vio_t struct must be freed by
calling os_vio_free. The caller must not tweak with the returned struct
directly. Instead calls must be made to os_vio_* functions.
@return	pointer to a vio struct. */
UNIV_INTERN
os_vio_t*
os_vio_init(
/*========*/
	ulint		type,	/*!< in: OS_FILE_READV or
				OS_FILE_WRITEV */
	os_file_t	fh,	/*!< in: file handle */
	os_offset_t	offset,	/*!< in: offset where to perform IO */
	ulint		size);	/*!< in: size in bytes */
/**********************************************************************//**
Frees up a vio array that has been allocated by os_vio_init. */
UNIV_INTERN
void
os_vio_free(
/*========*/
	os_vio_t*	vio);	/*!< in/out: the vio struct to free */
/**********************************************************************//**
Adds a vectored IO request to the iov.
Note that access to vio and the buffer where IO operation is to be
performed must be controlled by the caller. */
UNIV_INTERN
void
os_vio_add_to_batch(
/*================*/
	os_vio_t*	vio,	/*!< in/out: the vio struct to free */
	ulint		type,	/*!< in: OS_FILE_READV or
				OS_FILE_WRITEV */
	byte*		buf,	/*!< in: buffer where to perform the
				IO operation. */
	ulint		size)	/*!< in: size in bytes */
	__attribute__((nonnull));
/**********************************************************************//**
Performs vectored IO on the requests that have been submitted by calling
os_vio_add_to_batch(). The IO operation is synchronous.
Note that access to vio and the buffer where IO operation is to be
performed must be controlled by the caller. */
UNIV_INTERN
void
os_vio_do_io(
/*=========*/
	os_vio_t*	vio)	/*!< in/out: the vio struct */
	__attribute__((nonnull));
#ifndef UNIV_NONINL
#include "os0vio.ic"
#endif

#endif
