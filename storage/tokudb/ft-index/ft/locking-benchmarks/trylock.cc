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
#define _MULTI_THREADED
#include <pthread.h>
#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include <unistd.h>

float tdiff (struct timeval *start, struct timeval *end) {
    return 1e6*(end->tv_sec-start->tv_sec) +(end->tv_usec - start->tv_usec);
}

/* Simple function to check the return code and exit the program
   if the function call failed
   */
static void compResults(char *string, int rc) {
  if (rc) {
    printf("Error on : %s, rc=%d",
           string, rc);
    exit(EXIT_FAILURE);
  }
  return;
}

pthread_rwlock_t       rwlock = PTHREAD_RWLOCK_INITIALIZER;

void *rdlockThread(void *arg __attribute__((unused)))
{
  int             rc;
  int             count=0;

  struct timeval start, end;

  printf("Entered thread, getting read lock with mp wait\n");
  Retry:

  gettimeofday(&start, 0);
  rc = pthread_rwlock_tryrdlock(&rwlock);
  gettimeofday(&end, 0);
  printf("pthread_rwlock_tryrdlock took %9.3fus\n", tdiff(&start,&end));
  if (rc == EBUSY) {
    if (count >= 10) {
      printf("Retried too many times, failure!\n");

      exit(EXIT_FAILURE);
    }
    ++count;
    printf("Could not get lock, do other work, then RETRY...\n");
    sleep(1);
    goto Retry;
  }
  compResults("pthread_rwlock_tryrdlock() 1\n", rc);

  sleep(2);

  printf("unlock the read lock\n");
  gettimeofday(&start, 0);
  rc = pthread_rwlock_unlock(&rwlock);
  gettimeofday(&end, 0);
  compResults("pthread_rwlock_unlock()\n", rc);
  printf("%lu.%6lu to %lu.%6lu is %9.2f\n", start.tv_sec, start.tv_usec, end.tv_sec, end.tv_usec, tdiff(&start, &end));

  printf("Secondary thread complete\n");
  return NULL;
}

int main(int argc __attribute__((unused)), char **argv)
{
  int                   rc=0;
  pthread_t             thread;
  struct timeval start, end;

  printf("Enter Testcase - %s\n", argv[0]);

  gettimeofday(&start, 0);
  gettimeofday(&end, 0);
  printf("nop Took %9.2f\n", tdiff(&start, &end));

  {
      int N=1000;
      int i;
      printf("Main, get and release the write lock %d times\n", N);
      gettimeofday(&start, 0);
      for (i=0; i<N; i++) {
	  rc = pthread_rwlock_wrlock(&rwlock);
	  rc = pthread_rwlock_unlock(&rwlock);
      }
      gettimeofday(&end, 0);
      compResults("pthread_rwlock_wrlock()\n", rc);
      printf("Took %9.2fns/op\n", 1000*tdiff(&start, &end)/N);
  }

  printf("Main, get the write lock\n");
  gettimeofday(&start, 0);
  rc = pthread_rwlock_wrlock(&rwlock);
  gettimeofday(&end, 0);
  compResults("pthread_rwlock_wrlock()\n", rc);
  printf("Took %9.2f\n", tdiff(&start, &end));

  printf("Main, create the try read lock thread\n");
  rc = pthread_create(&thread, NULL, rdlockThread, NULL);
  compResults("pthread_create\n", rc);

  printf("Main, wait a bit holding the write lock\n");
  sleep(5);

  printf("Main, Now unlock the write lock\n");
  gettimeofday(&start, 0);
  rc = pthread_rwlock_unlock(&rwlock);
  gettimeofday(&end, 0);
  compResults("pthread_rwlock_unlock()\n", rc);
  printf("Took %9.2f\n", tdiff(&start, &end));

  printf("Main, wait for the thread to end\n");
  rc = pthread_join(thread, NULL);
  compResults("pthread_join\n", rc);

  rc = pthread_rwlock_destroy(&rwlock);
  compResults("pthread_rwlock_destroy()\n", rc);
  printf("Main completed\n");
  return 0;
}
