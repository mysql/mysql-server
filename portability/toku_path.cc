/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "toku_path.h"
#include <toku_assert.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>

const char *toku_test_filename(const char *default_filename) {
    const char *filename = getenv("TOKU_TEST_FILENAME");
    if (filename == nullptr) {
        filename = basename((char *) default_filename);
        assert(filename != nullptr);
    }
    return filename;
}

char *toku_path_join(char *dest, int n, const char *base, ...) {
    static const char PATHSEP = '/';
    char *end = stpncpy(dest, base, TOKU_PATH_MAX);
    va_list ap;
    va_start(ap, base);
    for (int i = 1; end - dest < TOKU_PATH_MAX && i < n; ++i) {
        if (*(end - 1) != PATHSEP) {
            *(end++) = PATHSEP;
        }
        const char *next = va_arg(ap, const char *);
        end = stpncpy(end, next, TOKU_PATH_MAX - (end - dest));
    }
    va_end(ap);
    return dest;
}
