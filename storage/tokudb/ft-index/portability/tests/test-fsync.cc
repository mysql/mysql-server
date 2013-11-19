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
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include "test.h"
#include "toku_time.h"
#include <portability/toku_path.h>


int verbose = 0;
static void
create_files(int N, int fds[/*N*/]) {
    int r;
    int i;
    char name[30];
    for (i = 0; i < N; i++) {
        snprintf(name, sizeof(name), "%d", i);
        fds[i] = open(name, O_CREAT|O_WRONLY, 0644);
        if (fds[i] < 0) {
            r = get_error_errno();
            CKERR(r);
        }
    }
}

static void
write_to_files(int N, int bytes, int fds[/*N*/]) {
    char junk[bytes];

    int i;
    for (i = 0; i < bytes; i++) {
        junk[i] = random() & 0xFF;
    }

    int r;
    for (i = 0; i < N; i++) {
        r = toku_os_write(fds[i], junk, bytes);
        CKERR(r);
    }
}

static void
time_many_fsyncs_one_file(int N, int bytes, int fds[/*N*/]) {
    if (verbose>1) {
        printf("Starting %s\n", __FUNCTION__);
        fflush(stdout);
    }
    struct timeval begin;
    struct timeval after_first;
    struct timeval end;
    write_to_files(1, bytes, fds);
    if (verbose>1) {
        printf("Done writing to os buffers\n");
        fflush(stdout);
    }
    int i;
    int r;

    r = gettimeofday(&begin, NULL);
    CKERR(r);
    r = fsync(fds[0]);
    CKERR(r);
    r = gettimeofday(&after_first, NULL);
    CKERR(r);
    for (i = 0; i < N; i++) {
        r = fsync(fds[0]);
        CKERR(r);
    }
    r = gettimeofday(&end, NULL);
    CKERR(r);

    if (verbose) {
        printf("Fsyncing one file %d times:\n"
               "\tFirst fsync took: [%f] seconds\n"
               "\tRemaining %d fsyncs took additional: [%f] seconds\n"
               "\tTotal time [%f] seconds\n",
               N + 1,
               toku_tdiff(&after_first, &begin),
               N, 
               toku_tdiff(&end, &after_first),
               toku_tdiff(&end, &begin));
        fflush(stdout);
    }
}

static void
time_fsyncs_many_files(int N, int bytes, int fds[/*N*/]) {
    if (verbose>1) {
        printf("Starting %s\n", __FUNCTION__);
        fflush(stdout);
    }
    write_to_files(N, bytes, fds);
    if (verbose>1) {
        printf("Done writing to os buffers\n");
        fflush(stdout);
    }
    struct timeval begin;
    struct timeval after_first;
    struct timeval end;
    int i;
    int r;

    r = gettimeofday(&begin, NULL);
    CKERR(r);
    for (i = 0; i < N; i++) {
        r = fsync(fds[i]);
        CKERR(r);
        if (i==0) {
            r = gettimeofday(&after_first, NULL);
            CKERR(r);
        }
        if (verbose>2) {
            printf("Done fsyncing %d\n", i);
            fflush(stdout);
        }
    }
    r = gettimeofday(&end, NULL);
    CKERR(r);
    if (verbose) {
        printf("Fsyncing %d files:\n"
               "\tFirst fsync took: [%f] seconds\n"
               "\tRemaining %d fsyncs took additional: [%f] seconds\n"
               "\tTotal time [%f] seconds\n",
               N,
               toku_tdiff(&after_first, &begin),
               N-1,
               toku_tdiff(&end, &after_first),
               toku_tdiff(&end, &begin));
        fflush(stdout);
    }
}

#if !TOKU_WINDOWS
//sync() does not appear to have an analogue on windows.
static void
time_sync_fsyncs_many_files(int N, int bytes, int fds[/*N*/]) {
    if (verbose>1) {
        printf("Starting %s\n", __FUNCTION__);
        fflush(stdout);
    }
    //TODO: timing
    write_to_files(N, bytes, fds);
    if (verbose>1) {
        printf("Done writing to os buffers\n");
        fflush(stdout);
    }
    int i;
    int r;
    struct timeval begin;
    struct timeval after_sync;
    struct timeval end;

    r = gettimeofday(&begin, NULL);
    CKERR(r);

    sync();

    r = gettimeofday(&after_sync, NULL);
    CKERR(r);
    if (verbose>1) {
        printf("Done with sync()\n");
        fflush(stdout);
    }

    for (i = 0; i < N; i++) {
        r = fsync(fds[i]);
        CKERR(r);
        if (verbose>2) {
            printf("Done fsyncing %d\n", i);
            fflush(stdout);
        }
    }
    r = gettimeofday(&end, NULL);
    CKERR(r);

    if (verbose) {
        printf("sync() then fsyncing %d files:\n"
               "\tsync() took: [%f] seconds\n"
               "\tRemaining %d fsyncs took additional: [%f] seconds\n"
               "\tTotal time [%f] seconds\n",
               N,
               toku_tdiff(&after_sync, &begin),
               N,
               toku_tdiff(&end, &after_sync),
               toku_tdiff(&end, &begin));
        fflush(stdout);
    }
}
#endif

int test_main(int argc, char *const argv[]) {
    int i;
    int r;
    int N = 1000;
    int bytes = 4096;
    for (i=1; i<argc; i++) {
        if (strcmp(argv[i], "-v") == 0) {
            if (verbose < 0) verbose = 0;
            verbose++;
            continue;
        }
        if (strcmp(argv[i], "-q") == 0) {
            verbose = 0;
            continue;
        }
        if (strcmp(argv[i], "-b") == 0) {
            i++;
            if (i>=argc) exit(1);
            bytes = atoi(argv[i]);
            if (bytes <= 0)  exit(1);
            continue;
        }
        if (strcmp(argv[i], "-n") == 0) {
            i++;
            if (i>=argc) exit(1);
            N = atoi(argv[i]);
            if (N <= 0)  exit(1);
            continue;
        }
    }
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    r = toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO);
    CKERR(r);
    r = chdir(TOKU_TEST_FILENAME);
    CKERR(r);

    int fds[N];
    create_files(N, fds);

    time_many_fsyncs_one_file(N, bytes, fds);
    time_fsyncs_many_files(N, bytes, fds);
#if !TOKU_WINDOWS
    time_sync_fsyncs_many_files(N, bytes, fds);
#endif

    return 0;
}
