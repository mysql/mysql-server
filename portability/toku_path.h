/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#ifndef PORTABILITY_TOKU_PATH_H
#define PORTABILITY_TOKU_PATH_H

#include <stdarg.h>
#include <limits.h>

__attribute__((nonnull))
const char *toku_test_filename(const char *default_filename);

#define TOKU_TEST_FILENAME toku_test_filename(__FILE__)

#define TOKU_PATH_MAX PATH_MAX

char *toku_path_join(char *dest, int n, const char *base, ...);
// Effect:
//  Concatenate all the parts into a filename, using portable path separators.
//  Store the result in dest.
// Requires:
//  dest is a buffer of size at least TOKU_PATH_MAX + 1.
//  There are n path components, including base.
// Returns:
//  dest (useful for chaining function calls)

#endif // PORTABILITY_TOKU_PATH_H
