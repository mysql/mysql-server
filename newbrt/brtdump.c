/* Tell me the diff between two brt files. */
#include <assert.h>
#include <fcntl.h>

#include "key.h"
#include "brt-internal.h"

void dump_header (int f) {
    struct brt_header *h;
    int r;
    r = toku_deserialize_brtheader_from (f, 0, &h); assert(r==0);
    printf("brtheader:\n");
    printf(" dirty=%d\n", h->dirty);
    printf(" nodesize=%d\n", h->nodesize);
    printf(" freelist=%lld\n", h->freelist);
    printf(" unused_memory=%lld\n", h->unused_memory);
    printf(" unnamed_root=%lld\n", h->unnamed_root);
}

int main (int argc, const char *argv[]) {
    assert(argc==2);
    const char *n = argv[1];
    int f = open(n, O_RDONLY);  assert(f>=0);
    dump_header(f);
    return 0;
}
