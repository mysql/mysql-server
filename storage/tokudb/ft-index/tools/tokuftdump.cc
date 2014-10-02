/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
/*
COPYING CONDITIONS NOTICE:

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation, and provided that the
  following conditions are met:

      * Redistributions of source code must retain this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below).

      * Redistributions in binary form must reproduce this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below) in the documentation and/or other materials
        provided with the distribution.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.

COPYRIGHT NOTICE:

  TokuFT, Tokutek Fractal Tree Indexing Library.
  Copyright (C) 2007-2013 Tokutek, Inc.

DISCLAIMER:

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

UNIVERSITY PATENT NOTICE:

  The technology is licensed by the Massachusetts Institute of
  Technology, Rutgers State University of New Jersey, and the Research
  Foundation of State University of New York at Stony Brook under
  United States of America Serial No. 11/760379 and to the patents
  and/or patent applications resulting from it.

PATENT MARKING NOTICE:

  This software is covered by US Patent No. 8,185,551.
  This software is covered by US Patent No. 8,489,638.

PATENT RIGHTS GRANT:

  "THIS IMPLEMENTATION" means the copyrightable works distributed by
  Tokutek as part of the Fractal Tree project.

  "PATENT CLAIMS" means the claims of patents that are owned or
  licensable by Tokutek, both currently or in the future; and that in
  the absence of this license would be infringed by THIS
  IMPLEMENTATION or by using or running THIS IMPLEMENTATION.

  "PATENT CHALLENGE" shall mean a challenge to the validity,
  patentability, enforceability and/or non-infringement of any of the
  PATENT CLAIMS or otherwise opposing any of the PATENT CLAIMS.

  Tokutek hereby grants to you, for the term and geographical scope of
  the PATENT CLAIMS, a non-exclusive, no-charge, royalty-free,
  irrevocable (except as stated in this section) patent license to
  make, have made, use, offer to sell, sell, import, transfer, and
  otherwise run, modify, and propagate the contents of THIS
  IMPLEMENTATION, where such license applies only to the PATENT
  CLAIMS.  This grant does not include claims that would be infringed
  only as a consequence of further modifications of THIS
  IMPLEMENTATION.  If you or your agent or licensee institute or order
  or agree to the institution of patent litigation against any entity
  (including a cross-claim or counterclaim in a lawsuit) alleging that
  THIS IMPLEMENTATION constitutes direct or contributory patent
  infringement, or inducement of patent infringement, then any rights
  granted to you under this License shall terminate as of the date
  such litigation is filed.  If you or your agent or exclusive
  licensee institute or order or agree to the institution of a PATENT
  CHALLENGE, then Tokutek may terminate any rights granted to you
  under this License.
*/

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

// Dump a fractal tree file

#include <config.h>

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <limits.h>

#include "ft/serialize/block_table.h"
#include "ft/cachetable/cachetable.h"
#include "ft/ft.h"
#include "ft/ft-internal.h"
#include "ft/serialize/ft-serialize.h"
#include "ft/serialize/ft_node-serialize.h"
#include "ft/node.h"

static int do_dump_data = 1;
static int do_interactive = 0;
static int do_header = 0;
static int do_fragmentation = 0;
static int do_garbage = 0;
static int do_translation_table = 0;
static int do_rootnode = 0;
static int do_node = 0;
static BLOCKNUM do_node_num;
static int do_tsv = 0;

static const char *arg0;
static const char *fname;

static void format_time(const uint64_t time_int, char *buf) {
    time_t timer = (time_t) time_int;
    ctime_r(&timer, buf);
    assert(buf[24] == '\n');
    buf[24] = 0;
}

static void print_item(const void *val, uint32_t len) {
    printf("\"");
    uint32_t i;
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

static void simple_hex_dump(unsigned char *vp, uint64_t size) {
    for (uint64_t i = 0; i < size; i++) {
        unsigned char c = vp[i];
        printf("%2.2X", c);
    }
}

static void hex_dump(unsigned char *vp, uint64_t offset, uint64_t size) {
    uint64_t n = size / 32;
    for (uint64_t i = 0; i < n; i++) {
        printf("%" PRIu64 ": ", offset);
        for (uint64_t j = 0; j < 32; j++) {
            unsigned char c = vp[j];
            printf("%2.2X", c);
            if (((j+1) % 4) == 0)
                printf(" ");
        }
        for (uint64_t j = 0; j < 32; j++) {
            unsigned char c = vp[j];
            printf("%c", isprint(c) ? c : ' ');
        }
        printf("\n");
        vp += 32;
        offset += 32;
    }
    size = size % 32;
    for (uint64_t i=0; i<size; i++) {
        if ((i % 32) == 0)
            printf("%" PRIu64 ": ", offset+i);
        printf("%2.2X", vp[i]);
        if (((i+1) % 4) == 0)
            printf(" ");
        if (((i+1) % 32) == 0)
            printf("\n");
    }
    printf("\n");
}

static void dump_descriptor(DESCRIPTOR d) {
    printf(" descriptor size %u ", d->dbt.size);
    simple_hex_dump((unsigned char*) d->dbt.data, d->dbt.size);
    printf("\n");
}

static void open_header(int fd, FT *header, CACHEFILE cf) {
    FT ft = NULL;
    int r;
    r = toku_deserialize_ft_from (fd, MAX_LSN, &ft);
    if (r != 0) {
        fprintf(stderr, "%s: can not deserialize from %s error %d\n", arg0, fname, r);
        exit(1);
    }
    assert_zero(r);
    ft->cf = cf;
    *header = ft;
}

static void dump_header(FT ft) {
    char timestr[26];
    printf("ft:\n");
    printf(" layout_version=%d\n", ft->h->layout_version);
    printf(" layout_version_original=%d\n", ft->h->layout_version_original);
    printf(" layout_version_read_from_disk=%d\n", ft->layout_version_read_from_disk);
    printf(" build_id=%d\n", ft->h->build_id);
    printf(" build_id_original=%d\n", ft->h->build_id_original);
    format_time(ft->h->time_of_creation, timestr);
    printf(" time_of_creation=         %" PRIu64 "    %s\n", ft->h->time_of_creation, timestr);
    format_time(ft->h->time_of_last_modification, timestr);
    printf(" time_of_last_modification=%" PRIu64 "    %s\n", ft->h->time_of_last_modification, timestr);
    printf(" dirty=%d\n", ft->h->dirty);
    printf(" checkpoint_count=%" PRId64 "\n", ft->h->checkpoint_count);
    printf(" checkpoint_lsn=%" PRId64 "\n", ft->h->checkpoint_lsn.lsn);
    printf(" nodesize=%u\n", ft->h->nodesize);
    printf(" basementnodesize=%u\n", ft->h->basementnodesize);
    printf(" compression_method=%u\n", (unsigned) ft->h->compression_method);
    printf(" unnamed_root=%" PRId64 "\n", ft->h->root_blocknum.b);
    printf(" flags=%u\n", ft->h->flags);
    dump_descriptor(&ft->descriptor);
    printf(" estimated numrows=%" PRId64 "\n", ft->in_memory_stats.numrows);
    printf(" estimated numbytes=%" PRId64 "\n", ft->in_memory_stats.numbytes);
}

static int print_le(const void* key, const uint32_t keylen, const LEAFENTRY &le, const uint32_t idx UU(), void *const ai UU()) {
    print_klpair(stdout, key, keylen, le);
    printf("\n");
    return 0;
}

static void dump_node(int fd, BLOCKNUM blocknum, FT ft) {
    FTNODE n;
    FTNODE_DISK_DATA ndd = nullptr;
    ftnode_fetch_extra bfe;
    bfe.create_for_full_read(ft);
    int r = toku_deserialize_ftnode_from (fd, blocknum, 0 /*pass zero for hash, it doesn't matter*/, &n, &ndd, &bfe);
    assert_zero(r);
    assert(n!=0);
    printf("ftnode\n");
    DISKOFF disksize, diskoffset;
    ft->blocktable.translate_blocknum_to_offset_size(blocknum, &diskoffset, &disksize);
    printf(" diskoffset  =%" PRId64 "\n", diskoffset);
    printf(" disksize    =%" PRId64 "\n", disksize);
    printf(" serialize_size =%u\n", toku_serialize_ftnode_size(n));
    printf(" flags       =%u\n", n->flags);
    printf(" blocknum=%" PRId64 "\n", n->blocknum.b);
    //printf(" log_lsn     =%lld\n", n->log_lsn.lsn); // The log_lsn is a memory-only value.
    printf(" height      =%d\n",   n->height);
    printf(" layout_version=%d\n", n->layout_version);
    printf(" layout_version_original=%d\n", n->layout_version_original);
    printf(" layout_version_read_from_disk=%d\n", n->layout_version_read_from_disk);
    printf(" build_id=%d\n", n->build_id);
    printf(" max_msn_applied_to_node_on_disk=%" PRId64 " (0x%" PRIx64 ")\n", n->max_msn_applied_to_node_on_disk.msn, n->max_msn_applied_to_node_on_disk.msn);
    printf(" io time %lf decompress time %lf deserialize time %lf\n", 
           tokutime_to_seconds(bfe.io_time), 
           tokutime_to_seconds(bfe.decompress_time), 
           tokutime_to_seconds(bfe.deserialize_time));

    printf(" n_children=%d\n", n->n_children);
    printf(" pivotkeys.total_size()=%u\n", (unsigned) n->pivotkeys.total_size());

    printf(" pivots:\n");
    for (int i=0; i<n->n_children-1; i++) {
        const DBT piv = n->pivotkeys.get_pivot(i);
        printf("  pivot %2d:", i);
        if (n->flags)
            printf(" flags=%x ", n->flags);
        print_item(piv.data, piv.size);
        printf("\n");
    }
    printf(" children:\n");
    for (int i=0; i<n->n_children; i++) {
            printf("  child %d: ", i);
        if (n->height > 0) {
            printf("%" PRId64 "\n", BP_BLOCKNUM(n, i).b);
            NONLEAF_CHILDINFO bnc = BNC(n, i);
            unsigned int n_bytes = toku_bnc_nbytesinbuf(bnc); 
            int n_entries = toku_bnc_n_entries(bnc);
            if (n_bytes > 0 || n_entries > 0) {
                printf("   buffer contains %u bytes (%d items)\n", n_bytes, n_entries);
            }
            if (do_dump_data) {
                struct dump_data_fn {
                    int operator()(const ft_msg &msg, bool UU(is_fresh)) {
                        enum ft_msg_type type = (enum ft_msg_type) msg.type();
                        MSN msn = msg.msn();
                        XIDS xids = msg.xids();
                        const void *key = msg.kdbt()->data;
                        const void *data = msg.vdbt()->data;
                        uint32_t keylen = msg.kdbt()->size;
                        uint32_t datalen = msg.vdbt()->size;
                        printf("    msn=%" PRIu64 " (0x%" PRIx64 ") ", msn.msn, msn.msn);
                        printf("    TYPE=");
                        switch (type) {
                            case FT_NONE: printf("NONE"); goto ok;
                            case FT_INSERT: printf("INSERT"); goto ok;
                            case FT_INSERT_NO_OVERWRITE: printf("INSERT_NO_OVERWRITE"); goto ok;
                            case FT_DELETE_ANY: printf("DELETE_ANY"); goto ok;
                            case FT_ABORT_ANY: printf("ABORT_ANY"); goto ok;
                            case FT_COMMIT_ANY: printf("COMMIT_ANY"); goto ok;
                            case FT_COMMIT_BROADCAST_ALL: printf("COMMIT_BROADCAST_ALL"); goto ok;
                            case FT_COMMIT_BROADCAST_TXN: printf("COMMIT_BROADCAST_TXN"); goto ok;
                            case FT_ABORT_BROADCAST_TXN: printf("ABORT_BROADCAST_TXN"); goto ok;
                            case FT_OPTIMIZE: printf("OPTIMIZE"); goto ok;
                            case FT_OPTIMIZE_FOR_UPGRADE: printf("OPTIMIZE_FOR_UPGRADE"); goto ok;
                            case FT_UPDATE:   printf("UPDATE"); goto ok;
                            case FT_UPDATE_BROADCAST_ALL: printf("UPDATE_BROADCAST_ALL"); goto ok;
                        }
                        printf("HUH?");
ok:
                        printf(" xid=");
                        toku_xids_fprintf(stdout, xids);
                        printf(" ");
                        print_item(key, keylen);
                        if (datalen>0) {
                            printf(" ");
                            print_item(data, datalen);
                        }
                        printf("\n");
                        return 0;
                    }
                } dump_fn;
                bnc->msg_buffer.iterate(dump_fn);
            }
        } else {
            printf(" n_bytes_in_buffer= %" PRIu64 "", BLB_DATA(n, i)->get_disk_size());
            printf(" items_in_buffer=%u\n", BLB_DATA(n, i)->num_klpairs());
            if (do_dump_data) {
                BLB_DATA(n, i)->iterate<void, print_le>(NULL);
            }
        }
    }
    toku_ftnode_free(&n);
    toku_free(ndd);
}

static void dump_block_translation(FT ft, uint64_t offset) {
    ft->blocktable.blocknum_dump_translation(make_blocknum(offset));
}

static void dump_fragmentation(int UU(f), FT ft, int tsv) {
    int64_t used_space;
    int64_t total_space;
    ft->blocktable.internal_fragmentation(&total_space, &used_space);
    int64_t fragsizes = total_space - used_space;

    if (tsv) {
        printf("%" PRId64 "\t%" PRId64 "\t%" PRId64 "\t%.1f\n", used_space, total_space, fragsizes,
               100. * ((double)fragsizes / (double)(total_space)));
    } else {
        printf("used_size\t%" PRId64 "\n",  used_space);
        printf("total_size\t%" PRId64 "\n", total_space);
        printf("fragsizes\t%" PRId64 "\n", fragsizes);
        printf("fragmentation\t%.1f\n", 100. * ((double)fragsizes / (double)(total_space)));
    }
}

typedef struct {
    int fd;
    FT ft;
    uint64_t blocksizes;
    uint64_t leafsizes;
    uint64_t leafblocks;
} frag_help_extra;

static int nodesizes_helper(BLOCKNUM b, int64_t size, int64_t UU(address), void *extra) {
    frag_help_extra *CAST_FROM_VOIDP(info, extra);
    FTNODE n;
    FTNODE_DISK_DATA ndd = NULL;
    ftnode_fetch_extra bfe;
    bfe.create_for_full_read(info->ft);
    int r = toku_deserialize_ftnode_from(info->fd, b, 0 /*pass zero for hash, it doesn't matter*/, &n, &ndd, &bfe);
    if (r==0) {
        info->blocksizes += size;
        if (n->height == 0) {
            info->leafsizes += size;
            info->leafblocks++;
        }
        toku_ftnode_free(&n);
        toku_free(ndd);
    }
    return 0;
}

static void dump_nodesizes(int fd, FT ft) {
    frag_help_extra info;
    memset(&info, 0, sizeof(info));
    info.fd = fd;
    info.ft = ft;
    ft->blocktable.iterate(block_table::TRANSLATION_CHECKPOINTED,
                           nodesizes_helper, &info, true, true);
    printf("leafblocks\t%" PRIu64 "\n", info.leafblocks);
    printf("blocksizes\t%" PRIu64 "\n", info.blocksizes);
    printf("leafsizes\t%" PRIu64 "\n", info.leafsizes);
}

static void dump_garbage_stats(int fd, FT ft) {
    assert(fd == toku_cachefile_get_fd(ft->cf));
    uint64_t total_space = 0;
    uint64_t used_space = 0;
    toku_ft_get_garbage(ft, &total_space, &used_space);
    printf("garbage total size\t%" PRIu64 "\n", total_space);
    printf("garbage used size\t%" PRIu64 "\n", used_space);
}

typedef struct __dump_node_extra {
    int fd;
    FT ft;
} dump_node_extra;

static int dump_node_wrapper(BLOCKNUM b, int64_t UU(size), int64_t UU(address), void *extra) {
    dump_node_extra *CAST_FROM_VOIDP(info, extra);
    dump_node(info->fd, b, info->ft);
    return 0;
}

static uint32_t get_unaligned_uint32(unsigned char *p) {
    uint32_t n;
    memcpy(&n, p, sizeof n);
    return n;
}

struct dump_sub_block {
    uint32_t compressed_size;
    uint32_t uncompressed_size;
    uint32_t xsum;
};

static void sub_block_deserialize(struct dump_sub_block *sb, unsigned char *sub_block_header) {
    sb->compressed_size = toku_dtoh32(get_unaligned_uint32(sub_block_header+0));
    sb->uncompressed_size = toku_dtoh32(get_unaligned_uint32(sub_block_header+4));
    sb->xsum = toku_dtoh32(get_unaligned_uint32(sub_block_header+8));
}

static void verify_block(unsigned char *cp, uint64_t file_offset, uint64_t size) {
    // verify the header checksum
    const size_t node_header = 8 + sizeof (uint32_t) + sizeof (uint32_t) + sizeof (uint32_t);
    
    printf("%.8s layout_version=%u %u build=%d\n", cp, get_unaligned_uint32(cp+8), get_unaligned_uint32(cp+12), get_unaligned_uint32(cp+16));

    unsigned char *sub_block_header = &cp[node_header];
    uint32_t n_sub_blocks = toku_dtoh32(get_unaligned_uint32(&sub_block_header[0]));
    uint32_t header_length = node_header + n_sub_blocks * sizeof (struct dump_sub_block);
    header_length += sizeof (uint32_t); // CRC
    if (header_length > size) {
        printf("header length too big: %u\n", header_length);
        return;
    }
    uint32_t header_xsum = toku_x1764_memory(cp, header_length);
    uint32_t expected_xsum = toku_dtoh32(get_unaligned_uint32(&cp[header_length]));
    if (header_xsum != expected_xsum) {
        printf("header checksum failed: %u %u\n", header_xsum, expected_xsum);
        return;
    }

    // deserialize the sub block header
    struct dump_sub_block sub_block[n_sub_blocks];
    sub_block_header += sizeof (uint32_t);
    for (uint32_t i = 0 ; i < n_sub_blocks; i++) {
        sub_block_deserialize(&sub_block[i], sub_block_header);
        sub_block_header += sizeof (struct dump_sub_block);
    }

    // verify the sub block header
    uint32_t offset = header_length + 4;
    for (uint32_t i = 0 ; i < n_sub_blocks; i++) {
        uint32_t xsum = toku_x1764_memory(cp + offset, sub_block[i].compressed_size);
        printf("%u: %u %u %u", i, sub_block[i].compressed_size, sub_block[i].uncompressed_size, sub_block[i].xsum);
        if (xsum != sub_block[i].xsum)
            printf(" fail %u offset %" PRIu64, xsum, file_offset + offset);
        printf("\n");
        offset += sub_block[i].compressed_size;
    }
    if (offset != size)
        printf("offset %u expected %" PRIu64 "\n", offset, size);
}

static void dump_block(int fd, BLOCKNUM blocknum, FT ft) {
    DISKOFF offset, size;
    ft->blocktable.translate_blocknum_to_offset_size(blocknum, &offset, &size);
    printf("%" PRId64 " at %" PRId64 " size %" PRId64 "\n", blocknum.b, offset, size);

    unsigned char *CAST_FROM_VOIDP(vp, toku_malloc(size));
    uint64_t r = pread(fd, vp, size, offset);
    if (r == (uint64_t)size) {
        verify_block(vp, offset, size);
    }
    toku_free(vp);
}

static void dump_file(int fd, uint64_t offset, uint64_t size, FILE *outfp) {
    unsigned char *XMALLOC_N(size, vp);
    uint64_t r = pread(fd, vp, size, offset);
    if (r == size) {
        if (outfp == stdout) {
            hex_dump(vp, offset, size);
        } else {
            size_t wrote = fwrite(vp, size, 1, outfp);
            assert(wrote == 1);
        }
    }
    toku_free(vp);
}

static void set_file(int fd, uint64_t offset, unsigned char newc) {
    toku_os_pwrite(fd, &newc, sizeof newc, offset);
}

static int readline(char *line, int maxline) {
    int i = 0;
    int c;
    while ((c = getchar()) != EOF && c != '\n' && i < maxline) {
        line[i++] = (char)c;
    }
    line[i++] = 0;
    return c == EOF ? EOF : i;
}

static int split_fields(char *line, char *fields[], int maxfields) {
    int i;
    for (i=0; i<maxfields; i++)
        fields[i] = NULL;
    for (i=0; i<maxfields; i++, line=NULL) {
        fields[i] = strtok(line, " ");
        if (fields[i] == NULL) {
            break;
        }
    }
    return i;
}

static uint64_t getuint64(const char *f) {
    if (strncmp(f, "0x", 2) == 0 || strncmp(f, "0X", 2) == 0)
        return strtoull(f, 0, 16);
    else if (strncmp(f, "0", 1) == 0)
        return strtoull(f, 0, 8);
    else
        return strtoull(f, 0, 10);
}

static void interactive_help(void) {
    fprintf(stderr, "help\n");
    fprintf(stderr, "header\n");
    fprintf(stderr, "node NUMBER\n");
    fprintf(stderr, "bx OFFSET | block_translation OFFSET\n");
    fprintf(stderr, "dumpdata 0|1\n");
    fprintf(stderr, "fragmentation\n");
    fprintf(stderr, "nodesizes\n");
    fprintf(stderr, "garbage\n");
    fprintf(stderr, "file OFFSET SIZE [outfilename]\n");
    fprintf(stderr, "quit\n");
}

static void run_iteractive_loop(int fd, FT ft, CACHEFILE cf) {
    while (1) {
        printf("ftdump>"); fflush(stdout);
        enum { maxline = 64};
        char line[maxline+1];
        int r = readline(line, maxline);
        if (r == EOF)
            break;
        const int maxfields = 4;
        char *fields[maxfields];
        int nfields = split_fields(line, fields, maxfields);
        if (nfields == 0) 
            continue;
        if (strcmp(fields[0], "help") == 0) {
            interactive_help();
        } else if (strcmp(fields[0], "header") == 0) {
            toku_ft_free(ft);
            open_header(fd, &ft, cf);
            dump_header(ft);
        } else if (strcmp(fields[0], "block") == 0 && nfields == 2) {
            BLOCKNUM blocknum = make_blocknum(getuint64(fields[1]));
            dump_block(fd, blocknum, ft);
        } else if (strcmp(fields[0], "node") == 0 && nfields == 2) {
            BLOCKNUM off = make_blocknum(getuint64(fields[1]));
            dump_node(fd, off, ft);
        } else if (strcmp(fields[0], "dumpdata") == 0 && nfields == 2) {
            do_dump_data = strtol(fields[1], NULL, 10);
        } else if (strcmp(fields[0], "block_translation") == 0 || strcmp(fields[0], "bx") == 0) {
            uint64_t offset = 0;
            if (nfields == 2)
                offset = getuint64(fields[1]);
            dump_block_translation(ft, offset);
        } else if (strcmp(fields[0], "fragmentation") == 0) {
            dump_fragmentation(fd, ft, do_tsv);
        } else if (strcmp(fields[0], "nodesizes") == 0) {
            dump_nodesizes(fd, ft);
        } else if (strcmp(fields[0], "garbage") == 0) {
            dump_garbage_stats(fd, ft);
        } else if (strcmp(fields[0], "file") == 0 && nfields >= 3) {
            uint64_t offset = getuint64(fields[1]);
            uint64_t size = getuint64(fields[2]);
            FILE *outfp = stdout;
            if (nfields >= 4)
                outfp = fopen(fields[3], "w");
            dump_file(fd, offset, size, outfp);
        } else if (strcmp(fields[0], "setfile") == 0 && nfields == 3) {
            uint64_t offset = getuint64(fields[1]);
            unsigned char newc = getuint64(fields[2]);
            set_file(fd, offset, newc);
        } else if (strcmp(fields[0], "quit") == 0 || strcmp(fields[0], "q") == 0) {
            break;
        }
    }
}

static int usage(void) {
    fprintf(stderr, "Usage: %s ", arg0);
    fprintf(stderr, "--interactive ");
    fprintf(stderr, "--nodata ");
    fprintf(stderr, "--dumpdata 0|1 ");
    fprintf(stderr, "--header ");
    fprintf(stderr, "--rootnode ");
    fprintf(stderr, "--node N ");
    fprintf(stderr, "--fragmentation ");
    fprintf(stderr, "--garbage ");
    fprintf(stderr, "--tsv ");
    fprintf(stderr, "--translation-table ");
    fprintf(stderr, "--tsv ");
    fprintf(stderr, "filename \n");
    return 1;
}

int main (int argc, const char *const argv[]) {
    arg0 = argv[0];
    argc--; argv++;
    while (argc>0) {
        if (strcmp(argv[0], "--interactive") == 0 || strcmp(argv[0], "--i") == 0) {
            do_interactive = 1;
        } else if (strcmp(argv[0], "--nodata") == 0) {
            do_dump_data = 0;
        } else if (strcmp(argv[0], "--dumpdata") == 0 && argc > 1) {
            argc--; argv++;
            do_dump_data = atoi(argv[0]);
        } else if (strcmp(argv[0], "--header") == 0) {
            do_header = 1;
        } else if (strcmp(argv[0], "--rootnode") == 0) {
            do_rootnode = 1;
        } else if (strcmp(argv[0], "--node") == 0 && argc > 1) {
            argc--; argv++;
            do_node = 1;
            do_node_num = make_blocknum(getuint64(argv[0]));
        } else if (strcmp(argv[0], "--fragmentation") == 0) {
            do_fragmentation = 1;
        } else if (strcmp(argv[0], "--garbage") == 0) {
            do_garbage = 1;
        } else if (strcmp(argv[0], "--tsv") == 0) {
            do_tsv = 1;
        } else if (strcmp(argv[0], "--translation-table") == 0) {
            do_translation_table = 1;
        } else if (strcmp(argv[0], "--help") == 0 || strcmp(argv[0], "-?") == 0 || strcmp(argv[0], "-h") == 0) {
            return usage();
        } else {
            break;
        }
        argc--; argv++;
    }
    if (argc != 1) 
        return usage();

    int r = toku_ft_layer_init();
    assert_zero(r);

    fname = argv[0];
    int fd = open(fname, O_RDWR + O_BINARY);
    if (fd < 0) {
        fprintf(stderr, "%s: can not open %s errno %d\n", arg0, fname, errno);
        return 1;
    }

    // create a cachefile for the header
    CACHETABLE ct = NULL;
    toku_cachetable_create(&ct, 1<<25, (LSN){0}, 0);

    CACHEFILE cf = NULL;
    r = toku_cachetable_openfd (&cf, ct, fd, fname);
    assert_zero(r);

    FT ft = NULL;
    open_header(fd, &ft, cf);

    if (do_interactive) {
        run_iteractive_loop(fd, ft, cf);
    } else {
        if (do_header) {
            dump_header(ft);
        }
        if (do_rootnode) {
            dump_node(fd, ft->h->root_blocknum, ft);
        } 
        if (do_node) {
            dump_node(fd, do_node_num, ft);
        }
        if (do_fragmentation) {
            dump_fragmentation(fd, ft, do_tsv);
        }
        if (do_translation_table) {
            ft->blocktable.dump_translation_table_pretty(stdout);
        }
        if (do_garbage) {
            dump_garbage_stats(fd, ft);
        }
        if (!do_header && !do_rootnode && !do_fragmentation && !do_translation_table && !do_garbage) {
            printf("Block translation:");
            ft->blocktable.dump_translation_table(stdout);

            dump_header(ft);
            
            struct __dump_node_extra info;
            info.fd = fd;
            info.ft = ft;
            ft->blocktable.iterate(block_table::TRANSLATION_CHECKPOINTED,
                                   dump_node_wrapper, &info, true, true);
        }
    }
    toku_cachefile_close(&cf, false, ZERO_LSN);
    toku_cachetable_close(&ct);
    toku_ft_free(ft);
    toku_ft_layer_destroy();
    return 0;
}
