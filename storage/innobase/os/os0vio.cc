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
@file os/os0vio.cc
Support for vectored IO

Created 2012/02/15 Inaam Rana
*******************************************************/

#include "os0vio.h"

#ifdef UNIV_NONINL
#include "os0vio.ic"
#endif

#include "os0file.h"
#include "ut0mem.h"
#include "srv0srv.h"

/* Maximum size of a single vectored IO request in bytes. */
#define OS_MAX_VIO_SIZE		((UNIV_PAGE_SIZE) * 1024)

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

{
	ut_ad(vio);
	ut_ad(type == OS_FILE_READV || type == OS_FILE_WRITEV);
	ut_ad(vio->init);
	ut_ad(vio->n_elems > 0);
	ut_ad(vio->size > 0);

	vio->type = type;
	vio->fh = fh;
	vio->offset = offset;
	vio->cnt = 0;
	vio->cur_size = 0;

#if defined(HAVE_WIN_SCATTER_GATHER_IO)
# if defined(UNIV_DEBUG)
	for (ulint i = 0; i < vio->n_elems; ++i) {
		vio->iov[i].Buffer = NULL;
	}
# endif /* UNIV_DEBUG */
	vio->iov[0].Buffer = NULL;
	ResetEvent(vio->ol.hEvent);
#else /* defined(__WIN__) */
	UNIV_MEM_INVALID(vio->iov, vio->n_elems * sizeof *vio->iov);
# if !defined(HAVE_VECTORED_IO)
	ut_ad(vio->buf_ua != NULL);
	ut_ad(vio->buf != NULL);
	ut_ad(vio->buf == ut_align(vio->buf_ua, UNIV_PAGE_SIZE));

# else /* !defined(HAVE_VECTORED_IO) */
	ut_ad(vio->buf_ua == NULL);
	ut_ad(vio->buf == NULL);
# endif /* !defined(HAVE_VECTORED_IO) */
#endif /* HAVE_WIN_SCATTER_GATHER_IO */
}

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
	ulint		size)	/*!< in: total size in bytes */
{
	os_vio_t*	vio;

	ut_ad(type == OS_FILE_READV || type == OS_FILE_WRITEV);
	ut_ad(size > 0);
	ut_ad(ut_is_2pow(size));
	ut_ad(size <= OS_MAX_VIO_SIZE);
	ut_ad(size % UNIV_PAGE_SIZE == 0);

	vio = static_cast<os_vio_t*>(ut_malloc(sizeof(os_vio_t)));

	/* We allocate twice the numbers of elements because in case
	of compressed pages we generate two IO requests. One for the
	actual page and the other for the padding. (null_page) */
	vio->n_elems = (size / UNIV_PAGE_SIZE) * 2;
#if defined(HAVE_WIN_SCATTER_GATHER_IO)
	ut_ad(srv_win_sys_page_size);

	/* Need one extra element for NULL */
	vio->n_elems = size / srv_win_sys_page_size + 1;

	vio->iov = static_cast<FILE_SEGMENT_ELEMENT*>(ut_malloc(vio->n_elems
		* sizeof *vio->iov));

	vio->ol.hEvent = CreateEvent(NULL,TRUE, FALSE, NULL);
#else /* defined(HAVE_WIN_SCATTER_GATHER_IO */
	vio->iov = static_cast<struct iovec*>(
		ut_malloc(sizeof *vio->iov * vio->n_elems));
# if !defined(HAVE_VECTORED_IO)
	/* Vectored IO is not supported. In such rare cases we allocate
	a buffer and simulate that we are doing vectored IO to the
	upper layers. */
	vio->buf_ua = static_cast<byte*>(ut_malloc(size + UNIV_PAGE_SIZE));
	vio->buf = static_cast<byte*>(ut_align(vio->buf_ua,
					       UNIV_PAGE_SIZE));
# else /* defined(HAVE_VECTORED_IO) */
	vio->buf_ua = vio->buf = NULL;
# endif /* defined(HAVE_VECTORED_IO) */
#endif /* HAVE_WIN_SCATTER_GATHER_IO */
	vio->init = TRUE;
	vio->size = size;
	os_vio_reset(vio, type, fh, offset);

	return(vio);
}

/**********************************************************************//**
Frees up a vio array that has been allocated by os_vio_init. */
UNIV_INTERN
void
os_vio_free(
/*========*/
	os_vio_t*	vio)	/*!< in/out: the vio struct to free */
{
	ut_ad(vio->init);

#if !defined(HAVE_VECTORED_IO) && !defined(HAVE_WIN_SCATTER_GATHER_IO)
	ut_ad(vio->buf != NULL);
	ut_ad(vio->buf_ua != NULL);
	ut_ad(vio->buf == ut_align(vio->buf_ua, UNIV_PAGE_SIZE));
	ut_free(vio->buf_ua);
#endif /* defined(HAVE_VECTORED_IO) */
	ut_free(vio->iov);
	ut_free(vio);
}

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
{
	ut_ad(vio->init);
	ut_ad(type == vio->type);
	ut_ad(vio->cur_size + size <= vio->size);
	ut_ad(vio->cnt < vio->n_elems);

#if defined(HAVE_WIN_SCATTER_GATHER_IO)
	ulint	i;
	ulint	n_req;

	/* On Windows vectored IO happens in units of system page
	size. */
	/* FIXME: Right now this won't work on windows if the size
	of the page is smaller than the srv_win_sys_page_size. I am
	not sure what is the range of allowable srv_win_sys_page_size
	but if, for example, we are using 1K compressed pages then this
	assert will likely get hit. I have to talk to our windows expert
	to find a reasonable solution for this. */
	ut_ad(size >= srv_win_sys_page_size);
	ut_ad(size % srv_win_sys_page_size == 0);

	/* Each buffer must be at least the size of a system memory
	page and must be aligned on a system memory page size boundary */
	ut_ad(((ulint) buf) % srv_win_sys_page_size == 0);

	n_req = size / srv_win_sys_page_size;

	ut_ad(vio->cnt + n_req <= vio->n_elems);

	for (i = 0; i < n_req; ++i, buf += srv_win_sys_page_size) {
		vio->iov[vio->cnt].Buffer = PtrToPtr64(buf);
		vio->cnt++;
		vio->cur_size += srv_win_sys_page_size;
	}
#else /* HAVE_WIN_SCATTER_GATHER_IO */
	vio->iov[vio->cnt].iov_base = buf;
	vio->iov[vio->cnt].iov_len = size;
# if !defined(HAVE_VECTORED_IO)
	/* Vectored IO is not supported. If we are doing a write
	operation copy the contents over to the vio buffer. */
	if (type == OS_FILE_WRITEV) {
		memcpy(vio->buf + vio->cur_size, buf, size);
	}
# endif /* !defined(HAVE_VECTORED_IO) */
	vio->cnt++;
	vio->cur_size += size;
#endif /* HAVE_WIN_SCATTER_GATHER_IO */
}

#if !defined(__WIN__) && defined(HAVE_VECTORED_IO)
/**********************************************************************//**
Performs vectored write on the requests that have been submitted by calling
os_vio_add_to_batch(). The IO operation is synchronous.
Note that access to vio and the buffer where IO operation is to be
performed must be controlled by the caller.
@return: number of bytes successfully written or -1 on error. */
static
ssize_t
os_vio_writev(
/*==========*/
	os_vio_t*	vio)	/*!< in: the vio struct */
{
	ut_ad(vio->type == OS_FILE_WRITEV);
#if defined(HAVE_PREADV)
	return(pwritev(vio->fh, vio->iov, vio->cnt, (off_t) vio->offset));
#else /* defined(HAVE_PREADV) */
	return(writev(vio->fh, vio->iov, vio->cnt));
#endif /* defined(HAVE_PREADV) */
}

/**********************************************************************//**
Performs vectored read on the requests that have been submitted by calling
os_vio_add_to_batch(). The IO operation is synchronous.
Note that access to vio and the buffer where IO operation is to be
performed must be controlled by the caller.
@return: number of bytes successfully read or -1 on error. */
static
ssize_t
os_vio_readv(
/*==========*/
	os_vio_t*	vio)	/*!< in: the vio struct */
{
	ut_ad(vio->type == OS_FILE_READV);
#if defined(HAVE_PREADV)
	return(preadv(vio->fh, vio->iov, vio->cnt, (off_t) vio->offset));
#else /* defined(HAVE_PREADV) */
	return(readv(vio->fh, vio->iov, vio->cnt));
#endif /* defined(HAVE_PREADV) */
}
#endif /* !defined(__WIN__) && defined(HAVE_VECTORED_IO) */

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
{
	ibool	retry = FALSE;
	int	ret = 0;

	ut_ad(vio->init);
	ut_ad(vio->cur_size <= vio->size);
	ut_ad(vio->cnt <= vio->n_elems);

try_again:
	ret = 0;

#if defined(HAVE_WIN_SCATTER_GATHER_IO)
	BOOL	ret_val;
	DWORD	len;

	vio->ol.Offset = (DWORD) vio->offset & 0xFFFFFFFF;
	vio->ol.OffsetHigh = (DWORD) (vio->offset >> 32);
	ResetEvent(vio->ol.hEvent);

	if (vio->type == OS_FILE_READV) {
		ret_val = ReadFileScatter(vio->fh, vio->iov, vio->cur_size,
					  NULL, &vio->ol);
	} else {
		ut_ad(vio->type == OS_FILE_WRITEV);
		ret_val = WriteFileGather(vio->fh, vio->iov, vio->cur_size,
					  NULL, &vio->ol);
	}

	if (!ret_val && GetLastError() == ERROR_IO_PENDING) {

		/* The IO was queued successfully. Wait for it to
		complete. */
		ret_val = GetOverlappedResult(vio->fh, &vio->ol, &len,
					      TRUE);

		if (ret_val) {
			ut_a(len == vio->cur_size);
		} else {
			goto failure;
		}

	} else if (!ret_val) {
		goto failure;
	}
#elif defined(HAVE_VECTORED_IO)
	off_t	offs;

	/* If off_t is > 4 bytes in size, then we assume we can pass a
	64-bit address */
	offs = (off_t) vio->offset;

	if (sizeof(off_t) <= 4) {
		if (vio->offset != (os_offset_t) offs) {
			fprintf(stderr,
				"InnoDB: Error: file IO at offset > 4 GB\n");
			ut_error;
		}
	}
# ifndef HAVE_PREADV
	ulint	i;
	off_t	ret_offset;

	/* Protect the seek / read operation with a mutex */
	i = ((ulint) vio->fh) % OS_FILE_N_SEEK_MUTEXES;

	os_mutex_enter(os_file_seek_mutexes[i]);

	ret_offset = lseek(vio->fh, offs, SEEK_SET);

	if (ret_offset < 0) {
		os_mutex_exit(os_file_seek_mutexes[i]);
		goto failure;
	}
# endif /* !defined(HAVE_PREADV) */
	if (vio->type == OS_FILE_READV) {
		ret = os_vio_readv(vio);
	} else {
		ut_ad(vio->type == OS_FILE_WRITEV);
		ret = os_vio_writev(vio);
	}
# if !defined(HAVE_PREADV)
	/* Release the mutex protecting lseek(). */
	os_mutex_exit(os_file_seek_mutexes[i]);
# endif /* !defined(HAVE_PREADV) */
	if (ret == -1) {
		goto failure;
	}

	ut_a((ulint)ret == vio->cur_size);
#else /* defined(HAVE_VECTORED_IO) */
	/* No vectored IO present on this platform. Do the normal
	synchronous IO. */
	if (!os_aio_func(vio->type == OS_FILE_READV ? OS_FILE_READ
			 : OS_FILE_WRITE, OS_AIO_SYNC,
			 srv_dblwr_data_file_name, vio->fh, vio->buf,
			 vio->offset, vio->cur_size, NULL, NULL)) {
		goto failure;
	}

	if (vio->type == OS_FILE_READV) {
		/* The data has been read into the vio buffer. We now
		need to copy it where it was requested by the caller. */
		for (ulint i = 0, indx = 0; i < vio->cnt; ++i) {
			memcpy(vio->iov[i].iov_base, vio->buf + indx,
			       vio->iov[i].iov_len);
			indx += vio->iov[i].iov_len;
		}
	}
#endif /* defined(HAVE_VECTORED_IO) */
#if 0
	fprintf(stderr, "VIO: n_req[%lu], ssize[%lu] ret_val[%d]\n",
		vio->cnt, vio->cur_size, ret);
#endif

	os_file_flush(vio->fh);

	os_vio_reset(vio, vio->type, vio->fh, vio->offset);

	return;

failure:
	retry = os_file_handle_error(NULL, vio->type == OS_FILE_READV
				     ? "vio read" : "vio write");
	if (retry) {

		goto try_again;
	}

	ut_error;
}
