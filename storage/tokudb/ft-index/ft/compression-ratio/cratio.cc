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
/* Measure the extent to which we can compress a file.
 * Works on version 8. */

#define _XOPEN_SOURCE 500
#include <arpa/inet.h>
#include <toku_assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <zlib.h>
#include <toku_portability.h>


toku_off_t fd_size (int fd) {
    int64_t file_size;
    int r = toku_os_get_file_size(fd, &file_size);
    assert(r==0);
    return file_size;
}

#define NSIZE (1<<20)
unsigned char fbuf[NSIZE];
unsigned char cbuf[NSIZE+500];

void
measure_header (int fd, toku_off_t off, // read header from this offset
		toku_off_t *usize,      // size uncompressed (but not including any padding)
		toku_off_t *csize)      // compressed size
{
    int r;
    r=pread(fd, fbuf, 12, off);
    assert(r==12);
    assert(memcmp(fbuf,"tokudata",8)==0);
    int bsize = toku_dtoh32(*(uint32_t*)(fbuf+8));
    //printf("Bsize=%d\n", bsize);
    (*usize)+=bsize;
    assert(bsize<=NSIZE);
    r=pread(fd, fbuf, bsize, off);
    assert(r==bsize);
    uLongf destLen=sizeof(cbuf);
    r=compress2(cbuf, &destLen,
		fbuf+20, bsize-=20, // skip magic nodesize and version
		1);
    assert(r==Z_OK);
    destLen+=16; // account for the size and magic and version
    //printf("Csize=%ld\n", destLen);
    (*csize)+=destLen;
}

void
measure_node (int fd, toku_off_t off, // read header from this offset
	      toku_off_t *usize,      // size uncompressed (but not including any padding)
	      toku_off_t *csize)      // compressed size
{
    int r;
    r=pread(fd, fbuf, 24, off);
    assert(r==24);
    //printf("fbuf[0..7]=%c%c%c%c%c%c%c%c\n", fbuf[0], fbuf[1], fbuf[2], fbuf[3], fbuf[4], fbuf[5], fbuf[6], fbuf[7]);
    assert(memcmp(fbuf,"tokuleaf",8)==0 || memcmp(fbuf, "tokunode", 8)==0);
    assert(8==toku_dtoh32(*(uint32_t*)(fbuf+8))); // check file version
    int bsize = toku_dtoh32(*(uint32_t*)(fbuf+20));
    //printf("Bsize=%d\n", bsize);
    (*usize)+=bsize;

    assert(bsize<=NSIZE);
    r=pread(fd, fbuf, bsize, off);
    assert(r==bsize);
    uLongf destLen=sizeof(cbuf);
    r=compress2(cbuf, &destLen,
		fbuf+28, bsize-=28, // skip constant header stuff
		1);
    destLen += 24; // add in magic (8), version(4), lsn (8), and size (4).  Actually lsn will be compressed, but ignore that for now.
    assert(r==Z_OK);
    //printf("Csize=%ld\n", destLen);
    (*csize)+=destLen;

}



/* The single argument is the filename to measure. */
int main (int argc, const char *argv[]) {
    assert(argc==2);
    const char *fname=argv[1];
    int fd = open(fname, O_RDONLY);
    assert(fd>=0);
    toku_off_t fsize = fd_size(fd);
    printf("fsize (uncompressed with   padding)=%lld\n", (long long)fsize);

    toku_off_t usize=0, csize=0;
    measure_header(fd, 0, &usize, &csize);

    toku_off_t i;
    for (i=NSIZE; i+24<fsize; i+=NSIZE) {
	measure_node(fd, i, &usize, &csize);
    }

    printf("usize (uncompressed with no padding)=%10lld  (ratio=%5.2f)\n", (long long)usize, (double)fsize/(double)usize);
    printf("csize (compressed)                  =%10lld  (ratio=%5.2f)\n", (long long)csize, (double)fsize/(double)csize);

    close(fd);
    return 0;
}
