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
    assert(argc==3);
    long long lo=parsell(argv[1]);
    long long hi=parsell(argv[2]);
    for (i=lo; i<hi; i++) {
	printf("%lld\t%d\n", i, random());
    }
    return 0;
}

