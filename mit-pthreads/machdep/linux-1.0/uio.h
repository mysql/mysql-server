/* ==== uio.h ============================================================
 * Copyright (c) 1994 by Chris Provenzano, proven@athena.mit.edu	
 *
 * Description : Correct Linux header file.
 */

#ifndef _PTHREAD_UIO_H_
#define _PTHREAD_UIO_H_

struct iovec {
	void 	*iov_base;
	size_t	iov_len;
};

#endif
