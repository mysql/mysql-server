/* ==== uio.h ============================================================
 * Copyright (c) 1994 by Chris Provenzano, proven@athena.mit.edu	
 *
 * Description : Correct HP-UX header file.
 */

#ifndef _PTHREAD_UIO_H_
#define _PTHREAD_UIO_H_

#include <sys/cdefs.h>

struct iovec {
	void 	*iov_base;
	size_t	iov_len;
};

__BEGIN_DECLS

int 		readv				__P_((int, const struct iovec *, int)); 
int 		writev				__P_((int, const struct iovec *, int));

__END_DECLS

#endif

