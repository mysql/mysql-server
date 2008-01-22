/* Tell me the diff between two brt files. */
#include <assert.h>
#include <fcntl.h>

#include "key.h"
#include "brt-internal.h"

void dump_header (int f, struct brt_header **header) {
    struct brt_header *h;
    int r;
    r = toku_deserialize_brtheader_from (f, 0, &h); assert(r==0);
    printf("brtheader:\n");
    printf(" dirty=%d\n", h->dirty);
    printf(" nodesize=%d\n", h->nodesize);
    printf(" freelist=%lld\n", h->freelist);
    printf(" unused_memory=%lld\n", h->unused_memory);
    printf(" unnamed_root=%lld\n", h->unnamed_root);
    printf(" n_named_roots=%d\n", h->n_named_roots);
    if (h->n_named_roots>=0) {
	int i;
	for (i=0; i<h->n_named_roots; i++) {
	    printf("  %s -> %lld\n", h->names[i], h->roots[i]);
	}
    }
    printf(" flags=%d\n", h->flags);
    *header = h;
}

void dump_node (int f, DISKOFF off, struct brt_header *h) {
    BRTNODE n;
    int r = toku_deserialize_brtnode_from (f, off, &n, h->flags, h->nodesize,
					   toku_default_compare_fun, toku_default_compare_fun,
					   (DB*)0, (FILENUM){0});
    assert(r==0);
    
}

int main (int argc, const char *argv[]) {
    assert(argc==2);
    const char *n = argv[1];
    int f = open(n, O_RDONLY);  assert(f>=0);
    struct brt_header *h;
    dump_header(f, &h);
    dump_node(f, 1<<20, h);
    return 0;
}
