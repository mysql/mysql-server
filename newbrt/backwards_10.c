/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id: brt-serialize.c 13852 2009-08-05 19:34:12Z yfogel $"
#ident "Copyright (c) 2007, 2008, 2009 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

/*******************
 * Purpose of this file is to provide backwards compatibility with earlier versions
 * of the file format.
 *
 * Used by brt-serialize.c.
 *
 * NOTE: All functions in this file are static.
 *       This file is included in brt-serialize.c.  It should not be compiled by itself.
 *       
 *
 */


// Given a version 10 header, create a version 11 header.
// If new memory is needed for the new header, allocate it here and free the memory of the old version header.
static int
upgrade_brtheader_10_11(brt_header_10 **brth_10, brt_header_11 ** brth_11) {
    assert((*brth_10)->layout_version == BRT_LAYOUT_VERSION_10);
    *brth_11 = *brth_10;
    *brth_10 = NULL;
    (*brth_11)->layout_version = BRT_LAYOUT_VERSION_11;
    return 0;
}


static int
deserialize_brtheader_10 (int fd, struct rbuf *rb, brt_header_10 **brth) {
    // We already know:
    //  we have an rbuf representing the header.
    //  The checksum has been validated

    //Steal rbuf (used to simplify merge, reduce diff size, and keep old code)
    struct rbuf rc = *rb;
    memset(rb, 0, sizeof(*rb));

    //Verification of initial elements.
    {
        //Check magic number
        bytevec magic;
        rbuf_literal_bytes(&rc, &magic, 8);
        assert(memcmp(magic,"tokudata",8)==0);
    }
 

    brt_header_10 *CALLOC(h);
    if (h==0) return errno;
    int ret=-1;
    if (0) { died1: toku_free(h); return ret; }
    h->type = BRTHEADER_CURRENT;
    h->checkpoint_header = NULL;
    h->dirty=0;
    h->panic = 0;
    h->panic_string = 0;
    list_init(&h->live_brts);
    list_init(&h->zombie_brts);
    //version MUST be in network order on disk regardless of disk order
    h->layout_version = rbuf_network_int(&rc);
    assert(h->layout_version==BRT_LAYOUT_VERSION_10);

    //Size MUST be in network order regardless of disk order.
    u_int32_t size = rbuf_network_int(&rc);
    assert(size==rc.size);

    bytevec tmp_byte_order_check;
    rbuf_literal_bytes(&rc, &tmp_byte_order_check, 8); //Must not translate byte order
    int64_t byte_order_stored = *(int64_t*)tmp_byte_order_check;
    assert(byte_order_stored == toku_byte_order_host);

    h->checkpoint_count = rbuf_ulonglong(&rc);
    h->checkpoint_lsn   = rbuf_lsn(&rc);
    h->nodesize      = rbuf_int(&rc);
    DISKOFF translation_address_on_disk = rbuf_diskoff(&rc);
    DISKOFF translation_size_on_disk    = rbuf_diskoff(&rc);
    assert(translation_address_on_disk>0);
    assert(translation_size_on_disk>0);

    // printf("%s:%d translated_blocknum_limit=%ld, block_translation_address_on_disk=%ld\n", __FILE__, __LINE__, h->translated_blocknum_limit, h->block_translation_address_on_disk);
    //Load translation table
    {
        lock_for_pwrite();
        unsigned char *XMALLOC_N(translation_size_on_disk, tbuf);
        {
            // This cast is messed up in 32-bits if the block translation table is ever more than 4GB.  But in that case, the translation table itself won't fit in main memory.
            ssize_t r = pread(fd, tbuf, translation_size_on_disk, translation_address_on_disk);
            assert(r==translation_size_on_disk);
        }
        unlock_for_pwrite();
        // Create table and read in data.
        toku_blocktable_create_from_buffer(&h->blocktable,
                                           translation_address_on_disk,
                                           translation_size_on_disk,
                                           tbuf);
        toku_free(tbuf);
    }

    h->root = rbuf_blocknum(&rc);
    h->root_hash.valid = FALSE;
    h->flags = rbuf_int(&rc);
    deserialize_descriptor_from(fd, h, &h->descriptor);
    (void)rbuf_int(&rc); //Read in checksum and ignore (already verified).
    if (rc.ndone!=rc.size) {ret = EINVAL; goto died1;}
    toku_free(rc.buf);
    rc.buf = NULL;
    *brth = h;
    return 0;
}
