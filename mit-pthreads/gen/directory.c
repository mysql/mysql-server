/*
 * Copyright (c) 1983 Regents of the University of California.
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
static char sccsid[] = "@(#)closedir.c	5.9 (Berkeley) 2/23/91";
#endif /* LIBC_SCCS and not lint */

/*
 * One of these structures is malloced to describe the current directory
 * position each time telldir is called. It records the current magic 
 * cookie returned by getdirentries and the offset within the buffer
 * associated with that return value.
 */
struct ddloc {
	struct	ddloc *loc_next;/* next structure in list */
	long	loc_index;	/* key associated with structure */
	long	loc_seek;	/* magic cookie returned by getdirentries */
	long	loc_loc;	/* offset of entry in buffer */
};

static long	dd_loccnt = 0;	/* Index of entry for sequential telldir's */

#include <errno.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/param.h>
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>


/*
 * close a directory.
 */
int closedir(DIR * dirp)
{
	void *ptr, *nextptr;
	int fd;

	pthread_mutex_lock (dirp->dd_lock);
	fd = dirp->dd_fd;
	dirp->dd_fd = -1;
	dirp->dd_loc = 0;
	for (ptr = (void *)dirp->dd_ddloc; ptr; ptr = nextptr) {
		nextptr = (void *)(((struct ddloc *)ptr)->loc_next);
		free(ptr);
	}
	for (ptr = (void *)dirp->dd_dp; ptr; ptr = nextptr) {
		nextptr = (void *)(((struct __dirent *)ptr)->next);
		free(ptr);
	}
	free((void *)dirp->dd_buf);
	free (dirp->dd_lock);
	free((void *)dirp);
	return(machdep_sys_close(fd));
}

/*
 * open a directory.
 */
DIR * opendir(const char * name)
{
	DIR *dirp;
	int fd;

	if ((fd = machdep_sys_open(name, 0)) < 0)
		return NULL;
	if (machdep_sys_fcntl(fd, F_SETFD, 1) < 0 ||
	    (dirp = (DIR *)malloc(sizeof(DIR))) == NULL) {
		machdep_sys_close (fd);
		return NULL;
	}
	dirp->dd_lock = (pthread_mutex_t*) malloc (sizeof (pthread_mutex_t));
	pthread_mutex_init (dirp->dd_lock, 0);
	/*
	 * If CLSIZE is an exact multiple of DIRBLKSIZ, use a CLSIZE
	 * buffer that it cluster boundary aligned.
	 * Hopefully this can be a big win someday by allowing page trades
	 * to user space to be done by getdirentries()
	 */
#ifndef CLSIZE
#define CLSIZE 1
#endif
	if ((CLSIZE % DIRBLKSIZ) == 0) {
		dirp->dd_buf = malloc(CLSIZE);
		dirp->dd_len = CLSIZE;
	} else {
		dirp->dd_buf = malloc(DIRBLKSIZ);
		dirp->dd_len = DIRBLKSIZ;
	}
	if (dirp->dd_buf == NULL) {
		machdep_sys_close (fd);
		free((void *)dirp);
		return NULL;
	}

	dirp->dd_ddloc = NULL;
	dirp->dd_dp = NULL;
	dirp->dd_seek = 0;
	dirp->dd_loc = 0;
	dirp->dd_fd = fd;
	return(dirp);
}

/*
 * The real work in gettint the next entry in a directory.
 * Return
 * NULL on End of directory
 * &ERR	on Error
 * dp	on valid directory;
 */
static struct dirent ERR;
static struct dirent * readdir_basic(DIR * dirp)
{
	register struct dirent *dp;

	for (;;) {
		if (dirp->dd_loc == 0) {
			dirp->dd_size = machdep_sys_getdirentries(dirp->dd_fd,
			    dirp->dd_buf, dirp->dd_len, &dirp->dd_seek);
			if (dirp->dd_size < 0)
				return(&ERR);
			if (dirp->dd_size == 0)
				return(NULL);
		}
		if (dirp->dd_loc >= dirp->dd_size) {
			dirp->dd_loc = 0;
			continue;
		}
		dp = (struct dirent *)(dirp->dd_buf + dirp->dd_loc);
		if ((long)dp & 03)	/* bogus pointer check */
			return(&ERR);
		if (dp->d_reclen <= 0 ||
		    dp->d_reclen > dirp->dd_len + 1 - dirp->dd_loc)
			return(&ERR);
		dirp->dd_loc += dp->d_reclen;
		if (dp->d_ino == 0)
			continue;
		return(dp);
	}
}

/*
 * POSIX.1 version of getting the next entry in a directory.
 */
struct dirent * readdir(DIR * dirp)
{
	register struct dirent * rp;
	struct __dirent * my__dp;
	pthread_t self;

	pthread_mutex_lock (dirp->dd_lock);

	self = pthread_self();
	/* Allocate space and return */
	for (my__dp = dirp->dd_dp; my__dp; my__dp = my__dp->next) {
		if (pthread_equal(my__dp->owner, self)) {
			break;
		}
	}
	if (my__dp == NULL) {
		if (my__dp = (struct __dirent *)(malloc(sizeof(struct __dirent)))) {
			my__dp->next = dirp->dd_dp;
			dirp->dd_dp = my__dp;
			my__dp->owner = self;
		} else {
			pthread_mutex_unlock (dirp->dd_lock);
			return(NULL);
		}
	}
	if (rp = readdir_basic(dirp)) {
		if (rp != &ERR) {
			memcpy(& (my__dp->data), rp, sizeof(struct dirent));
			rp = & (my__dp->data);
		} else {
			rp = NULL;
		}
	}
	pthread_mutex_unlock (dirp->dd_lock);
	return(rp);
}

/*
 * POSIX.4a version of getting the next entry in a directory.
 */
int readdir_r(DIR * dirp, struct dirent * entry, struct dirent ** result)
{
	register struct dirent * rp;
	int ret;

	pthread_mutex_lock (dirp->dd_lock);
	rp = readdir_basic(dirp);
	if (rp != &ERR) {
		if (rp) {
			memcpy(entry, rp, sizeof(struct dirent));
			*result = entry;
			ret = 0;
		} else {
			*result = NULL;
			ret = 0;
		}
	} else {
		/* Should get it from errno */
		ret = EBADF;
	}
	pthread_mutex_unlock (dirp->dd_lock);
	return(ret);
}

void rewinddir(DIR * dirp)
{
	pthread_mutex_lock (dirp->dd_lock);
	(void)machdep_sys_lseek(dirp->dd_fd, 0, 0);
	dirp->dd_seek = 0;
	dirp->dd_loc = 0;
	pthread_mutex_unlock (dirp->dd_lock);
}

/*
 * Seek to an entry in a directory.
 * _seekdir is in telldir.c so that it can share opaque data structures.
 *
 * Use the POSIX reentrant safe readdir_r to simplify varifying POSIX
 * thread-safe compliance.
 */
void seekdir(DIR * dirp, long loc)
{
  register struct ddloc ** prevlp;
  register struct ddloc * lp;
  struct dirent * dp;
  struct dirent   de;

  pthread_mutex_lock (dirp->dd_lock);
  prevlp = (struct ddloc **)&(dirp->dd_ddloc);
  lp = *prevlp;
  while (lp != NULL) {
    if (lp->loc_index == loc)
      break;
    prevlp = &lp->loc_next;
    lp = lp->loc_next;
  }
  if (lp) {
    if (lp->loc_seek != dirp->dd_seek) {
      if (machdep_sys_lseek(dirp->dd_fd, lp->loc_seek, 0) < 0) {
	*prevlp = lp->loc_next;
	pthread_mutex_unlock (dirp->dd_lock);
	return;
      }
      dirp->dd_seek = lp->loc_seek;
      dirp->dd_loc = 0;
      while (dirp->dd_loc < lp->loc_loc) {
	if (readdir_r(dirp, &de, &dp)) {
	  *prevlp = lp->loc_next;
	  break;
	}
      }
    }
  }
  pthread_mutex_unlock (dirp->dd_lock);
}

/*
 * return a pointer into a directory
 */
long telldir(DIR *dirp)
{
	struct ddloc *lp, **fakeout;
	int ret;

	pthread_mutex_lock (dirp->dd_lock);
	if (lp = (struct ddloc *)malloc(sizeof(struct ddloc))) {
		lp->loc_index = dd_loccnt++;
		lp->loc_seek = dirp->dd_seek;
		lp->loc_loc = dirp->dd_loc;
		lp->loc_next = dirp->dd_ddloc;

		/* Compiler won't let us change anything pointed to by db directly */
		/* So we fake to the left and do it anyway */
		/* Wonder if the compile optomizes it to the correct solution */
		fakeout = (struct ddloc **)&(dirp->dd_ddloc);
		*fakeout = lp;

		ret = lp->loc_index;
	} else {
		ret = -1;
	}
	pthread_mutex_unlock (dirp->dd_lock);
	return(ret);
}

