/* -*- mode: C; c-basic-offset: 4 -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id: ft-serialize.c 43686 2012-05-18 23:21:00Z leifwalsh $"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <ft-internal.h>
#include <db.h>

////////////////////////////////////////
// 1. Sets initial values
//
inline void
initialize_ftnode(FTNODE node, BLOCKNUM blocknum)
{
    node->fullhash = 0xDEADBEEF; // <CER> Is this 'spoof' ok?
    node->thisnodename = blocknum;
    node->dirty = 0;
    node->bp = NULL;
    // <CER> Can we use this initialization as a correctness assert in
    // a later function?
    node->layout_version_read_from_disk = 0;
}

// TODO:

/************************
 * In other deserialization code, we check the rb size member.  We
 * verify that it is greater than or equal to 24.  Ignoring this magic
 * number for a moment, should we put this check in its own function? *
*************************/

/////////////////////////////////////////////////////////////////////
// 2. Read and check the 'magic' bytes on disk.  Returns an error if
// the magic does not match.
//
inline int
read_and_check_magic(struct rbuf *rb)
{
    int r = 0;
    bytevec magic;
    rbuf_literal_bytes(rb, &magic, 8);
    if (memcmp(magic, "tokuleaf", 8)!=0 &&
        memcmp(magic, "tokunode", 8)!=0) {
	r = DB_BADFORMAT; // TODO: Return more meaningful error.
    }

    return r;
}

////////////////////
// 3. 
inline int
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

////////////////////
// 4. 
inline void
read_node_info(FTNODE node, struct rbuf *rb, int version)
{
    node->layout_version = version;
    node->layout_version_original = rbuf_int(rb);
    node->build_id = rbuf_int(rb);
    node->n_children = rbuf_int(rb);
}

////////////////////
// 5.
// <CER> Should these be two seperate functions?
inline void
allocate_and_read_partition_offsets(FTNODE node, struct rbuf *rb, FTNODE_DISK_DATA *ndd)
{
    XMALLOC_N(node->n_children, node->bp);
    // TODO: Fix this to use xmalloc_n
    *ndd = toku_xmalloc(node->n_children * sizeof(**ndd));
    // Read the partition locations.
    for (int i = 0; i < node->n_children; i++) {
        BP_START(*ndd, i) = rbuf_int(rb);
        BP_SIZE (*ndd, i) = rbuf_int(rb);
    }
}

////////////////////
// 6. Check the node info checksum.
inline int
check_node_info_checksum(struct rbuf *rb)
{
    int r = 0;
    // Verify checksum of header stored.
    u_int32_t checksum = x1764_memory(rb->buf, rb->ndone);
    u_int32_t stored_checksum = rbuf_int(rb);

    if (stored_checksum != checksum) {
        // TODO: dump_bad_block(rb->buf, rb->size);
	r = TOKUDB_BAD_CHECKSUM;
    }

    return r;
}


// Two functions, at this point, SHOULD be split into sperate read and
// checksum check calls: 

// 1. read_and_decompress_sub_block - this is generic, used elsewhere.

// So....
// a. call read_compressed_sub_block() directly
// then....


//


// 2. deserialize_ftnode_info() - this actually reads in the node
// 'info' fields, such as height, nodesize, etc.


/////////////////////////////////
// ?. ----
// setup_ftnode_paritions() - 
// calls :
// a. update_bfe_using_ftnode
// b. setup_partitions_using_bfe()

/////////////////////////////////
// ?. partition from sub-block deserialization.
// A.decompress_and_deserialize_worker()
// calls:
// a. read_and_decompress_sub_block (SEE ABOVE) - 
// -calls:
// -ii. read_compressed_sub_block() - Returns checksum error AND reads out buffer. 
// b. deserialize_ftnode_partition()
// -calls:
// -ii. verify_ftnode_sub_block() - JUST verifies checksum.
//
// OR
//
// B. check_and_copy_compressed_sub_block_worker
// calls:
// a. read_compressed_sub_block() - Returns checksum AND reads out of buffer.

/////////////////////////////////
// 1. first calls verify_ftnode_sub_block() which must be refactored
// into two seperate calls.
