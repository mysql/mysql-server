/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <assert.h>
#include <db_cxx.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    int r;

    DbEnv env(DB_CXX_NO_EXCEPTIONS);
    r = env.set_flags(0, 0); assert(r == 0);
    r = env.set_flags(0, 1); assert(r == 0);
    return 0;
}
