/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
#include <stdio.h>
#include "memory.h"

int main(void) {
    toku_memory_startup();
    LOCAL_MEMORY_STATUS_S s;
    toku_memory_get_status(&s);
    printf("mallocator: %s\n", s.mallocator_version);
    printf("mmap threshold: %" PRIu64 "\n", s.mmap_threshold);
    toku_memory_shutdown();
    return 0;
}
