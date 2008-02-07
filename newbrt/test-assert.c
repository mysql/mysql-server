#include "toku_assert.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main (int argc, const char *argv[]) {
    if (argc!=2) { printf("argcount should be 2.\n");  exit(1); }
    const char *str=argv[1];
    toku_assert(strcmp(str,"ok")==0);
    return 0;
}
