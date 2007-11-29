/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include <stdlib.h>
#include <stdio.h>
int main (int argc, char *argv[]) {
    int i;
    for (i=0; i<1000; i++) {
	printf("%d\t%d\n", random(), random());
    }
    return 0;
}
