/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include <test.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <toku_portability.h>
#include <toku_assert.h>
#include <toku_os.h>

static void
check_snprintf(int i) {
    char buf_before[8];
    char target[5];
    char buf_after[8];
    memset(target, 0xFF, sizeof(target));
    memset(buf_before, 0xFF, sizeof(buf_before));
    memset(buf_after, 0xFF, sizeof(buf_after));
    int64_t n = 1;
    
    int j;
    for (j = 0; j < i; j++) n *= 10;

    int bytes = snprintf(target, sizeof target, "%"PRId64, n);
    assert(bytes==i+1 ||
           (i+1>=(int)(sizeof target) && bytes>=(int)(sizeof target)));
    if (bytes>=(int)(sizeof target)) {
        //Overflow prevented by snprintf
        assert(target[sizeof target - 1] == '\0');
        assert(strlen(target)==sizeof target-1);
    }
    else {
        assert(target[bytes] == '\0');
        assert(strlen(target)==(size_t)bytes);
    }
}


int test_main(int argc, char *const argv[]) {
    int i;
    for (i = 0; i < 8; i++) {
        check_snprintf(i);
    }
    return 0;
}

