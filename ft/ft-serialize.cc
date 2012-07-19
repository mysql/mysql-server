/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "includes.h"
#include "sort.h"
#include "threadpool.h"
#include <compress.h>

#if defined(HAVE_CILK)
#include <cilk/cilk.h>
#define cilk_worker_count (__cilkrts_get_nworkers())
#else
#define cilk_spawn
#define cilk_sync
#define cilk_for for
#define cilk_worker_count 1
#endif

// not version-sensitive because we only serialize a descriptor using the current layout_version
u_int32_t
toku_serialize_descriptor_size(const DESCRIPTOR desc) {
    //Checksum NOT included in this.  Checksum only exists in header's version.
    u_int32_t size = 4; // four bytes for size of descriptor
    size += desc->dbt.size;
    return size;
}

static u_int32_t
deserialize_descriptor_size(const DESCRIPTOR desc, int layout_version) {
    //Checksum NOT included in this.  Checksum only exists in header's version.
    u_int32_t size = 4; // four bytes for size of descriptor
    if (layout_version == FT_LAYOUT_VERSION_13)
        size += 4;   // for version 13, include four bytes of "version"
    size += desc->dbt.size;
    return size;
}

void
toku_serialize_descriptor_contents_to_wbuf(struct wbuf *wb, const DESCRIPTOR desc) {
    wbuf_bytes(wb, desc->dbt.data, desc->dbt.size);
}

//Descriptor is written to disk during toku_ft_handle_open iff we have a new (or changed)
//descriptor.
//Descriptors are NOT written during the header checkpoint process.
void
toku_serialize_descriptor_contents_to_fd(int fd, const DESCRIPTOR desc, DISKOFF offset) {
    // make the checksum
    int64_t size = toku_serialize_descriptor_size(desc)+4; //4 for checksum
    struct wbuf w;
    wbuf_init(&w, toku_xmalloc(size), size);
    toku_serialize_descriptor_contents_to_wbuf(&w, desc);
    {
        //Add checksum
        u_int32_t checksum = x1764_finish(&w.checksum);
        wbuf_int(&w, checksum);
    }
    lazy_assert(w.ndone==w.size);
    {
        //Actual Write translation table
        toku_os_full_pwrite(fd, w.buf, size, offset);
    }
    toku_free(w.buf);
}

static void
deserialize_descriptor_from_rbuf(struct rbuf *rb, DESCRIPTOR desc, int layout_version) {
    if (layout_version <= FT_LAYOUT_VERSION_13) {
        // in older versions of TokuDB the Descriptor had a 4 byte
        // version, which we skip over
        (void) rbuf_int(rb);
    }

    u_int32_t size;
    bytevec data;
    rbuf_bytes(rb, &data, &size);
    bytevec data_copy = data;
    if (size > 0) {
        data_copy = toku_memdup(data, size); //Cannot keep the reference from rbuf. Must copy.
        lazy_assert(data_copy);
    } else {
        lazy_assert(size==0);
        data_copy = NULL;
    }
    toku_fill_dbt(&desc->dbt, data_copy, size);
}

static int
deserialize_descriptor_from(int fd, BLOCK_TABLE bt, DESCRIPTOR desc, int layout_version) {
    int r = 0;
    DISKOFF offset;
    DISKOFF size;
    unsigned char *dbuf = NULL;
    toku_get_descriptor_offset_size(bt, &offset, &size);
    memset(desc, 0, sizeof(*desc));
    if (size > 0) {
        lazy_assert(size>=4); //4 for checksum
        {
            XMALLOC_N(size, dbuf);
            {
                ssize_t sz_read = toku_os_pread(fd, dbuf, size, offset);
                lazy_assert(sz_read==size);
            }
            {
                // check the checksum
                u_int32_t x1764 = x1764_memory(dbuf, size-4);
                //printf("%s:%d read from %ld (x1764 offset=%ld) size=%ld\n", __FILE__, __LINE__, block_translation_address_on_disk, offset, block_translation_size_on_disk);
                u_int32_t stored_x1764 = toku_dtoh32(*(int*)(dbuf + size-4));
                if (x1764 != stored_x1764) {
                    fprintf(stderr, "Descriptor checksum failure: calc=0x%08x read=0x%08x\n", x1764, stored_x1764);
                    r = TOKUDB_BAD_CHECKSUM;
                    toku_free(dbuf);
                    goto exit;
                }
            }
            {
                struct rbuf rb = {.buf = dbuf, .size = (unsigned int) size, .ndone = 0};
                //Not temporary; must have a toku_memdup'd copy.
                deserialize_descriptor_from_rbuf(&rb, desc, layout_version);
            }
            lazy_assert(deserialize_descriptor_size(desc, layout_version)+4 == size);
            toku_free(dbuf);
        }
    }
exit:
    return r;
}

// We only deserialize brt header once and then share everything with all the brts.
int
deserialize_ft_versioned(int fd, struct rbuf *rb, FT *ftp, uint32_t version)
{
    int r;
    FT ft = NULL;
    invariant(version >= FT_LAYOUT_MIN_SUPPORTED_VERSION);
    invariant(version <= FT_LAYOUT_VERSION);
    // We already know:
    //  we have an rbuf representing the header.
    //  The checksum has been validated

    //Verification of initial elements.
    //Check magic number
    bytevec magic;
    rbuf_literal_bytes(rb, &magic, 8);
    lazy_assert(memcmp(magic,"tokudata",8)==0);

    XCALLOC(ft);
    ft->checkpoint_header = NULL;
    ft->panic = 0;
    ft->panic_string = 0;
    toku_list_init(&ft->live_ft_handles);

    //version MUST be in network order on disk regardless of disk order
    ft->layout_version_read_from_disk = rbuf_network_int(rb);
    invariant(ft->layout_version_read_from_disk >= FT_LAYOUT_MIN_SUPPORTED_VERSION);
    invariant(ft->layout_version_read_from_disk <= FT_LAYOUT_VERSION);

    //build_id MUST be in network order on disk regardless of disk order
    uint32_t build_id;
    build_id = rbuf_network_int(rb);

    //Size MUST be in network order regardless of disk order.
    u_int32_t size;
    size = rbuf_network_int(rb);
    lazy_assert(size == rb->size);

    bytevec tmp_byte_order_check;
    lazy_assert((sizeof tmp_byte_order_check) >= 8);
    rbuf_literal_bytes(rb, &tmp_byte_order_check, 8); //Must not translate byte order
    int64_t byte_order_stored;
    byte_order_stored = *(int64_t*)tmp_byte_order_check;
    lazy_assert(byte_order_stored == toku_byte_order_host);

    uint64_t checkpoint_count;
    checkpoint_count = rbuf_ulonglong(rb);
    LSN checkpoint_lsn;
    checkpoint_lsn = rbuf_lsn(rb);
    unsigned nodesize;
    nodesize = rbuf_int(rb);
    DISKOFF translation_address_on_disk;
    translation_address_on_disk = rbuf_diskoff(rb);
    DISKOFF translation_size_on_disk;
    translation_size_on_disk = rbuf_diskoff(rb);
    lazy_assert(translation_address_on_disk > 0);
    lazy_assert(translation_size_on_disk > 0);

    // initialize the tree lock
    toku_ft_init_treelock(ft);
    toku_ft_init_reflock(ft);

    //Load translation table
    {
        unsigned char *XMALLOC_N(translation_size_on_disk, tbuf);
        {
            // This cast is messed up in 32-bits if the block translation
            // table is ever more than 4GB.  But in that case, the
            // translation table itself won't fit in main memory.
            ssize_t readsz = toku_os_pread(fd, tbuf, translation_size_on_disk,
                                           translation_address_on_disk);
            lazy_assert(readsz == translation_size_on_disk);
        }
        // Create table and read in data.
        r = toku_blocktable_create_from_buffer(fd,
                                               &ft->blocktable,
                                               translation_address_on_disk,
                                               translation_size_on_disk,
                                               tbuf);
        toku_free(tbuf);
        if (r != 0) {
            goto exit;
        }
    }

    BLOCKNUM root_blocknum;
    root_blocknum = rbuf_blocknum(rb);
    unsigned flags;
    flags = rbuf_int(rb);
    if (ft->layout_version_read_from_disk <= FT_LAYOUT_VERSION_13) {
        // deprecate 'TOKU_DB_VALCMP_BUILTIN'. just remove the flag
        flags &= ~TOKU_DB_VALCMP_BUILTIN_13;
    }
    int layout_version_original;
    layout_version_original = rbuf_int(rb);
    uint32_t build_id_original;
    build_id_original = rbuf_int(rb);
    uint64_t time_of_creation;
    time_of_creation = rbuf_ulonglong(rb);
    uint64_t time_of_last_modification;
    time_of_last_modification = rbuf_ulonglong(rb);

    if (ft->layout_version_read_from_disk <= FT_LAYOUT_VERSION_18) {
        // 17 was the last version with these fields, we no longer store
        // them, so read and discard them
        (void) rbuf_ulonglong(rb);  // num_blocks_to_upgrade_13
        if (ft->layout_version_read_from_disk >= FT_LAYOUT_VERSION_15) {
            (void) rbuf_ulonglong(rb);  // num_blocks_to_upgrade_14
        }
    }

    // fake creation during the last checkpoint
    TXNID root_xid_that_created;
    root_xid_that_created = checkpoint_lsn.lsn;
    if (ft->layout_version_read_from_disk >= FT_LAYOUT_VERSION_14) {
        rbuf_TXNID(rb, &root_xid_that_created);
    }

    // TODO(leif): get this to default to what's specified, not the
    // hard-coded default
    unsigned basementnodesize;
    basementnodesize = FT_DEFAULT_BASEMENT_NODE_SIZE;
    uint64_t time_of_last_verification;
    time_of_last_verification = 0;
    if (ft->layout_version_read_from_disk >= FT_LAYOUT_VERSION_15) {
        basementnodesize = rbuf_int(rb);
        time_of_last_verification = rbuf_ulonglong(rb);
    }

    STAT64INFO_S on_disk_stats;
    on_disk_stats = ZEROSTATS;
    uint64_t time_of_last_optimize_begin;
    time_of_last_optimize_begin = 0;
    uint64_t time_of_last_optimize_end;
    time_of_last_optimize_end = 0;
    uint32_t count_of_optimize_in_progress;
    count_of_optimize_in_progress = 0;
    MSN msn_at_start_of_last_completed_optimize;
    msn_at_start_of_last_completed_optimize = ZERO_MSN;
    if (ft->layout_version_read_from_disk >= FT_LAYOUT_VERSION_18) {
        on_disk_stats.numrows = rbuf_ulonglong(rb);
        on_disk_stats.numbytes = rbuf_ulonglong(rb);
        ft->in_memory_stats = on_disk_stats;
        time_of_last_optimize_begin = rbuf_ulonglong(rb);
        time_of_last_optimize_end = rbuf_ulonglong(rb);
        count_of_optimize_in_progress = rbuf_int(rb);
        msn_at_start_of_last_completed_optimize = rbuf_msn(rb);
    }

    enum toku_compression_method compression_method;
    MSN highest_unused_msn_for_upgrade;
    highest_unused_msn_for_upgrade.msn = (MIN_MSN.msn - 1);
    if (ft->layout_version_read_from_disk >= FT_LAYOUT_VERSION_19) {
        unsigned char method = rbuf_char(rb);
        compression_method = (enum toku_compression_method) method;
        highest_unused_msn_for_upgrade = rbuf_msn(rb);
    } else {
        // we hard coded zlib until 5.2, then quicklz in 5.2
        if (ft->layout_version_read_from_disk < FT_LAYOUT_VERSION_18) {
            compression_method = TOKU_ZLIB_METHOD;
        } else {
            compression_method = TOKU_QUICKLZ_METHOD;
        }
    }

    (void) rbuf_int(rb); //Read in checksum and ignore (already verified).
    if (rb->ndone != rb->size) {
        fprintf(stderr, "Header size did not match contents.\n");
        r = EINVAL;
        goto exit;
    }

    {
        struct ft_header h = {
            .type = FT_CURRENT,
            .dirty = 0,
            .checkpoint_count = checkpoint_count,
            .checkpoint_lsn = checkpoint_lsn,
            .layout_version = FT_LAYOUT_VERSION,
            .layout_version_original = layout_version_original,
            .build_id = build_id,
            .build_id_original = build_id_original,
            .time_of_creation = time_of_creation,
            .root_xid_that_created = root_xid_that_created,
            .time_of_last_modification = time_of_last_modification,
            .time_of_last_verification = time_of_last_verification,
            .root_blocknum = root_blocknum,
            .flags = flags,
            .nodesize = nodesize,
            .basementnodesize = basementnodesize,
            .compression_method = compression_method,
            .highest_unused_msn_for_upgrade = highest_unused_msn_for_upgrade,
            .time_of_last_optimize_begin = time_of_last_optimize_begin,
            .time_of_last_optimize_end = time_of_last_optimize_end,
            .count_of_optimize_in_progress = count_of_optimize_in_progress,
            .count_of_optimize_in_progress_read_from_disk = count_of_optimize_in_progress,
            .msn_at_start_of_last_completed_optimize = msn_at_start_of_last_completed_optimize,
            .on_disk_stats = on_disk_stats
        };
        XMEMDUP(ft->h, &h);
    }

    if (ft->layout_version_read_from_disk < FT_LAYOUT_VERSION_18) {
        // This needs ft->h to be non-null, so we have to do it after we
        // read everything else.
        r = toku_upgrade_subtree_estimates_to_stat64info(fd, ft);
        if (r != 0) {
            goto exit;
        }
    }

    invariant((uint32_t) ft->layout_version_read_from_disk == version);
    r = deserialize_descriptor_from(fd, ft->blocktable, &ft->descriptor, version);
    if (r != 0) {
        goto exit;
    }
    // copy descriptor to cmp_descriptor for #4541
    ft->cmp_descriptor.dbt.size = ft->descriptor.dbt.size;
    ft->cmp_descriptor.dbt.data = toku_xmemdup(ft->descriptor.dbt.data, ft->descriptor.dbt.size);
    // Version 13 descriptors had an extra 4 bytes that we don't read
    // anymore.  Since the header is going to think it's the current
    // version if it gets written out, we need to write the descriptor in
    // the new format (without those bytes) before that happens.
    if (version <= FT_LAYOUT_VERSION_13) {
        toku_ft_update_descriptor_with_fd(ft, &ft->cmp_descriptor, fd);
    }
    r = 0;
exit:
    if (r != 0 && ft != NULL) {
        toku_free(ft);
        ft = NULL;
    }
    *ftp = ft;
    return r;
}

static u_int32_t
serialize_ft_min_size (u_int32_t version) {
    u_int32_t size = 0;


    switch(version) {
    case FT_LAYOUT_VERSION_20:
    case FT_LAYOUT_VERSION_19:
        size += 1; // compression method
        size += sizeof(uint64_t);  // highest_unused_msn_for_upgrade
    case FT_LAYOUT_VERSION_18:
        size += sizeof(uint64_t);  // time_of_last_optimize_begin
        size += sizeof(uint64_t);  // time_of_last_optimize_end
        size += sizeof(uint32_t);  // count_of_optimize_in_progress
        size += sizeof(MSN);       // msn_at_start_of_last_completed_optimize
        size -= 8;                 // removed num_blocks_to_upgrade_14
        size -= 8;                 // removed num_blocks_to_upgrade_13
    case FT_LAYOUT_VERSION_17:
        size += 16;
        invariant(sizeof(STAT64INFO_S) == 16);
    case FT_LAYOUT_VERSION_16:
    case FT_LAYOUT_VERSION_15:
        size += 4;  // basement node size
        size += 8;  // num_blocks_to_upgrade_14 (previously num_blocks_to_upgrade, now one int each for upgrade from 13, 14
        size += 8;  // time of last verification
    case FT_LAYOUT_VERSION_14:
        size += 8;  //TXNID that created
    case FT_LAYOUT_VERSION_13:
        size += ( 4 // build_id
                  +4 // build_id_original
                  +8 // time_of_creation
                  +8 // time_of_last_modification
            );
        // fall through
    case FT_LAYOUT_VERSION_12:
        size += (+8 // "tokudata"
                 +4 // version
                 +4 // original_version
                 +4 // size
                 +8 // byte order verification
                 +8 // checkpoint_count
                 +8 // checkpoint_lsn
                 +4 // tree's nodesize
                 +8 // translation_size_on_disk
                 +8 // translation_address_on_disk
                 +4 // checksum
                 +8 // Number of blocks in old version.
                 +8 // diskoff
                 +4 // flags
            );
        break;
    default:
        lazy_assert(FALSE);
    }
    lazy_assert(size <= BLOCK_ALLOCATOR_HEADER_RESERVE);
    return size;
}

// Simply reading the raw bytes of the header into an rbuf is insensitive
// to disk format version.  If that ever changes, then modify this.
//
// TOKUDB_DICTIONARY_NO_HEADER means we can overwrite everything in the
// file AND the header is useless
int
deserialize_ft_from_fd_into_rbuf(int fd,
                                 toku_off_t offset_of_header,
                                 struct rbuf *rb,
                                 u_int64_t *checkpoint_count,
                                 LSN *checkpoint_lsn,
                                 u_int32_t * version_p)
{
    int r = 0;
    const int64_t prefix_size = 8 + // magic ("tokudata")
                                4 + // version
                                4 + // build_id
                                4;  // size
    unsigned char prefix[prefix_size];
    rb->buf = NULL;
    int64_t n = toku_os_pread(fd, prefix, prefix_size, offset_of_header);
    if (n != prefix_size) {
        if (n==0) {
            r = TOKUDB_DICTIONARY_NO_HEADER;
        } else if (n<0) {
            r = get_error_errno();
        } else {
            r = EINVAL;
        }
        goto exit;
    }

    rbuf_init(rb, prefix, prefix_size);

    //Check magic number
    bytevec magic;
    rbuf_literal_bytes(rb, &magic, 8);
    if (memcmp(magic,"tokudata",8)!=0) {
        if ((*(u_int64_t*)magic) == 0) {
            r = TOKUDB_DICTIONARY_NO_HEADER;
        } else {
            r = EINVAL; //Not a tokudb file! Do not use.
        }
        goto exit;
    }

    //Version MUST be in network order regardless of disk order.
    u_int32_t version;
    version = rbuf_network_int(rb);
    *version_p = version;
    if (version < FT_LAYOUT_MIN_SUPPORTED_VERSION) {
        r = TOKUDB_DICTIONARY_TOO_OLD; //Cannot use
        goto exit;
    } else if (version > FT_LAYOUT_VERSION) {
        r = TOKUDB_DICTIONARY_TOO_NEW; //Cannot use
        goto exit;
    }

    //build_id MUST be in network order regardless of disk order.
    u_int32_t build_id __attribute__((__unused__));
    build_id = rbuf_network_int(rb);
    int64_t min_header_size;
    min_header_size = serialize_ft_min_size(version);

    //Size MUST be in network order regardless of disk order.
    u_int32_t size;
    size = rbuf_network_int(rb);
    //If too big, it is corrupt.  We would probably notice during checksum
    //but may have to do a multi-gigabyte malloc+read to find out.
    //If its too small reading rbuf would crash, so verify.
    if (size > BLOCK_ALLOCATOR_HEADER_RESERVE || size < min_header_size) {
        r = TOKUDB_DICTIONARY_NO_HEADER;
        goto exit;
    }

    lazy_assert(rb->ndone==prefix_size);
    rb->size = size;
    XMALLOC_N(rb->size, rb->buf);

    n = toku_os_pread(fd, rb->buf, rb->size, offset_of_header);
    if (n != rb->size) {
        if (n < 0) {
            r = get_error_errno();
        } else {
            r = EINVAL; //Header might be useless (wrong size) or could be a disk read error.
        }
        goto exit;
    }
    //It's version 14 or later.  Magic looks OK.
    //We have an rbuf that represents the header.
    //Size is within acceptable bounds.

    //Verify checksum (FT_LAYOUT_VERSION_13 or later, when checksum function changed)
    u_int32_t calculated_x1764;
    calculated_x1764 = x1764_memory(rb->buf, rb->size-4);
    u_int32_t stored_x1764;
    stored_x1764 = toku_dtoh32(*(int*)(rb->buf+rb->size-4));
    if (calculated_x1764 != stored_x1764) {
        r = TOKUDB_BAD_CHECKSUM; //Header useless
        fprintf(stderr, "Header checksum failure: calc=0x%08x read=0x%08x\n", calculated_x1764, stored_x1764);
        goto exit;
    }

    //Verify byte order
    bytevec tmp_byte_order_check;
    lazy_assert((sizeof toku_byte_order_host) == 8);
    rbuf_literal_bytes(rb, &tmp_byte_order_check, 8); //Must not translate byte order
    int64_t byte_order_stored;
    byte_order_stored = *(int64_t*)tmp_byte_order_check;
    if (byte_order_stored != toku_byte_order_host) {
        r = TOKUDB_DICTIONARY_NO_HEADER; //Cannot use dictionary
        goto exit;
    }

    //Load checkpoint count
    *checkpoint_count = rbuf_ulonglong(rb);
    *checkpoint_lsn = rbuf_lsn(rb);
    //Restart at beginning during regular deserialization
    rb->ndone = 0;

exit:
    if (r != 0 && rb->buf != NULL) {
        if (rb->buf != prefix) { // don't free prefix, it's stack alloc'd
            toku_free(rb->buf);
        }
        rb->buf = NULL;
    }
    return r;
}

// Read ft from file into struct.  Read both headers and use one.
// We want the latest acceptable header whose checkpoint_lsn is no later
// than max_acceptable_lsn.
int
toku_deserialize_ft_from(int fd,
                         LSN max_acceptable_lsn,
                         FT *ft)
{
    struct rbuf rb_0;
    struct rbuf rb_1;
    u_int64_t checkpoint_count_0;
    u_int64_t checkpoint_count_1;
    LSN checkpoint_lsn_0;
    LSN checkpoint_lsn_1;
    u_int32_t version_0, version_1, version = 0;
    BOOL h0_acceptable = FALSE;
    BOOL h1_acceptable = FALSE;
    struct rbuf *rb = NULL;
    int r0, r1, r;

    toku_off_t header_0_off = 0;
    r0 = deserialize_ft_from_fd_into_rbuf(fd, header_0_off, &rb_0, &checkpoint_count_0, &checkpoint_lsn_0, &version_0);
    if (r0 == 0 && checkpoint_lsn_0.lsn <= max_acceptable_lsn.lsn) {
        h0_acceptable = TRUE;
    }

    toku_off_t header_1_off = BLOCK_ALLOCATOR_HEADER_RESERVE;
    r1 = deserialize_ft_from_fd_into_rbuf(fd, header_1_off, &rb_1, &checkpoint_count_1, &checkpoint_lsn_1, &version_1);
    if (r1 == 0 && checkpoint_lsn_1.lsn <= max_acceptable_lsn.lsn) {
        h1_acceptable = TRUE;
    }

    // if either header is too new, the dictionary is unreadable
    if (r0 == TOKUDB_DICTIONARY_TOO_NEW || r1 == TOKUDB_DICTIONARY_TOO_NEW ||
        !(h0_acceptable || h1_acceptable)) {
        // We were unable to read either header or at least one is too
        // new.  Certain errors are higher priority than others. Order of
        // these if/else if is important.
        if (r0 == TOKUDB_DICTIONARY_TOO_NEW || r1 == TOKUDB_DICTIONARY_TOO_NEW) {
            r = TOKUDB_DICTIONARY_TOO_NEW;
        } else if (r0 == TOKUDB_DICTIONARY_TOO_OLD || r1 == TOKUDB_DICTIONARY_TOO_OLD) {
            r = TOKUDB_DICTIONARY_TOO_OLD;
        } else if (r0 == TOKUDB_BAD_CHECKSUM && r1 == TOKUDB_BAD_CHECKSUM) {
            fprintf(stderr, "Both header checksums failed.\n");
            r = TOKUDB_BAD_CHECKSUM;
        } else if (r0 == TOKUDB_DICTIONARY_NO_HEADER || r1 == TOKUDB_DICTIONARY_NO_HEADER) {
            r = TOKUDB_DICTIONARY_NO_HEADER;
        } else {
            r = r0 ? r0 : r1; //Arbitrarily report the error from the
                              //first header, unless it's readable
        }

        // it should not be possible for both headers to be later than the max_acceptable_lsn
        invariant(!((r0==0 && checkpoint_lsn_0.lsn > max_acceptable_lsn.lsn) &&
                    (r1==0 && checkpoint_lsn_1.lsn > max_acceptable_lsn.lsn)));
        invariant(r!=0);
        goto exit;
    }

    if (h0_acceptable && h1_acceptable) {
        if (checkpoint_count_0 > checkpoint_count_1) {
            invariant(checkpoint_count_0 == checkpoint_count_1 + 1);
            invariant(version_0 >= version_1);
            rb = &rb_0;
            version = version_0;
        }
        else {
            invariant(checkpoint_count_1 == checkpoint_count_0 + 1);
            invariant(version_1 >= version_0);
            rb = &rb_1;
            version = version_1;
        }
    } else if (h0_acceptable) {
        if (r1 == TOKUDB_BAD_CHECKSUM) {
            // print something reassuring
            fprintf(stderr, "Header 2 checksum failed, but header 1 ok.  Proceeding.\n");
        }
        rb = &rb_0;
        version = version_0;
    } else if (h1_acceptable) {
        if (r0 == TOKUDB_BAD_CHECKSUM) {
            // print something reassuring
            fprintf(stderr, "Header 1 checksum failed, but header 2 ok.  Proceeding.\n");
        }
        rb = &rb_1;
        version = version_1;
    }

    invariant(rb);
    r = deserialize_ft_versioned(fd, rb, ft, version);

exit:
    if (rb_0.buf) {
        toku_free(rb_0.buf);
    }
    if (rb_1.buf) {
        toku_free(rb_1.buf);
    }
    return r;
}


int toku_serialize_ft_size (FT_HEADER h) {
    u_int32_t size = serialize_ft_min_size(h->layout_version);
    //There is no dynamic data.
    lazy_assert(size <= BLOCK_ALLOCATOR_HEADER_RESERVE);
    return size;
}


int toku_serialize_ft_to_wbuf (
    struct wbuf *wbuf, 
    FT_HEADER h, 
    DISKOFF translation_location_on_disk, 
    DISKOFF translation_size_on_disk
    ) 
{
    wbuf_literal_bytes(wbuf, "tokudata", 8);
    wbuf_network_int  (wbuf, h->layout_version); //MUST be in network order regardless of disk order
    wbuf_network_int  (wbuf, BUILD_ID); //MUST be in network order regardless of disk order
    wbuf_network_int  (wbuf, wbuf->size); //MUST be in network order regardless of disk order
    wbuf_literal_bytes(wbuf, &toku_byte_order_host, 8); //Must not translate byte order
    wbuf_ulonglong(wbuf, h->checkpoint_count);
    wbuf_LSN    (wbuf, h->checkpoint_lsn);
    wbuf_int    (wbuf, h->nodesize);

    wbuf_DISKOFF(wbuf, translation_location_on_disk);
    wbuf_DISKOFF(wbuf, translation_size_on_disk);
    wbuf_BLOCKNUM(wbuf, h->root_blocknum);
    wbuf_int(wbuf, h->flags);
    wbuf_int(wbuf, h->layout_version_original);
    wbuf_int(wbuf, h->build_id_original);
    wbuf_ulonglong(wbuf, h->time_of_creation);
    wbuf_ulonglong(wbuf, h->time_of_last_modification);
    wbuf_TXNID(wbuf, h->root_xid_that_created);
    wbuf_int(wbuf, h->basementnodesize);
    wbuf_ulonglong(wbuf, h->time_of_last_verification);
    wbuf_ulonglong(wbuf, h->on_disk_stats.numrows);
    wbuf_ulonglong(wbuf, h->on_disk_stats.numbytes);
    wbuf_ulonglong(wbuf, h->time_of_last_optimize_begin);
    wbuf_ulonglong(wbuf, h->time_of_last_optimize_end);
    wbuf_int(wbuf, h->count_of_optimize_in_progress);
    wbuf_MSN(wbuf, h->msn_at_start_of_last_completed_optimize);
    wbuf_char(wbuf, (unsigned char) h->compression_method);
    wbuf_MSN(wbuf, h->highest_unused_msn_for_upgrade);
    u_int32_t checksum = x1764_finish(&wbuf->checksum);
    wbuf_int(wbuf, checksum);
    lazy_assert(wbuf->ndone == wbuf->size);
    return 0;
}

int toku_serialize_ft_to (int fd, FT_HEADER h, BLOCK_TABLE blocktable, CACHEFILE cf) {
    int rr = 0;
    lazy_assert(h->type==FT_CHECKPOINT_INPROGRESS);
    struct wbuf w_translation;
    int64_t size_translation;
    int64_t address_translation;
    {
        //Must serialize translation first, to get address,size for header.
        toku_serialize_translation_to_wbuf(blocktable, fd, &w_translation,
                                                   &address_translation,
                                                   &size_translation);
        lazy_assert(size_translation==w_translation.size);
    }
    struct wbuf w_main;
    unsigned int size_main = toku_serialize_ft_size(h);
    {
        wbuf_init(&w_main, toku_xmalloc(size_main), size_main);
        {
            int r=toku_serialize_ft_to_wbuf(&w_main, h, address_translation, size_translation);
            lazy_assert_zero(r);
        }
        lazy_assert(w_main.ndone==size_main);
    }
    {
        //Actual Write translation table
        toku_os_full_pwrite(fd, w_translation.buf,
                                size_translation, address_translation);
    }
    {
        //Everything but the header MUST be on disk before header starts.
        //Otherwise we will think the header is good and some blocks might not
        //yet be on disk.
        //If the header has a cachefile we need to do cachefile fsync (to
        //prevent crash if we redirected to dev null)
        //If there is no cachefile we still need to do an fsync.
        if (cf) {
            rr = toku_cachefile_fsync(cf);
        }
        else {
            rr = toku_file_fsync(fd);
        }
        if (rr==0) {
            //Alternate writing header to two locations:
            //   Beginning (0) or BLOCK_ALLOCATOR_HEADER_RESERVE
            toku_off_t main_offset;
            main_offset = (h->checkpoint_count & 0x1) ? 0 : BLOCK_ALLOCATOR_HEADER_RESERVE;
            toku_os_full_pwrite(fd, w_main.buf, w_main.ndone, main_offset);
        }
    }
    toku_free(w_main.buf);
    toku_free(w_translation.buf);
    return rr;
}
