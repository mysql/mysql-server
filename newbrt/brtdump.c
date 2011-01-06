#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

/* Tell me the diff between two brt files. */

#include "includes.h"

static int dump_data = 1;

static CACHETABLE ct;

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
simple_hex_dump(unsigned char *vp, u_int64_t size) {
    for (u_int64_t i = 0; i < size; i++) {
        unsigned char c = vp[i];
        printf("%2.2X", c);
    }
}

static void
hex_dump(unsigned char *vp, u_int64_t offset, u_int64_t size) {
    u_int64_t n = size / 32;
    for (u_int64_t i = 0; i < n; i++) {
        printf("%"PRIu64": ", offset);
	for (u_int64_t j = 0; j < 32; j++) {
	    unsigned char c = vp[j];
	    printf("%2.2X", c);
	    if (((j+1) % 4) == 0)
	        printf(" ");
	}
	for (u_int64_t j = 0; j < 32; j++) {
            unsigned char c = vp[j];
	    printf("%c", isprint(c) ? c : ' ');
	}
	printf("\n");
	vp += 32;
	offset += 32;
    }
    size = size % 32;
    for (u_int64_t i=0; i<size; i++) {
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
dump_descriptor(DESCRIPTOR d) {
    printf(" descriptor version %u size %u ", d->version, d->dbt.size);
    simple_hex_dump(d->dbt.data, d->dbt.size);
    printf("\n");
}

static void
dump_header (int f, struct brt_header **header, CACHEFILE cf) {
    struct brt_header *h;
    int r;
    r = toku_deserialize_brtheader_from (f, &h); assert(r==0);
    h->cf = cf;
    printf("brtheader:\n");
    printf(" layout_version=%d\n", h->layout_version);
    printf(" layout_version_original=%d\n", h->layout_version_original);
    printf(" layout_version_read_from_disk=%d\n", h->layout_version_read_from_disk);
    printf(" dirty=%d\n", h->dirty);
    printf(" nodesize=%u\n", h->nodesize);
    printf(" unnamed_root=%" PRId64 "\n", h->root.b);
    printf(" flags=%u\n", h->flags);
    dump_descriptor(&h->descriptor);
    *header = h;
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
    //printf(" log_lsn     =%lld\n", n->log_lsn.lsn); // The log_lsn is a memory-only value.
    printf(" height      =%d\n",   n->height);
    printf(" layout_version=%d\n", n->layout_version);
    printf(" layout_version_original=%d\n", n->layout_version_original);
    printf(" layout_version_read_from_disk=%d\n", n->layout_version_read_from_disk);
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
	    printf("{nkey=%" PRIu64 " ndata=%" PRIu64 " dsize=%" PRIu64 " %s }", est->nkeys, est->ndata, est->dsize, est->exact ? "T" : "F");
	}
	printf("}\n");
	printf(" pivots:\n");
	for (i=0; i<n->u.n.n_children-1; i++) {
	    struct kv_pair *piv = n->u.n.childkeys[i];
	    printf("  pivot %d:", i);
            assert(n->flags == 0);
	    print_item(kv_pair_key_const(piv), kv_pair_keylen(piv));
	    printf("\n");
	}
	printf(" children:\n");
	for (i=0; i<n->u.n.n_children; i++) {
	    printf("   child %d: %" PRId64 "\n", i, BNC_BLOCKNUM(n, i).b);
	    unsigned int n_bytes = BNC_NBYTESINBUF(n, i); 
	    int n_entries = toku_fifo_n_entries(BNC_BUFFER(n, i));
	    if (n_bytes > 0 || n_entries > 0)
    	        printf("   buffer contains %u bytes (%d items)\n", n_bytes, n_entries);
	    if (dump_data) {
		FIFO_ITERATE(BNC_BUFFER(n,i), key, keylen, data, datalen, typ, xids,
			     {
				 printf("    TYPE=");
				 switch ((enum brt_msg_type)typ) {
				 case BRT_NONE: printf("NONE"); goto ok;
				 case BRT_INSERT: printf("INSERT"); goto ok;
				 case BRT_INSERT_NO_OVERWRITE: printf("INSERT_NO_OVERWRITE"); goto ok;
				 case BRT_DELETE_ANY: printf("DELETE_ANY"); goto ok;
				 case BRT_ABORT_ANY: printf("ABORT_ANY"); goto ok;
				 case BRT_COMMIT_ANY: printf("COMMIT_ANY"); goto ok;
				 case BRT_COMMIT_BROADCAST_ALL: printf("COMMIT_BROADCAST_ALL"); goto ok;
				 case BRT_COMMIT_BROADCAST_TXN: printf("COMMIT_BROADCAST_TXN"); goto ok;
				 case BRT_ABORT_BROADCAST_TXN: printf("ABORT_BROADCAST_TXN"); goto ok;
				 case BRT_OPTIMIZE: printf("OPTIMIZE"); goto ok;
				 case BRT_OPTIMIZE_FOR_UPGRADE: printf("OPTIMIZE_FOR_UPGRADE"); goto ok;
				 }
				 printf("HUH?");
			     ok:
				 printf(" xid=");
                                 xids_fprintf(stdout, xids);
                                 printf(" ");
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
        struct subtree_estimates *est = &n->u.l.leaf_stats;
        printf("{nkey=%" PRIu64 " ndata=%" PRIu64 " dsize=%" PRIu64 " %s }\n", est->nkeys, est->ndata, est->dsize, est->exact ? "T" : "F");
        printf(" optimized_for_upgrade=%u\n", n->u.l.optimized_for_upgrade);
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

static u_int32_t 
get_unaligned_uint32(unsigned char *p) {
    return *(u_int32_t *)p;
}

struct sub_block {
  u_int32_t compressed_size;
  u_int32_t uncompressed_size;
  u_int32_t xsum;
};

static void
sub_block_deserialize(struct sub_block *sb, unsigned char *sub_block_header) {
    sb->compressed_size = toku_dtoh32(get_unaligned_uint32(sub_block_header+0));
    sb->uncompressed_size = toku_dtoh32(get_unaligned_uint32(sub_block_header+4));
    sb->xsum = toku_dtoh32(get_unaligned_uint32(sub_block_header+8));
}

static void
verify_block(unsigned char *cp, u_int64_t file_offset, u_int64_t size) {
    // verify the header checksum
    const size_t node_header = 8 + sizeof (u_int32_t) + sizeof (u_int32_t);
    unsigned char *sub_block_header = &cp[node_header];
    u_int32_t n_sub_blocks = toku_dtoh32(get_unaligned_uint32(&sub_block_header[0]));
    u_int32_t header_length = node_header + n_sub_blocks * sizeof (struct sub_block);
    header_length += sizeof (u_int32_t); // CRC
    if (header_length > size) {
        printf("header length too big: %u\n", header_length);
        return;
    }
    u_int32_t header_xsum = x1764_memory(cp, header_length);
    u_int32_t expected_xsum = toku_dtoh32(get_unaligned_uint32(&cp[header_length]));
    if (header_xsum != expected_xsum) {
        printf("header checksum failed: %u %u\n", header_xsum, expected_xsum);
        return;
    }

    // deserialize the sub block header
    struct sub_block sub_block[n_sub_blocks];
    sub_block_header += sizeof (u_int32_t);
    for (u_int32_t i = 0 ; i < n_sub_blocks; i++) {
        sub_block_deserialize(&sub_block[i], sub_block_header);
        sub_block_header += sizeof (struct sub_block);
    }

    // verify the sub block header
    u_int32_t offset = header_length + 4;
    for (u_int32_t i = 0 ; i < n_sub_blocks; i++) {
        u_int32_t xsum = x1764_memory(cp + offset, sub_block[i].compressed_size);
        printf("%u: %u %u %u", i, sub_block[i].compressed_size, sub_block[i].uncompressed_size, sub_block[i].xsum);
        if (xsum != sub_block[i].xsum)
            printf(" fail %u offset %"PRIu64, xsum, file_offset + offset);
        printf("\n");
        offset += sub_block[i].compressed_size;
    }
    if (offset != size)
        printf("offset %u expected %"PRIu64"\n", offset, size);
}

static void
dump_block(int f, BLOCKNUM blocknum, struct brt_header *h) {
    DISKOFF offset, size;
    toku_translate_blocknum_to_offset_size(h->blocktable, blocknum, &offset, &size);
    printf("%"PRId64" at %"PRId64" size %"PRId64"\n", blocknum.b, offset, size);

    unsigned char *vp = toku_malloc(size);
    u_int64_t r = pread(f, vp, size, offset);
    if (r == (u_int64_t)size) {
        printf("%.8s layout_version=%u %u\n", (char*)vp, get_unaligned_uint32(vp+8), get_unaligned_uint32(vp+12));
        verify_block(vp, offset, size);
    }
    toku_free(vp);
}

static void
dump_file(int f, u_int64_t offset, u_int64_t size, FILE *outfp) {
    unsigned char *vp = toku_malloc(size);
    u_int64_t r = pread(f, vp, size, offset);
    if (r == size) {
        if (outfp == stdout)
            hex_dump(vp, offset, size);
        else
            fwrite(vp, size, 1, outfp);
    }
    toku_free(vp);
}

static void
set_file(int f, u_int64_t offset, unsigned char newc) {
    toku_os_pwrite(f, &newc, sizeof newc, offset);
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

static void 
interactive_help(void) {
    fprintf(stderr, "help\n");
    fprintf(stderr, "header\n");
    fprintf(stderr, "node NUMBER\n");
    fprintf(stderr, "bx OFFSET | block_translation OFFSET\n");
    fprintf(stderr, "dumpdata 0|1\n");
    fprintf(stderr, "fragmentation\n");
    fprintf(stderr, "file OFFSET SIZE [outfilename]\n");
    fprintf(stderr, "quit\n");
}

static uint64_t
getuint64(const char *f) {
    if (strncmp(f, "0x", 2) == 0 || strncmp(f, "0X", 2) == 0)
        return strtoull(f, 0, 16);
    else if (strncmp(f, "0", 1) == 0)
        return strtoull(f, 0, 8);
    else
        return strtoull(f, 0, 10);
}

int 
main (int argc, const char *const argv[]) {
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
    int f = open(n, O_RDWR + O_BINARY);  assert(f>=0);
    struct brt_header *h;
    // create a cachefile for the header
    int r = toku_create_cachetable(&ct, 1<<25, (LSN){0}, 0);
    assert(r == 0);
    CACHEFILE cf;
    FILENUM fn={0};
    r = toku_cachetable_openfd_with_filenum (&cf, ct, f, n, FALSE, fn, FALSE);
    assert(r==0);
    dump_header(f, &h, cf);
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
            if (strcmp(fields[0], "help") == 0) {
                interactive_help();
            } else if (strcmp(fields[0], "header") == 0) {
                toku_brtheader_free(h);
                dump_header(f, &h, cf);
            } else if (strcmp(fields[0], "block") == 0 && nfields == 2) {
                BLOCKNUM blocknum = make_blocknum(getuint64(fields[1]));
                dump_block(f, blocknum, h);
            } else if (strcmp(fields[0], "node") == 0 && nfields == 2) {
                BLOCKNUM off = make_blocknum(getuint64(fields[1]));
                dump_node(f, off, h);
            } else if (strcmp(fields[0], "dumpdata") == 0 && nfields == 2) {
                dump_data = strtol(fields[1], NULL, 10);
	    } else if (strcmp(fields[0], "block_translation") == 0 || strcmp(fields[0], "bx") == 0) {
     	        u_int64_t offset = 0;
	        if (nfields == 2)
		    offset = getuint64(fields[1]);
	        dump_block_translation(h, offset);
	    } else if (strcmp(fields[0], "fragmentation") == 0) {
	        dump_fragmentation(f, h);
            } else if (strcmp(fields[0], "file") == 0 && nfields >= 3) {
                u_int64_t offset = getuint64(fields[1]);
                u_int64_t size = getuint64(fields[2]);
                FILE *outfp = stdout;
                if (nfields >= 4)
                    outfp = fopen(fields[3], "w");
                dump_file(f, offset, size, outfp);
            } else if (strcmp(fields[0], "setfile") == 0 && nfields == 3) {
                u_int64_t offset = getuint64(fields[1]);
                unsigned char newc = getuint64(fields[2]);
                set_file(f, offset, newc);
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
    return 0;
}


