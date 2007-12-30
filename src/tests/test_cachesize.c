/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <db.h>

#include "test.h"

void expect_eq(u_int64_t a, u_int64_t b) {
    if (a != b)
        printf("WARNING: expect %" PRId64 " got %" PRId64 "\n", a, b);
}
 
u_int64_t size_from(u_int32_t gbytes, u_int32_t bytes) {
    return ((u_int64_t)gbytes << 30) + bytes;
}

void size_to(u_int64_t s, u_int32_t *gbytes, u_int32_t *bytes) {
    *gbytes = s >> 30;
    *bytes = s & ((1<<30) - 1);
}

void test_cachesize() {
#if DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR >= 3
    int r;
    DB_ENV *env;
    u_int32_t gbytes, bytes; int ncache;

    r = db_env_create(&env, 0); assert(r == 0);
    r = env->get_cachesize(env, &gbytes, &bytes, &ncache); assert(r == 0);
    if (verbose) printf("default %u %u %d\n", gbytes, bytes, ncache);

    r = env->set_cachesize(env, 0, 0, 1); assert(r == 0);
    r = env->get_cachesize(env, &gbytes, &bytes, &ncache); assert(r == 0);
    if (verbose) printf("minimum %u %u %d\n", gbytes, bytes, ncache);
    u_int64_t minsize = size_from(gbytes, bytes);

    u_int64_t s = 1; size_to(s, &gbytes, &bytes);
    while (gbytes <= 32) {
        r = env->set_cachesize(env, gbytes, bytes, ncache); 
        if (r != 0) {
            if (verbose) printf("max %u %u\n", gbytes, bytes);
            break;
        }
        assert(r == 0);
        r = env->get_cachesize(env, &gbytes, &bytes, &ncache); assert(r == 0);
        assert(ncache == 1);
        if (s <= minsize)
            expect_eq(minsize, size_from(gbytes, bytes));
        else
            expect_eq(s, size_from(gbytes, bytes));
        s *= 2; size_to(s, &gbytes, &bytes);
    }
    r = env->close(env, 0); assert(r == 0);
#endif
}


int main(int argc, const char *argv[]) {
    parse_args(argc, argv);
    test_cachesize();

    return 0;
}
