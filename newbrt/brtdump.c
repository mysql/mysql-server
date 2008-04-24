/* Tell me the diff between two brt files. */
#include <ctype.h>
#include <fcntl.h>
#include <inttypes.h>

#include "toku_assert.h"
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

void print_item (bytevec val, ITEMLEN len) {
    printf("\"");
    ITEMLEN i;
    for (i=0; i<len; i++) {
	unsigned char ch = ((unsigned char*)val)[i];
	if (isprint(ch) && ch!='\\' && ch!='"') {
	    printf("%c", ch);
	} else {
	    printf("\\%03o", ch);
	}
    }
    printf("\"");
}

void dump_node (int f, DISKOFF off) {
    BRTNODE n;
    int r = toku_deserialize_brtnode_from (f, off, &n);
    assert(r==0);
    assert(n!=0);
    printf("brtnode\n");
    printf(" nodesize    =%u\n", n->nodesize);
    printf(" sizeonddisk =%d\n", toku_serialize_brtnode_size(n));
    printf(" flags       =%u\n", n->flags);
    printf(" thisnodename=%lld\n", n->thisnodename);
    printf(" disk_lsn    =%" PRId64 "\n", n->disk_lsn.lsn);
    //printf(" log_lsn     =%lld\n", n->log_lsn.lsn); // The log_lsn is a memory-only value.
    printf(" height      =%d\n",   n->height);
    printf(" layout_version=%d\n", n->layout_version);
    printf(" rand4fp     =%08x\n", n->rand4fingerprint);
    printf(" localfp     =%08x\n", n->local_fingerprint);
    if (n->height>0) {
	printf(" n_children=%d\n", n->u.n.n_children);
	printf(" total_childkeylens=%u\n", n->u.n.totalchildkeylens);
	printf(" n_bytes_in_buffers=%u\n", n->u.n.n_bytes_in_buffers);
	int i;
	printf(" subfingerprints={");
	for (i=0; i<n->u.n.n_children; i++) {
	    if (i>0) printf(" ");
	    printf("%08x", BNC_SUBTREE_FINGERPRINT(n, i));
	}
	printf("}\n");
	printf(" pivots:\n");
	for (i=0; i<n->u.n.n_children-1; i++) {
	    struct kv_pair *piv = n->u.n.childkeys[i];
	    printf("  pivot %d:", i);
	    print_item(kv_pair_key_const(piv), kv_pair_keylen(piv));
	    assert(n->flags==0); // if not zero, we must print the other part of the pivot.
	    printf("\n");
	}
	printf(" children:\n");
	for (i=0; i<n->u.n.n_children; i++) {
	    printf("   child %d: %lld\n", i, BNC_DISKOFF(n, i));
	    printf("   buffer contains %d bytes (%d items)\n", BNC_NBYTESINBUF(n, i), toku_fifo_n_entries(BNC_BUFFER(n,i)));
	    FIFO_ITERATE(BNC_BUFFER(n,i), key, keylen, data, datalen, typ, xid,
			 ({
			     printf("    TYPE=");
			     switch ((enum brt_cmd_type)typ) {
			     case BRT_NONE: printf("NONE"); goto ok;
			     case BRT_INSERT: printf("INSERT"); goto ok;
			     case BRT_DELETE_ANY: printf("DELETE_ANY"); goto ok;
			     case BRT_DELETE_BOTH: printf("DELETE_BOTH"); goto ok;
			     case BRT_ABORT_ANY: printf("ABORT_ANY"); goto ok;
			     case BRT_ABORT_BOTH: printf("ABORT_BOTH"); goto ok;
			     case BRT_COMMIT_ANY: printf("COMMIT_ANY"); goto ok;
			     case BRT_COMMIT_BOTH: printf("COMMIT_BOTH"); goto ok;
			     }
			     printf("HUH?");
			 ok:
			     printf(" xid=%"PRId64" ", xid);
			     print_item(key, keylen);
			     if (datalen>0) {
				 printf(" ");
				 print_item(data, datalen);
			     }
			     printf("\n");
			 })
			 );
	}
    } else {
	printf(" n_bytes_in_buffer=%d\n", n->u.l.n_bytes_in_buffer);
	printf(" items_in_buffer  =%d\n", toku_omt_size(n->u.l.buffer));
	int print_le(LEAFENTRY le, u_int32_t UU(idx), void *UU(v)) {
	    print_leafentry(stdout, le);
	    printf("\n");
	    return 0;
	}
	toku_omt_iterate(n->u.l.buffer, print_le, 0);
    }
}

int main (int argc, const char *argv[]) {
    assert(argc==2);
    const char *n = argv[1];
    int f = open(n, O_RDONLY);  assert(f>=0);
    struct brt_header *h;
    dump_header(f, &h);
    DISKOFF off;
    for (off=h->nodesize; off<h->unused_memory; off+=h->nodesize) {
	dump_node(f, off);
    }
    return 0;
}
