/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
/*
COPYING CONDITIONS NOTICE:

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation, and provided that the
  following conditions are met:

      * Redistributions of source code must retain this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below).

      * Redistributions in binary form must reproduce this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below) in the documentation and/or other materials
        provided with the distribution.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.

COPYRIGHT NOTICE:

  TokuDB, Tokutek Fractal Tree Indexing Library.
  Copyright (C) 2007-2013 Tokutek, Inc.

DISCLAIMER:

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

UNIVERSITY PATENT NOTICE:

  The technology is licensed by the Massachusetts Institute of
  Technology, Rutgers State University of New Jersey, and the Research
  Foundation of State University of New York at Stony Brook under
  United States of America Serial No. 11/760379 and to the patents
  and/or patent applications resulting from it.

PATENT MARKING NOTICE:

  This software is covered by US Patent No. 8,185,551.
  This software is covered by US Patent No. 8,489,638.

PATENT RIGHTS GRANT:

  "THIS IMPLEMENTATION" means the copyrightable works distributed by
  Tokutek as part of the Fractal Tree project.

  "PATENT CLAIMS" means the claims of patents that are owned or
  licensable by Tokutek, both currently or in the future; and that in
  the absence of this license would be infringed by THIS
  IMPLEMENTATION or by using or running THIS IMPLEMENTATION.

  "PATENT CHALLENGE" shall mean a challenge to the validity,
  patentability, enforceability and/or non-infringement of any of the
  PATENT CLAIMS or otherwise opposing any of the PATENT CLAIMS.

  Tokutek hereby grants to you, for the term and geographical scope of
  the PATENT CLAIMS, a non-exclusive, no-charge, royalty-free,
  irrevocable (except as stated in this section) patent license to
  make, have made, use, offer to sell, sell, import, transfer, and
  otherwise run, modify, and propagate the contents of THIS
  IMPLEMENTATION, where such license applies only to the PATENT
  CLAIMS.  This grant does not include claims that would be infringed
  only as a consequence of further modifications of THIS
  IMPLEMENTATION.  If you or your agent or licensee institute or order
  or agree to the institution of patent litigation against any entity
  (including a cross-claim or counterclaim in a lawsuit) alleging that
  THIS IMPLEMENTATION constitutes direct or contributory patent
  infringement, or inducement of patent infringement, then any rights
  granted to you under this License shall terminate as of the date
  such litigation is filed.  If you or your agent or exclusive
  licensee institute or order or agree to the institution of a PATENT
  CHALLENGE, then Tokutek may terminate any rights granted to you
  under this License.
*/

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
/* Test pthread rwlocks in multiprocess environment. */

/* How expensive is
 *   - Obtaining a read-only lock for the first obtainer.
 *   - Obtaining it for the second one?
 *   - The third one? */

#include <toku_assert.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>


float tdiff (struct timeval *start, struct timeval *end) {
    return 1e6*(end->tv_sec-start->tv_sec) +(end->tv_usec - start->tv_usec);
}

#define FILE "process.data"

int main (int argc __attribute__((__unused__)), char *argv[] __attribute__((__unused__))) {
    int r;
    int fd;
    void *p;
    fd=open(FILE, O_CREAT|O_RDWR|O_TRUNC, 0666); assert(fd>=0);
    int i;
    for (i=0; i<4096; i++) {
	r=write(fd, "\000", 1);
	assert(r==1);
    }
    p=mmap(0, 4096, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (p==MAP_FAILED) {
	printf("err=%d %s (EPERM=%d)\n", errno, strerror(errno), EPERM);
    }
    assert(p!=MAP_FAILED);
    r=close(fd); assert(r==0);

    pthread_rwlockattr_t attr;
    pthread_rwlock_t *lock=p;
    r=pthread_rwlockattr_init(&attr);                               assert(r==0);
    r=pthread_rwlockattr_setpshared(&attr, PTHREAD_PROCESS_SHARED); assert(r==0);
    r=pthread_rwlock_init(lock, &attr);                             assert(r==0);
    r=pthread_rwlock_init(lock+1, &attr);                             assert(r==0);

    r=pthread_rwlock_wrlock(lock);

    pid_t pid;
    if ((pid=fork())==0) {
	// I'm the child
	r = munmap(p, 4096); assert(r==0);
	fd = open(FILE, O_RDWR, 0666); assert(fd>=0);
	p=mmap(0, 4096, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	assert(p!=MAP_FAILED);
	r=close(fd); assert(r==0);
	
	printf("A0\n");
	r=pthread_rwlock_wrlock(lock);
	printf("C\n");
	sleep(1);
    
	r=pthread_rwlock_unlock(lock);
	printf("D\n");

	r=pthread_rwlock_rdlock(lock);
	printf("E0\n");
	sleep(1);

    } else {
	printf("A1\n");
	sleep(1);
	printf("B\n");
	r=pthread_rwlock_unlock(lock); // release the lock grabbed before the fork
	assert(r==0);

	sleep(1);
	r=pthread_rwlock_rdlock(lock);
	assert(r==0);
	printf("E1\n");
	sleep(1);

	int status;
	pid_t waited=wait(&status);
	assert(waited==pid);
    }
    return 0;    
    
    
#if 0    

    int j;
    int i;
    int r;
    struct timeval start, end;
    for (j=0; j<3; j++) {
	for (i=0; i<K; i++) {
	    r=pthread_rwlock_init(&rwlocks[i], NULL);
	    assert(r==0);
	}
	gettimeofday(&start, 0);
	for (i=0; i<K; i++) {
	    r = pthread_rwlock_tryrdlock(&rwlocks[i]);
	    assert(r==0);
	}
	gettimeofday(&end, 0);
	printf("pthread_rwlock_tryrdlock took %9.3fus for %d ops:  %9.3fus/lock (%9.3fMops/s)\n", tdiff(&start,&end), K, tdiff(&start,&end)/K, K/tdiff(&start,&end));
    }

    for (j=0; j<3; j++) {
	for (i=0; i<K; i++) {
	    r=pthread_rwlock_init(&rwlocks[i], NULL);
	    assert(r==0);
	}
	gettimeofday(&start, 0);
	for (i=0; i<K; i++) {
	    r = pthread_rwlock_rdlock(&rwlocks[i]);
	    assert(r==0);
	}
	gettimeofday(&end, 0);
	printf("pthread_rwlock_rdlock    took %9.3fus for %d ops:  %9.3fus/lock (%9.3fMops/s)\n", tdiff(&start,&end), K, tdiff(&start,&end)/K, K/tdiff(&start,&end));
    }

    for (j=0; j<3; j++) {
	for (i=0; i<K; i++) {
	    blocks[i].state=0;
	    blocks[i].mutex=0;
	}
	gettimeofday(&start, 0);
	for (i=0; i<K; i++) {
	    brwl_rlock(&blocks[i]);
	}
	gettimeofday(&end, 0);
	printf("brwl_rlock               took %9.3fus for %d ops:  %9.3fus/lock (%9.3fMops/s)\n", tdiff(&start,&end), K, tdiff(&start,&end)/K, K/tdiff(&start,&end));
    }
    return 0;
#endif
}
