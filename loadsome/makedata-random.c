/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>

long long parsell (char *s) {
    char *end;
    errno=0;
    long long r = strtoll(s, &end, 10);
    assert(*end==0 && end!=s && errno==0);
    return r;
}

int main (int argc, char *argv[]) {
    long long i;
    assert(argc==4);
    long long count=parsell(argv[1]);
    long long range=100*parsell(argv[2]);
    long long seed =parsell(argv[3]);
    srandom(seed);
    for (i=0; i<count; i++) {
	printf("%lld\t%ld\n", (random()%range), random());
    }
    return 0;
}

