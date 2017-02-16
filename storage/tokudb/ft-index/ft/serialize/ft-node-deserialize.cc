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

#include <config.h>

#include "ft/node.h"
#include "ft/ft-internal.h"
#include "ft/serialize/ft_node-serialize.h"

/*
 * ft-node-deserialize.c -
 *      This file contains functions used by deserializtion
 *  code paths in and out of the engine.  The functions can,
 *  essentially, be broken up into two types.  Some of these
 *  functions return error codes based expected values inside
 *  the fractal tree node, others merely read the specific 
 *  quantities of bytes out of the buffer.  It is expeceted
 *  that these will be called in the correct order by users
 *  of these functions/this API.
 *
 */

// Sets initial values for the given fractal tree node to be
// deserialized
void
initialize_ftnode(FTNODE node, BLOCKNUM blocknum)
{
    node->fullhash = 0xDEADBEEF; // <CER> Is this 'spoof' ok?
    node->blocknum = blocknum;
    node->dirty = 0;
    node->bp = NULL;
    // <CER> Can we use this initialization as a correctness assert in
    // a later function?
    node->layout_version_read_from_disk = 0;
}

/************************
 * TODO: In other deserialization code, we check the rb size member.  We
 * verify that it is greater than or equal to 24.  Ignoring this magic
 * number for a moment, should we put this check in its own function? *
*************************/


// Read and check the 'magic' bytes on disk.  Returns an error if
// the magic does not match.
int
read_and_check_magic(struct rbuf *rb)
{
    int r = 0;
    const void *magic;
    rbuf_literal_bytes(rb, &magic, 8);
    if (memcmp(magic, "tokuleaf", 8)!=0 &&
        memcmp(magic, "tokunode", 8)!=0) {
        r = DB_BADFORMAT; // TODO: Return more meaningful error.
    }

    return r;
}

// Read the version number from the given buffer
// and returns an error if the version is too old.
int
read_and_check_version(FTNODE node, struct rbuf *rb)
{
    int r = 0;
    int version = rbuf_int(rb);
    node->layout_version_read_from_disk = version;
    if (version < FT_LAYOUT_MIN_SUPPORTED_VERSION) {
        r = 1; // TODO: Better error reporting.
    }

    return r;
}

// Reads the basic version, build, and child info from
// the given buffer.
void
read_node_info(FTNODE node, struct rbuf *rb, int version)
{
    node->layout_version = version;
    node->layout_version_original = rbuf_int(rb);
    node->build_id = rbuf_int(rb);
    node->n_children = rbuf_int(rb);
}

// Allocates the partitions based on the given node's nubmer
// of children.  It then reads, out of the given buffer,
// the start and size of each child partition.
// TODO: Should these be two seperate functions?
void
allocate_and_read_partition_offsets(FTNODE node, struct rbuf *rb, FTNODE_DISK_DATA *ndd)
{
    XMALLOC_N(node->n_children, node->bp);
    // TODO: Fix this to use xmalloc_n
    XMALLOC_N(node->n_children, *ndd);
    // Read the partition locations.
    for (int i = 0; i < node->n_children; i++) {
        BP_START(*ndd, i) = rbuf_int(rb);
        BP_SIZE (*ndd, i) = rbuf_int(rb);
    }
}

// Compares checksum of stored (in the given buffer) checksum
// and the checksum of the buffer itself.  If these are NOT
// equal, this function returns an appropriate error code.
int
check_node_info_checksum(struct rbuf *rb)
{
    int r = 0;
    // Verify checksum of header stored.
    uint32_t checksum = toku_x1764_memory(rb->buf, rb->ndone);
    uint32_t stored_checksum = rbuf_int(rb);

    if (stored_checksum != checksum) {
        // TODO: dump_bad_block(rb->buf, rb->size);
        r = TOKUDB_BAD_CHECKSUM;
    }

    return r;
}

// Reads node info from older (13 and 14) fractal tree nodes
// out of the given buffer.
void
read_legacy_node_info(FTNODE node, struct rbuf *rb, int version)
{
    (void)rbuf_int(rb); // 1. nodesize
    node->flags = rbuf_int(rb);    // 2. flags
    node->height = rbuf_int(rb);   // 3. height
    
    // If the version is less than 14, there are two extra ints here.
    // we would need to ignore them if they are there.
    if (version == FT_LAYOUT_VERSION_13) {
        (void) rbuf_int(rb);       // 4. rand4
        (void) rbuf_int(rb);       // 5. local
    }
}

// Assuming the given buffer is in the correct position,
// this checks to see if the stored checksum matches the
// checksum of the entire buffer.
int
check_legacy_end_checksum(struct rbuf *rb)
{
    int r = 0;
    uint32_t expected_xsum = rbuf_int(rb);
    uint32_t actual_xsum = toku_x1764_memory(rb->buf, rb->size - 4);
    if (expected_xsum != actual_xsum) {
        r = TOKUDB_BAD_CHECKSUM;
    }
    
    return r;
}
