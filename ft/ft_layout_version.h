/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#ifndef FT_LAYOUT_VERSION_H
#define FT_LAYOUT_VERSION_H

#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

//Must be defined before other recursive headers could include logger.h
enum ft_layout_version_e {
    FT_LAYOUT_VERSION_5 = 5,
    FT_LAYOUT_VERSION_6 = 6,   // Diff from 5 to 6:  Add leafentry_estimate
    FT_LAYOUT_VERSION_7 = 7,   // Diff from 6 to 7:  Add exact-bit to leafentry_estimate #818, add magic to header #22, add per-subdatase flags #333
    FT_LAYOUT_VERSION_8 = 8,   // Diff from 7 to 8:  Use murmur instead of crc32.  We are going to make a simplification and stop supporting version 7 and before.  Current As of Beta 1.0.6
    FT_LAYOUT_VERSION_9 = 9,   // Diff from 8 to 9:  Variable-sized blocks and compression.
    FT_LAYOUT_VERSION_10 = 10, // Diff from 9 to 10: Variable number of compressed sub-blocks per block, disk byte order == intel byte order, Subtree estimates instead of just leafentry estimates, translation table, dictionary descriptors, checksum in header, subdb support removed from brt layer
    FT_LAYOUT_VERSION_11 = 11, // Diff from 10 to 11: Nested transaction leafentries (completely redesigned).  FT_CMDs on disk now support XIDS (multiple txnids) instead of exactly one.
    FT_LAYOUT_VERSION_12 = 12, // Diff from 11 to 12: Added FT_CMD 'FT_INSERT_NO_OVERWRITE', compressed block format, num old blocks
    FT_LAYOUT_VERSION_13 = 13, // Diff from 12 to 13: Fixed loader pivot bug, added build_id to every node, timestamps to ft 
    FT_LAYOUT_VERSION_14 = 14, // Diff from 13 to 14: Added MVCC; deprecated TOKU_DB_VALCMP_BUILTIN(_13); Remove fingerprints; Support QUICKLZ; add end-to-end checksum on uncompressed data.
    FT_LAYOUT_VERSION_15 = 15, // Diff from 14 to 15: basement nodes, last verification time
    FT_LAYOUT_VERSION_16 = 16, // Dr. No:  No subtree estimates, partition layout information represented more transparently. 
                                // ALERT ALERT ALERT: version 16 never released to customers, internal and beta use only
    FT_LAYOUT_VERSION_17 = 17, // Dr. No:  Add STAT64INFO_S to brt_header
    FT_LAYOUT_VERSION_18 = 18, // Dr. No:  Add HOT info to brt_header
    FT_LAYOUT_VERSION_19 = 19, // Doofenshmirtz: Add compression method, highest_unused_msn_for_upgrade
    FT_LAYOUT_VERSION_20 = 20, // Deadshot: Add compression method to log_fcreate,
                               // mgr_last_xid after begin checkpoint,
                               // last_xid to shutdown
    FT_NEXT_VERSION,           // the version after the current version
    FT_LAYOUT_VERSION   = FT_NEXT_VERSION-1, // A hack so I don't have to change this line.
    FT_LAYOUT_MIN_SUPPORTED_VERSION = FT_LAYOUT_VERSION_13, // Minimum version supported

    // Define these symbolically so the knowledge of exactly which layout version got rid of fingerprints isn't spread all over the code.
    FT_LAST_LAYOUT_VERSION_WITH_FINGERPRINT = FT_LAYOUT_VERSION_13,
    FT_FIRST_LAYOUT_VERSION_WITH_END_TO_END_CHECKSUM = FT_LAYOUT_VERSION_14,
    FT_FIRST_LAYOUT_VERSION_WITH_BASEMENT_NODES = FT_LAYOUT_VERSION_15,
};

#endif
