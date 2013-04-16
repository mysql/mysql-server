/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#pragma once

constexpr char UU() static_tolower(const char a) {
    return a >= 'A' && a <= 'Z' ? a - 'A' + 'a' : a;
}

constexpr int UU() static_strncasecmp(const char *a, const char *b, size_t len) {
    return len == 0 ? 0 : (
         static_tolower(*a) != static_tolower(*b)  || *a == '\0' ?
         static_tolower(*a) - static_tolower(*b) :
         static_strncasecmp(a+1, b+1, len-1)
        );
}

