
/* Tell me the diff between two brt files. */

#include "includes.h"

static int dump_data = 1;

static void
print_item (bytevec val, ITEMLEN len) {
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

static void
dump_header (int f, struct brt_header **header) {
    struct brt_header *h;
    int r;
    r = toku_deserialize_brtheader_from (f, header_blocknum, &h); assert(r==0);
    printf("brtheader:\n");
    if (h->layout_version==BRT_LAYOUT_VERSION_6) printf(" layout_version<=6\n");
    else printf(" layout_version=%d\n", h->layout_version);
    printf(" dirty=%d\n", h->dirty);
    printf(" nodesize=%u\n", h->nodesize);
    printf(" free_blocks=%" PRId64 "\n", h->free_blocks.b);
    printf(" unused_memory=%" PRId64 "\n", h->unused_blocks.b);
    if (h->n_named_roots==-1) {
	printf(" unnamed_root=%" PRId64 "\n", h->roots[0].b);
	printf(" flags=%u\n", h->flags_array[0]);
    } else {
	printf(" n_named_roots=%d\n", h->n_named_roots);
	if (h->n_named_roots>=0) {
	    int i;
	    for (i=0; i<h->n_named_roots; i++) {
		printf("  %s -> %" PRId64 "\n", h->names[i], h->roots[i].b);
		printf(" flags=%u\n", h->flags_array[i]);
	    }
	}
    }
    *header = h;
    printf("Fifo:\n");
    printf(" fifo has %d entries\n", toku_fifo_n_entries(h->fifo));
    if (dump_data) {
	FIFO_ITERATE(h->fifo, key, keylen, data, datalen, type, xid,
		     {
			 printf(" ");
			 switch (type) {
			 case BRT_NONE: printf("NONE"); goto ok;
			 case BRT_INSERT: printf("INSERT"); goto ok;
			 case BRT_DELETE_ANY: printf("DELETE_ANY"); goto ok;
			 case BRT_DELETE_BOTH: printf("DELETE_BOTH"); goto ok;
			 case BRT_ABORT_ANY: printf("ABORT_ANY"); goto ok;
			 case BRT_ABORT_BOTH: printf("ABORT_BOTH"); goto ok;
			 case BRT_COMMIT_ANY: printf("COMMIT_ANY"); goto ok;
			 case BRT_COMMIT_BOTH: printf("COMMIT_BOTH"); goto ok;
			 }
			 printf("huh?");
		     ok:
			 printf(" %lld ", (long long)xid);
			 print_item(key, keylen);
			 printf(" ");
			 print_item(data, datalen);
			 printf("\n");
		     });
    }
}

static int
print_le (OMTVALUE lev, u_int32_t UU(idx), void *UU(v)) {
    LEAFENTRY le=lev;
    print_leafentry(stdout, le);
    printf("\n");
    return 0;
}

static void
dump_node (int f, BLOCKNUM blocknum, struct brt_header *h) {
    BRTNODE n;
    int r = toku_deserialize_brtnode_from (f, blocknum, 0 /*pass zero for hash, it doesn't matter*/, &n, h);
    assert(r==0);
    assert(n!=0);
    printf("brtnode\n");
    printf(" nodesize    =%u\n", n->nodesize);
    printf(" sizeonddisk =%u\n", toku_serialize_brtnode_size(n));
    printf(" flags       =%u\n", n->flags);
    printf(" thisnodename=%" PRId64 "\n", n->thisnodename.b);
    printf(" disk_lsn    =%" PRIu64 "\n", n->disk_lsn.lsn);
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
	printf(" subleafentry_estimates={");
	for (i=0; i<n->u.n.n_children; i++) {
	    if (i>0) printf(" ");
	    printf("%llu", (unsigned long long)(BNC_SUBTREE_LEAFENTRY_ESTIMATE(n, i)));
	}
	printf("}\n");
	printf(" pivots:\n");
	for (i=0; i<n->u.n.n_children-1; i++) {
	    struct kv_pair *piv = n->u.n.childkeys[i];
	    printf("  pivot %d:", i);
            assert(n->flags == 0 || n->flags == TOKU_DB_DUP+TOKU_DB_DUPSORT);
	    print_item(kv_pair_key_const(piv), kv_pair_keylen(piv));
            if (n->flags == TOKU_DB_DUP+TOKU_DB_DUPSORT) 
                print_item(kv_pair_val_const(piv), kv_pair_vallen(piv));
	    printf("\n");
	}
	printf(" children:\n");
	for (i=0; i<n->u.n.n_children; i++) {
	    printf("   child %d: %" PRId64 "\n", i, BNC_BLOCKNUM(n, i).b);
	    printf("   buffer contains %u bytes (%d items)\n", BNC_NBYTESINBUF(n, i), toku_fifo_n_entries(BNC_BUFFER(n,i)));
	    if (dump_data) {
		FIFO_ITERATE(BNC_BUFFER(n,i), key, keylen, data, datalen, typ, xid,
			     {
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
				 printf(" xid=%"PRIu64" ", xid);
				 print_item(key, keylen);
				 if (datalen>0) {
				     printf(" ");
				     print_item(data, datalen);
				 }
				 printf("\n");
			     }
			     );
	    }
	}
    } else {
	printf(" n_bytes_in_buffer=%u\n", n->u.l.n_bytes_in_buffer);
	printf(" items_in_buffer  =%u\n", toku_omt_size(n->u.l.buffer));
	if (dump_data) toku_omt_iterate(n->u.l.buffer, print_le, 0);
    }    toku_brtnode_free(&n);
}

static void 
dump_block_translation(struct brt_header *h, u_int64_t offset) {
    if (offset < h->translated_blocknum_limit) {
        struct block_translation_pair *bx = &h->block_translation[offset];
        printf("%"PRIu64": %"PRId64" %"PRId64"\n", offset, bx->diskoff, bx->size);
    }
}

static int 
bxpcmp(const void *a, const void *b) {
    const struct block_translation_pair *bxpa = a;
    const struct block_translation_pair *bxpb = b;
    if (bxpa->diskoff < bxpb->diskoff) return -1;
    if (bxpa->diskoff > bxpb->diskoff) return +1;
    return 0;
}

static void 
dump_fragmentation(int f, struct brt_header *h) {
    u_int64_t blocksizes = 0;
    u_int64_t leafsizes = 0; 
    u_int64_t leafblocks = 0;
    u_int64_t fragsizes = 0;
    u_int64_t i;
    for (i = 0; i < h->translated_blocknum_limit; i++) {
        BRTNODE n;
	BLOCKNUM blocknum = make_blocknum(i);
        int r = toku_deserialize_brtnode_from (f, blocknum, 0 /*pass zero for hash, it doesn't matter*/, &n, h);
	if (r != 0) continue;
        blocksizes += h->block_translation[i].size;
	if (n->height == 0) {
	    leafsizes += h->block_translation[i].size;
	    leafblocks += 1;
	}
	toku_brtnode_free(&n);
    }
    size_t n = h->translated_blocknum_limit * sizeof (struct block_translation_pair);
    struct block_translation_pair *bx = malloc(n);
    memcpy(bx, h->block_translation, n);
    qsort(bx, h->translated_blocknum_limit, sizeof (struct block_translation_pair), bxpcmp);
    for (i = 0; i < h->translated_blocknum_limit - 1; i++) {
        // printf("%lu %lu %lu\n", i, bx[i].diskoff, bx[i].size);
        fragsizes += bx[i+1].diskoff - (bx[i].diskoff + bx[i].size);
    }
    free(bx);
    printf("translated_blocknum_limit: %"PRIu64"\n", h->translated_blocknum_limit);
    printf("leafblocks: %"PRIu64"\n", leafblocks);
    printf("blocksizes: %"PRIu64"\n", blocksizes);
    printf("leafsizes: %"PRIu64"\n", leafsizes);
    printf("fragsizes: %"PRIu64"\n", fragsizes);
    printf("fragmentation: %.1f%%\n", 100. * ((double)fragsizes / (double)(fragsizes + blocksizes)));
}

static void
readline (char *line, int maxline) {
    int i = 0;
    int c;
    while ((c = getchar()) != EOF && c != '\n' && i < maxline) {
        line[i++] = (char)c;
    }
    line[i++] = 0;
}

static int
split_fields (char *line, char *fields[], int maxfields) {
    int i;
    for (i=0; i<maxfields; i++, line=NULL) {
        fields[i] = strtok(line, " ");
        if (fields[i] == NULL) break;
    }
    return i;
}

int 
main (int argc, const char *argv[]) {
    const char *arg0 = argv[0];
    static int interactive = 0;
    argc--; argv++;
    while (argc>1) {
	if (strcmp(argv[0], "--nodata")==0) {
	    dump_data = 0;
        } else if (strcmp(argv[0], "--interactive") == 0) {
            interactive = 1;
	} else {
	    printf("Usage: %s [--nodata] brtfilename\n", arg0);
	    exit(1);
	}
	argc--; argv++;
    }
    assert(argc==1);
    const char *n = argv[0];
    int f = open(n, O_RDONLY);  assert(f>=0);
    struct brt_header *h;
    dump_header(f, &h);
    if (interactive) {
        while (1) {
            printf("brtdump>"); fflush(stdout);
	    enum { maxline = 64};
            char line[maxline+1];
            readline(line, maxline);
            if (strcmp(line, "") == 0) 
                break;
            enum { maxfields = 2 };
            char *fields[maxfields];
            int nfields = split_fields(line, fields, maxfields);
            if (nfields == 0) 
                continue;
            if (strcmp(fields[0], "header") == 0) {
                toku_brtheader_free(h);
                dump_header(f, &h);
            } else if (strcmp(fields[0], "node") == 0 && nfields == 2) {
                BLOCKNUM off = make_blocknum(strtoll(fields[1], NULL, 10));
                dump_node(f, off, h);
            } else if (strcmp(fields[0], "dumpdata") == 0 && nfields == 2) {
                dump_data = strtol(fields[1], NULL, 10);
	    } else if (strcmp(fields[0], "block_translation") == 0 || strcmp(fields[0], "bx") == 0) {
     	        u_int64_t offset = 0;
	        if (nfields == 2)
		    offset = strtoll(fields[1], NULL, 10);
	        dump_block_translation(h, offset);
	    } else if (strcmp(fields[0], "fragmentation") == 0) {
	        dump_fragmentation(f, h);
            } else if (strcmp(fields[0], "quit") == 0 || strcmp(fields[0], "q") == 0) {
                break;
            }
        }
    } else {
	BLOCKNUM blocknum;
	printf("Block translation:");
	for (blocknum.b=0; blocknum.b<h->unused_blocks.b; blocknum.b++) {
	    printf(" %" PRId64 ":", blocknum.b);
	    if (h->block_translation[blocknum.b].size == -1) printf("free");
	    else printf("%" PRId64 ":%" PRId64, h->block_translation[blocknum.b].diskoff, h->block_translation[blocknum.b].size);
	}
	for (blocknum.b=1; blocknum.b<h->unused_blocks.b; blocknum.b++) {
	    if (h->block_translation[blocknum.b].size != -1)
		dump_node(f, blocknum, h);
        }
    }
    toku_brtheader_free(h);
    toku_malloc_cleanup();
    return 0;
}
