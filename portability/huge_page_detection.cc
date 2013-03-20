/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id: memory.cc 52238 2013-01-18 20:21:22Z zardosht $"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."

#include <sys/mman.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <toku_assert.h>

#include "huge_page_detection.h"

extern "C" {

static bool check_huge_pages_config_file(const char *fname)
// Effect: Return true if huge pages are there.  If so, print diagnostics.
{
    FILE *f=fopen(fname, "r");
    if (f) {
        // It's redhat and the feature appears to be there.  Is it enabled?
        char buf[1000];
        char *r = fgets(buf, sizeof(buf), f);
        assert(r!=NULL);
        if (strstr(buf, "[always]")) {
            fprintf(stderr, "Transparent huge pages are enabled, according to %s\n", fname);
            return true;
        } else {
            return false;
        }
    }
    return false;
}

/* struct mapinfo { */
/*     void *addr; */
/*     size_t size; */
/* }; */

/* static void* map_it(size_t size, struct mapinfo *mi, int *n_maps) { */
/*     void *r = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0); */
/*     if ((long)r==-1) perror("mmap failed"); */
/*     mi[*n_maps].addr = r; */
/*     mi[*n_maps].size = size; */
/*     (*n_maps)++; */
/*     return r; */
/* } */

static bool check_huge_pages_in_practice(void)
// Effect: Return true if huge pages appear to be defined in practice.
{
    return false;
    const size_t TWO_MB = 2UL*1024UL*1024UL;

    void *first = mmap(NULL, 2*TWO_MB, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if ((long)first==-1) perror("mmap failed");
    {
        int r = munmap(first, 2*TWO_MB);
        assert(r==0);
    }

    void *second_addr = (void*)(((unsigned long)first + TWO_MB) & ~(TWO_MB -1));
    void *second = mmap(second_addr, TWO_MB, PROT_READ|PROT_WRITE, MAP_FIXED|MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if ((long)second==-1) perror("mmap failed");
    assert((long)second%TWO_MB == 0);

    const long pagesize = 4096;
    const long n_pages = TWO_MB/pagesize;
    unsigned char vec[n_pages];
    {
        int r = mincore(second, TWO_MB, vec);
        assert(r==0);
    }
    for (long i=0; i<n_pages; i++) {
        assert(!vec[i]);
    }
    ((char*)second)[0] = 1;
    {
        int r = mincore(second, TWO_MB, vec);
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
}

bool complain_and_return_true_if_huge_pages_are_enabled(void)
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
}
