/* ==== writev.c ============================================================
 * Copyright (c) 1995 by Chris Provenzano, proven@mit.edu
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *  This product includes software developed by Chris Provenzano.
 * 4. The name of Chris Provenzano may not be used to endorse or promote 
 *	  products derived from this software without specific prior written
 *	  permission.
 *
 * THIS SOFTWARE IS PROVIDED BY CHRIS PROVENZANO ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL CHRIS PROVENZANO BE LIABLE FOR ANY 
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE.
 *
 * Description : Implementation of writev().
 *
 *  1.00 95/06/19 proven
 *      -Started coding this file.
 */

#ifndef lint
static const char rcsid[] = "$Id$";
#endif

#include "config.h"

#ifndef HAVE_SYSCALL_WRITEV

#include <errno.h>
#include <unistd.h>
#include <sys/uio.h>
#include <sys/types.h>

/* ==========================================================================
 * machdep_sys_writev()
 *
 * modified from the GNU C Library posix/writev.c
 */
int machdep_sys_writev(int fd, struct iovec * vector, int count)
{
	size_t bytes, i;
	char *buffer;
	int ret;

	/* Find the total number of bytes to be written.  */
	for (bytes = 0, i = 0; i < count; ++i)
  		bytes += vector[i].iov_len;

	if (bytes) {
		/*
		 * Allocate a temporary buffer to hold the data.
		 * Don't use alloca because threads tend to have smaller stacks.
		 */
		if ((buffer = (char *)malloc(bytes)) == NULL) {
			return(-ENOMEM);
		}
		/* Copy the data from memory specified by VECTOR to BUFFER */
		for (ret = 0, i = 0; i < count; ++i) {
       		memcpy(buffer + ret, vector[i].iov_base, vector[i].iov_len);
			ret += vector[i].iov_len;
    	}
	} else {
		buffer = NULL;
	}

	ret = (int)machdep_sys_write(fd, buffer, bytes);
	if (buffer)
		free(buffer);
	return(ret);
}

#endif
