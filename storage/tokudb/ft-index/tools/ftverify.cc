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

////////////////////////////////////////////////////////////////////
// ftverify - Command line tool that checks the validity of a given
// fractal tree file, one block at a time.
////////////////////////////////////////////////////////////////////

#include <config.h>

#include "portability/toku_assert.h"
#include "portability/toku_list.h"
#include "portability/toku_portability.h"

#include "ft/serialize/block_allocator.h"
#include "ft/ft-internal.h"
#include "ft/serialize/ft-serialize.h"
#include "ft/serialize/ft_layout_version.h"
#include "ft/serialize/ft_node-serialize.h"
#include "ft/node.h"
#include "ft/serialize/rbuf.h"
#include "ft/serialize/sub_block.h"
#include "util/threadpool.h"

#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sysexits.h>
#include <unistd.h>

static int num_cores = 0; // cache the number of cores for the parallelization
static struct toku_thread_pool *ft_pool = NULL;
static FILE *outf;
static double pct = 0.5;

// Struct for reporting sub block stats.
struct verify_block_extra {
    BLOCKNUM b;
    int n_sub_blocks;
    uint32_t header_length;
    uint32_t calc_xsum;
    uint32_t stored_xsum;
    bool header_valid;
    bool sub_blocks_valid;
    struct sub_block_info *sub_block_results;
};

// Initialization function for the sub block stats.
static void
init_verify_block_extra(BLOCKNUM b, struct verify_block_extra *e)
{
    static const struct verify_block_extra default_vbe =
    {
        .b = { 0 },
        .n_sub_blocks = 0,
        .header_length = 0,
        .calc_xsum = 0,
        .stored_xsum = 0,
        .header_valid = true,
        .sub_blocks_valid = true,
        .sub_block_results = NULL
    };
    *e = default_vbe;
    e->b = b;
}

// Reports percentage of completed blocks.
static void
report(int64_t blocks_done, int64_t blocks_failed, int64_t total_blocks)
{
    int64_t blocks_per_report = llrint(pct * total_blocks / 100.0);
    if (blocks_per_report < 1) {
        blocks_per_report = 1;
    }
    if (blocks_done % blocks_per_report == 0) {
        double pct_actually_done = (100.0 * blocks_done) / total_blocks;
        printf("% 3.3lf%% | %" PRId64 " blocks checked, %" PRId64 " bad block(s) detected\n",
               pct_actually_done, blocks_done, blocks_failed);
        fflush(stdout);
    }
}

// Helper function to deserialize one of the two headers for the ft
// we are checking.
static void
deserialize_headers(int fd, struct ft **h1p, struct ft **h2p)
{
    struct rbuf rb_0;
    struct rbuf rb_1;
    uint64_t checkpoint_count_0;
    uint64_t checkpoint_count_1;
    LSN checkpoint_lsn_0;
    LSN checkpoint_lsn_1;
    uint32_t version_0, version_1;
    bool h0_acceptable = false;
    bool h1_acceptable = false;
    int r0, r1;
    int r;

    {
        toku_off_t header_0_off = 0;
        r0 = deserialize_ft_from_fd_into_rbuf(
            fd,
            header_0_off,
            &rb_0,
            &checkpoint_count_0,
            &checkpoint_lsn_0,
            &version_0
            );
        if ((r0==0) && (checkpoint_lsn_0.lsn <= MAX_LSN.lsn)) {
            h0_acceptable = true;
        }
    }
    {
        toku_off_t header_1_off = block_allocator::BLOCK_ALLOCATOR_HEADER_RESERVE;
        r1 = deserialize_ft_from_fd_into_rbuf(
            fd,
            header_1_off,
            &rb_1,
            &checkpoint_count_1,
            &checkpoint_lsn_1,
            &version_1
            );
        if ((r1==0) && (checkpoint_lsn_1.lsn <= MAX_LSN.lsn)) {
            h1_acceptable = true;
        }
    }

    // If either header is too new, the dictionary is unreadable
    if (r0 == TOKUDB_DICTIONARY_TOO_NEW || r1 == TOKUDB_DICTIONARY_TOO_NEW) {
        fprintf(stderr, "This dictionary was created with a version of TokuFT that is too new.  Aborting.\n");
        abort();
    }
    if (h0_acceptable) {
        printf("Found dictionary header 1 with LSN %" PRIu64 "\n", checkpoint_lsn_0.lsn);
        r = deserialize_ft_versioned(fd, &rb_0, h1p, version_0);

	if (r != 0) {
	    printf("---Header Error----\n");
	}

    } else {
        *h1p = NULL;
    }
    if (h1_acceptable) {
        printf("Found dictionary header 2 with LSN %" PRIu64 "\n", checkpoint_lsn_1.lsn);
        r = deserialize_ft_versioned(fd, &rb_1, h2p, version_1);
	if (r != 0) {
	    printf("---Header Error----\n");
	}
    } else {
        *h2p = NULL;
    }

    if (rb_0.buf) toku_free(rb_0.buf);
    if (rb_1.buf) toku_free(rb_1.buf);
}

// Helper struct for tracking block checking progress.
struct check_block_table_extra {
    int fd;
    int64_t blocks_done, blocks_failed, total_blocks;
    struct ft *h;
};

// Check non-upgraded (legacy) node.
// NOTE: These nodes have less checksumming than more 
// recent nodes.  This effectively means that we are 
// skipping over these nodes.
static int
check_old_node(FTNODE node, struct rbuf *rb, int version)
{
    int r = 0;
    read_legacy_node_info(node, rb, version);
    // For version 14 nodes, advance the buffer to the end
    // and verify the checksum.
    if (version == FT_FIRST_LAYOUT_VERSION_WITH_END_TO_END_CHECKSUM) {
        // Advance the buffer to the end.
        rb->ndone = rb->size - 4;
        r = check_legacy_end_checksum(rb);
    }
    
    return r;
}

// Read, decompress, and check the given block.
static int
check_block(BLOCKNUM blocknum, int64_t UU(blocksize), int64_t UU(address), void *extra)
{
    int r = 0;
    int failure = 0;
    struct check_block_table_extra *CAST_FROM_VOIDP(cbte, extra);
    int fd = cbte->fd;
    FT ft = cbte->h;

    struct verify_block_extra be;
    init_verify_block_extra(blocknum, &be);

    // Let's read the block off of disk and fill a buffer with that
    // block.
    struct rbuf rb = RBUF_INITIALIZER;
    read_block_from_fd_into_rbuf(fd, blocknum, ft, &rb);

    // Allocate the node.
    FTNODE XMALLOC(node);
    
    initialize_ftnode(node, blocknum);
    
    r = read_and_check_magic(&rb);
    if (r == DB_BADFORMAT) {
        printf(" Magic failed.\n");
        failure++;
    }

    r = read_and_check_version(node, &rb);
    if (r != 0) {
       	printf(" Version check failed.\n");
        failure++;
    }

    int version = node->layout_version_read_from_disk;

      ////////////////////////////
     // UPGRADE FORK GOES HERE //
    ////////////////////////////
    
    // Check nodes before major layout changes in version 15.
    // All newer versions should follow the same layout, for now.
    // This predicate would need to be changed if the layout
    // of the nodes on disk does indeed change in the future.
    if (version < FT_FIRST_LAYOUT_VERSION_WITH_BASEMENT_NODES)
    {
        struct rbuf nrb;
        // Use old decompression method for legacy nodes.
        r = decompress_from_raw_block_into_rbuf(rb.buf, rb.size, &nrb, blocknum);
        if (r != 0) {
            failure++;
            goto cleanup;
        }
        
        // Check the end-to-end checksum.
        r = check_old_node(node, &nrb, version);
        if (r != 0) {
            failure++;
        }
        goto cleanup;
    }

    read_node_info(node, &rb, version);

    FTNODE_DISK_DATA ndd;
    allocate_and_read_partition_offsets(node, &rb, &ndd);

    r = check_node_info_checksum(&rb);
    if (r == TOKUDB_BAD_CHECKSUM) {
       	printf(" Node info checksum failed.\n");
        failure++;
    }

    // Get the partition info sub block.
    struct sub_block sb;
    sub_block_init(&sb);
    r = read_compressed_sub_block(&rb, &sb);
    if (r != 0) {
       	printf(" Partition info checksum failed.\n");
        failure++;
    }

    just_decompress_sub_block(&sb);

    // If we want to inspect the data inside the partitions, we need
    // to call setup_ftnode_partitions(node, bfe, true)

    // <CER> TODO: Create function for this.
    // Using the node info, decompress all the keys and pivots to
    // detect any corruptions.
    for (int i = 0; i < node->n_children; ++i) {
        uint32_t curr_offset = BP_START(ndd,i);
        uint32_t curr_size   = BP_SIZE(ndd,i);
        struct rbuf curr_rbuf = {.buf = NULL, .size = 0, .ndone = 0};
        rbuf_init(&curr_rbuf, rb.buf + curr_offset, curr_size);
        struct sub_block curr_sb;
        sub_block_init(&curr_sb);

        r = read_compressed_sub_block(&rb, &sb);
        if (r != 0) {
            printf(" Compressed child partition %d checksum failed.\n", i);
            failure++;
        }
        just_decompress_sub_block(&sb);
	
        r = verify_ftnode_sub_block(&sb);
        if (r != 0) {
            printf(" Uncompressed child partition %d checksum failed.\n", i);
            failure++;
        }

	// <CER> If needed, we can print row and/or pivot info at this
	// point.
    }

cleanup:
    // Cleanup and error incrementing.
    if (failure) {
       	cbte->blocks_failed++;
    }

    cbte->blocks_done++;

    if (node) {
        toku_free(node);
    }

    // Print the status of this block to the console.
    report(cbte->blocks_done, cbte->blocks_failed, cbte->total_blocks);
    // We need to ALWAYS return 0 if we want to continue iterating
    // through the nodes in the file.
    r = 0;
    return r;
}

// This calls toku_blocktable_iterate on the given block table.
// Passes our check_block() function to be called as we iterate over
// the block table.  This will print any interesting failures and
// update us on our progress.
static void check_block_table(int fd, block_table *bt, struct ft *h) {
    int64_t num_blocks = bt->get_blocks_in_use_unlocked();
    printf("Starting verification of checkpoint containing");
    printf(" %" PRId64 " blocks.\n", num_blocks);
    fflush(stdout);

    struct check_block_table_extra extra = { .fd = fd,
					     .blocks_done = 0,
					     .blocks_failed = 0,
					     .total_blocks = num_blocks,
					     .h = h };
    int r = bt->iterate(block_table::TRANSLATION_CURRENT,
                    check_block,
                    &extra,
                    true,
                    true);
    if (r != 0) {
        // We can print more information here if necessary.
    }

    assert(extra.blocks_done == extra.total_blocks);
    printf("Finished verification. ");
    printf(" %" PRId64 " blocks checked,", extra.blocks_done);
    printf(" %" PRId64 " bad block(s) detected\n", extra.blocks_failed);
    fflush(stdout);
}

int
main(int argc, char const * const argv[])
{
    // open the file
    int r = 0;
    int dictfd;
    const char *dictfname, *outfname;
    if (argc < 3 || argc > 4) {
        fprintf(stderr, "%s: Invalid arguments.\n", argv[0]);
        fprintf(stderr, "Usage: %s <dictionary> <logfile> [report%%]\n", argv[0]);
        r = EX_USAGE;
        goto exit;
    }

    assert(argc == 3 || argc == 4);
    dictfname = argv[1];
    outfname = argv[2];
    if (argc == 4) {
        set_errno(0);
        pct = strtod(argv[3], NULL);
        assert_zero(get_maybe_error_errno());
        assert(pct > 0.0 && pct <= 100.0);
    }

    // Open the file as read-only.
    dictfd = open(dictfname, O_RDONLY | O_BINARY, S_IRWXU | S_IRWXG | S_IRWXO);
    if (dictfd < 0) {
        perror(dictfname);
        fflush(stderr);
        abort();
    }
    outf = fopen(outfname, "w");
    if (!outf) {
        perror(outfname);
        fflush(stderr);
        abort();
    }

    // body of toku_ft_serialize_init();
    num_cores = toku_os_get_number_active_processors();
    r = toku_thread_pool_create(&ft_pool, num_cores); lazy_assert_zero(r);
    assert_zero(r);

    // deserialize the header(s)
    struct ft *h1, *h2;
    deserialize_headers(dictfd, &h1, &h2);

    // walk over the block table and check blocks
    if (h1) {
        printf("Checking dictionary from header 1.\n");
        check_block_table(dictfd, &h1->blocktable, h1);
    }
    if (h2) {
        printf("Checking dictionary from header 2.\n");
        check_block_table(dictfd, &h2->blocktable, h2);
    }
    if (h1 == NULL && h2 == NULL) {
        printf("Both headers have a corruption and could not be used.\n");
    }

    toku_thread_pool_destroy(&ft_pool);
exit:
    return r;
}
