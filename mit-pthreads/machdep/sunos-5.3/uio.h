/* ==== uio.h ============================================================
 * Copyright (c) 1994 by Chris Provenzano, proven@athena.mit.edu	
 *
 * Description : Correct Solaris header file.
 */

#ifndef _PTHREAD_UIO_H_
#define _PTHREAD_UIO_H_

#include <sys/cdefs.h>

struct iovec {
	void 	*iov_base;
	size_t	iov_len;
};

typedef struct iovec iovec_t;

struct uio {
	iovec_t *uio_iov;   /* pointer to array of iovecs */
	int uio_iovcnt; /* number of iovecs */
	/* These are all bogus */
	int _uio_offset;    /* file offset */
	int uio_segflg;   /* address space (kernel or user) */
	short   uio_fmode;  /* file mode flags */
	int _uio_limit;   /* u-limit (maximum "block" offset) */
    int uio_resid;  /* residual count */
};

typedef struct uio uio_t;

__BEGIN_DECLS

int 		readv				__P_((int, const struct iovec *, int)); 
int 		writev				__P_((int, const struct iovec *, int));

__END_DECLS

#endif

