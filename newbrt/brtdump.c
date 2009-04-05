
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
    r = toku_deserialize_brtheader_from (f, &h); assert(r==0);
    printf("brtheader:\n");
    if (h->layout_version==BRT_LAYOUT_VERSION_6) printf(" layout_version<=6\n");
    else printf(" layout_version=%d\n", h->layout_version);
    printf(" dirty=%d\n", h->dirty);
    printf(" nodesize=%u\n", h->nodesize);
    printf(" unnamed_root=%" PRId64 "\n", h->root.b);
    printf(" flags=%u\n", h->flags);
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
    DISKOFF disksize, diskoffset;
    toku_translate_blocknum_to_offset_size(h->blocktable, blocknum, &diskoffset, &disksize);
    printf(" diskoffset  =%" PRId64 "\n", diskoffset);
    printf(" disksize    =%" PRId64 "\n", disksize);
    printf(" nodesize    =%u\n", n->nodesize);
    printf(" serialize_size =%u\n", toku_serialize_brtnode_size(n));
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
	    struct subtree_estimates *est = &(BNC_SUBTREE_ESTIMATES(n, i));
	    printf("{nkey=%" PRIu64 " ndata=%" PRIu64 " dsize=%" PRIu64 "}", est->nkeys, est->ndata, est->dsize);
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
    toku_blocknum_dump_translation(h->blocktable, make_blocknum(offset));
}

typedef struct {
    int f;
    struct brt_header *h;
    u_int64_t blocksizes;
    u_int64_t leafsizes;
    u_int64_t leafblocks;
} frag_help_extra;

static int
fragmentation_helper(BLOCKNUM b, int64_t size, int64_t UU(address), void *extra) {
    frag_help_extra *info = extra;
    BRTNODE n;
    int r = toku_deserialize_brtnode_from(info->f, b, 0 /*pass zero for hash, it doesn't matter*/, &n, info->h);
    if (r==0) {
        info->blocksizes += size;
        if (n->height == 0) {
            info->leafsizes += size;
            info->leafblocks++;
        }
	toku_brtnode_free(&n);
    }
    return 0;
}

static void 
dump_fragmentation(int f, struct brt_header *h) {
    frag_help_extra info;
    memset(&info, 0, sizeof(info));
    info.f = f;
    info.h = h;
    toku_blocktable_iterate(h->blocktable, TRANSLATION_CHECKPOINTED,
                            fragmentation_helper, &info, TRUE, TRUE);
    int64_t used_space;
    int64_t total_space;
    toku_blocktable_internal_fragmentation(h->blocktable, &total_space, &used_space);
    int64_t fragsizes = total_space - used_space;

    printf("leafblocks: %" PRIu64 "\n", info.leafblocks);
    printf("blocksizes: %" PRIu64 "\n", info.blocksizes);
    printf("used size: %" PRId64 "\n",  used_space);
    printf("total size: %" PRId64 "\n", total_space);
    printf("leafsizes: %" PRIu64 "\n", info.leafsizes);
    printf("fragsizes: %" PRId64 "\n", fragsizes);
    printf("fragmentation: %.1f%%\n", 100. * ((double)fragsizes / (double)(total_space)));
}

static void
hex_dump(unsigned char *vp, u_int64_t offset, u_int64_t size) {
    u_int64_t i;
    for (i=0; i<size; i++) {
        if ((i % 32) == 0)
            printf("%"PRIu64": ", offset+i);
        printf("%2.2X", vp[i]);
        if (((i+1) % 4) == 0)
            printf(" ");
        if (((i+1) % 32) == 0)
            printf("\n");
    }
    printf("\n");
}

static void
dump_file(int f, u_int64_t offset, u_int64_t size) {
    unsigned char *vp = toku_malloc(size);
    u_int64_t r = pread(f, vp, size, offset);
    if (r == size) 
        hex_dump(vp, offset, size);
    toku_free(vp);
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

static int
usage(const char *arg0) {
    printf("Usage: %s [--nodata] [--interactive] brtfilename\n", arg0);
    return 1;
}

typedef struct __dump_node_extra {
    int f;
    struct brt_header *h;
} dump_node_extra;

static int
dump_node_wrapper(BLOCKNUM b, int64_t UU(size), int64_t UU(address), void *extra) {
    dump_node_extra *info = extra;
    dump_node(info->f, b, info->h);
    return 0;
}

int 
main (int argc, const char *argv[]) {
    const char *arg0 = argv[0];
    int interactive = 0;
    argc--; argv++;
    while (argc>0) {
	if (strcmp(argv[0], "--nodata") == 0) {
	    dump_data = 0;
        } else if (strcmp(argv[0], "--interactive") == 0 || strcmp(argv[0], "--i") == 0) {
            interactive = 1;
        } else if (strcmp(argv[0], "--help") == 0) {
            return usage(arg0);
	} else 
	    break;
	argc--; argv++;
    }
    if (argc != 1) return usage(arg0);

    const char *n = argv[0];
    int f = open(n, O_RDONLY + O_BINARY);  assert(f>=0);
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
            const int maxfields = 4;
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
            } else if (strcmp(fields[0], "file") == 0 && nfields == 3) {
                u_int64_t offset, size;
                if (strncmp(fields[1], "0x", 2) == 0)
                    offset = strtoll(fields[1], NULL, 16);
                else
                    offset = strtoll(fields[1], NULL, 10);
                if (strncmp(fields[2], "0x", 2) == 0)
                    size = strtoll(fields[2], NULL, 16);
                else
                    size = strtoll(fields[2], NULL, 10);
                dump_file(f, offset, size);
            } else if (strcmp(fields[0], "quit") == 0 || strcmp(fields[0], "q") == 0) {
                break;
            }
        }
    } else {
	printf("Block translation:");

        toku_dump_translation_table(stdout, h->blocktable);

        struct __dump_node_extra info;
        info.f = f;
        info.h = h;
        toku_blocktable_iterate(h->blocktable, TRANSLATION_CHECKPOINTED,
                                dump_node_wrapper, &info, TRUE, TRUE);
    }
    toku_brtheader_free(h);
    toku_malloc_cleanup();
    return 0;
}


