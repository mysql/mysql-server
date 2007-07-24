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
    long long count=parsell(argv[1]);
    long long range=100*parsell(argv[2]);
    for (i=0; i<count; i++) {
	printf("%lld\t%d\n", (random()%range), random());
    }
    return 0;
}

