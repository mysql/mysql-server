/*
 * Copyright (c) 1988 The Regents of the University of California.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)ttyname.c	5.10 (Berkeley) 5/6/91";
#endif /* LIBC_SCCS and not lint */

#include "config.h"
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

static pthread_mutex_t ttyname_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_key_t ttyname_key;
static int ttyname_init = 0;
extern void free();

char * __ttyname_r_basic(int fd, char * buf, size_t len)
{
	register struct dirent *dirp;
	register DIR *dp;
	struct stat dsb;
	struct stat sb;
	char * rval;
	int minlen;

    rval = NULL;

	/* Must be a terminal. */
	if (! isatty_basic(fd))
		return(rval);
	/* Must be a character device. */
	if (machdep_sys_fstat(fd, &sb) || !S_ISCHR(sb.st_mode))
		return(rval);
	/* Must have enough room */
    if (len <= sizeof(_PATH_PTY)) 
		return(rval);

	if ((dp = opendir(_PATH_PTY)) != NULL) {
		memcpy(buf, _PATH_PTY, sizeof(_PATH_PTY));
		for (rval = NULL; dirp = readdir(dp);) {
			if (dirp->d_fileno != sb.st_ino)
				continue;
			minlen = (len - (sizeof(_PATH_PTY) - 1)) < (dirp->d_namlen + 1) ?
			  (len - (sizeof(_PATH_PTY) - 1)) : (dirp->d_namlen + 1);
			memcpy (buf + sizeof(_PATH_PTY) - 1, dirp->d_name, minlen);
			if (stat(buf, &dsb) || sb.st_dev != dsb.st_dev ||
	    		sb.st_ino != dsb.st_ino)
				continue;
			rval = buf;
			break;
		}
		(void)closedir(dp);
	}
	return(rval);
}

char * __ttyname_basic(int fd)
{
	char *buf;

	pthread_mutex_lock (&ttyname_lock);
	if (ttyname_init == 0) {
		if (pthread_key_create(&ttyname_key, free)) {
			pthread_mutex_unlock (&ttyname_lock);
			return(NULL);
		}
		ttyname_init = 1;
	}
	pthread_mutex_unlock (&ttyname_lock);

	/* Must have thread specific data field to put data */
    if ((buf = pthread_getspecific(ttyname_key)) == NULL) {
		if (buf = malloc(sizeof(_PATH_PTY) + MAXNAMLEN)) {
			if (pthread_setspecific(ttyname_key, buf) != OK) {
				free(buf);
				return(NULL);
			}
		} else {
			return(NULL);
		}
	}
	return(__ttyname_r_basic(fd, buf, sizeof(_PATH_PTY) + MAXNAMLEN));
}

char * ttyname_r(int fd, char * buf, size_t len)
{
	char * ret;

	if (fd_lock(fd, FD_READ) == OK) {
     	ret = __ttyname_r_basic(fd_table[fd]->fd.i, buf, len);
		fd_unlock(fd, FD_READ);
	} else {
		ret = NULL;
	}
	return(ret);
}

char * ttyname(int fd)
{
	char * ret;

	if (fd_lock(fd, FD_READ) == OK) {
     	ret = __ttyname_basic(fd_table[fd]->fd.i);
		fd_unlock(fd, FD_READ);
	} else {
		ret = NULL;
	}
	return(ret);
}


