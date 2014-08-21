/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id: memory.cc 52238 2013-01-18 20:21:22Z zardosht $"
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

#include <sys/mman.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <portability/toku_assert.h>
#include <portability/toku_os.h>

static bool check_huge_pages_config_file(const char *fname)
// Effect: Return true if huge pages are there.  If so, print diagnostics.
{
    bool huge_pages_enabled = false;
    FILE *f = fopen(fname, "r");
    if (f) {
        // It's redhat and the feature appears to be there.  Is it enabled?
        char buf[1000];
        char *r = fgets(buf, sizeof(buf), f);
        assert(r != NULL);
        if (strstr(buf, "[always]")) {
            fprintf(stderr, "Transparent huge pages are enabled, according to %s\n", fname);
            huge_pages_enabled = true;
        } else {
            huge_pages_enabled =false;
        }
        fclose(f);
    }
    return huge_pages_enabled;
}

static bool check_huge_pages_in_practice(void)
// Effect: Return true if huge pages appear to be defined in practice.
{
#ifdef HAVE_MINCORE    
#ifdef HAVE_MAP_ANONYMOUS    
    const int map_anonymous = MAP_ANONYMOUS;
#else
    const int map_anonymous = MAP_ANON;
#endif
    const size_t TWO_MB = 2UL*1024UL*1024UL;

    void *first = mmap(NULL, 2*TWO_MB, PROT_READ|PROT_WRITE, MAP_PRIVATE|map_anonymous, -1, 0);
    if ((long)first==-1) perror("mmap failed");
    {
        int r = munmap(first, 2*TWO_MB);
        assert(r==0);
    }

    void *second_addr = (void*)(((unsigned long)first + TWO_MB) & ~(TWO_MB -1));
    void *second = mmap(second_addr, TWO_MB, PROT_READ|PROT_WRITE, MAP_FIXED|MAP_PRIVATE|map_anonymous, -1, 0);
    if ((long)second==-1) perror("mmap failed");
    assert((long)second%TWO_MB == 0);

    const long pagesize = 4096;
    const long n_pages = TWO_MB/pagesize;
    unsigned char vec[n_pages];
    {
        int r = mincore(second, TWO_MB, vec);
        if (r!=0 && errno==ENOMEM) {
            // On some kernels (e.g., Centos 5.8), mincore doesn't work.  It seems unlikely that huge pages are here.
            munmap(second, TWO_MB);
            return false;
        }
        assert(r==0);
    }
    for (long i=0; i<n_pages; i++) {
        assert(!vec[i]);
    }
    ((char*)second)[0] = 1;
    {
        int r = mincore(second, TWO_MB, vec);
        // If the mincore worked the first time, it probably works here too.x
        assert(r==0);
    }
    assert(vec[0]);
    {
        int r = munmap(second, TWO_MB);
        assert(r==0);
    }
    if (vec[1]) {
        fprintf(stderr, "Transparent huge pages appear to be enabled according to mincore()\n");
        return true;
    } else {
        return false;
    }
#else
    // No mincore, so no way to check this in practice
    return false;
#endif
}

bool toku_os_huge_pages_enabled(void)
// Effect: Return true if huge pages appear to be enabled.  If so, print some diagnostics to stderr.
//  If environment variable TOKU_HUGE_PAGES_OK is set, then don't complain.
{
    char *toku_huge_pages_ok = getenv("TOKU_HUGE_PAGES_OK");
    if (toku_huge_pages_ok) {
        return false;
    } else {
        bool conf1 = check_huge_pages_config_file("/sys/kernel/mm/redhat_transparent_hugepage/enabled");
        bool conf2 = check_huge_pages_config_file("/sys/kernel/mm/transparent_hugepage/enabled");
        bool prac = check_huge_pages_in_practice();
        return conf1|conf2|prac;
    }
}
