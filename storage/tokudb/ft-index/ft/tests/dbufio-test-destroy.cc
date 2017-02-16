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

  TokuFT, Tokutek Fractal Tree Indexing Library.
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
#include "loader/dbufio.h"
#include <stdio.h>
#include <fcntl.h>
#include <toku_assert.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

enum { N  = 5 };
enum { M = 10 };
static void test1 (size_t chars_per_file, size_t UU(bytes_per_read)) {
    int fds[N];
    char fnames[N][100];
    size_t n_read[N];
    int UU(n_live)=N;
    for (int i=0; i<N; i++) {
	snprintf(fnames[i], 100, "dbufio-test-destroy-file%d.data", i);
	unlink(fnames[i]);
	fds[i] = open(fnames[i], O_CREAT|O_RDWR, S_IRWXU);
	//printf("fds[%d]=%d is %s\n", i, fds[i], fnames[i]);
	assert(fds[i]>=0);
	n_read[i]=0;
	for (size_t j=0; j<chars_per_file; j++) {
	    unsigned char c = (i+j)%256;
	    int r = toku_os_write(fds[i], &c, 1);
	    if (r!=0) {
                int er = get_maybe_error_errno();
                printf("fds[%d]=%d r=%d errno=%d (%s)\n", i, fds[i], r, er, strerror(er));
            }
	    assert(r==0);
	}
	{
	    int r = lseek(fds[i], 0, SEEK_SET);
	    assert(r==0);
	}
	
    }
    DBUFIO_FILESET bfs;
    {
	int r = create_dbufio_fileset(&bfs, N, fds, M, false);
	assert(r==0);
    }

    { int r = panic_dbufio_fileset(bfs, EIO); assert(r == 0); }

    {
	int r = destroy_dbufio_fileset(bfs);
	assert(r==0);
    }
    for (int i=0; i<N; i++) {
	{
	    int r = unlink(fnames[i]);
	    assert(r==0);
	}
	{
	    int r = close(fds[i]);
	    assert(r==0);
	}
	assert(n_read[i]==0);
    }
}

				  

int main (int argc __attribute__((__unused__)), char *argv[]__attribute__((__unused__))) {
//    test1(0, 1);
//    test1(1, 1);
//    test1(15, 1);
//    test1(100, 1);
    test1(30, 3); // 3 and M are relatively prime.  But 3 divides the file size.
    return 0;
}
