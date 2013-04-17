/* -*- mode: C; c-basic-offset: 4 -*- */
#ifndef BRT_LAYOUT_VERSION_H
#define BRT_LAYOUT_VERSION_H

#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

//Must be defined before other recursive headers could include logger.h
enum brt_layout_version_e {
    BRT_LAYOUT_VERSION_5 = 5,
    BRT_LAYOUT_VERSION_6 = 6,   // Diff from 5 to 6:  Add leafentry_estimate
    BRT_LAYOUT_VERSION_7 = 7,   // Diff from 6 to 7:  Add exact-bit to leafentry_estimate #818, add magic to header #22, add per-subdatase flags #333
    BRT_LAYOUT_VERSION_8 = 8,   // Diff from 7 to 8:  Use murmur instead of crc32.  We are going to make a simplification and stop supporting version 7 and before.  Current As of Beta 1.0.6
    BRT_LAYOUT_VERSION_9 = 9,   // Diff from 8 to 9:  Variable-sized blocks and compression.
    BRT_LAYOUT_VERSION_10 = 10, // Diff from 9 to 10: Variable number of compressed sub-blocks per block, disk byte order == intel byte order, Subtree estimates instead of just leafentry estimates, translation table, dictionary descriptors, checksum in header, subdb support removed from brt layer
    BRT_LAYOUT_VERSION_11 = 11, // Diff from 10 to 11: Nested transaction leafentries (completely redesigned).  BRT_CMDs on disk now support XIDS (multiple txnids) instead of exactly one.
    BRT_LAYOUT_VERSION_12 = 12, // Diff from 11 to 12: Added BRT_CMD 'BRT_INSERT_NO_OVERWRITE', compressed block format, num old blocks
    BRT_LAYOUT_VERSION_13 = 13, // Diff from 12 to 13: Fixed loader pivot bug, added build_id to every node, timestamps to brtheader 
    BRT_LAYOUT_VERSION_14 = 14, // Diff from 13 to 14: Added MVCC; deprecated TOKU_DB_VALCMP_BUILTIN(_13); Remove fingerprints; Support QUICKLZ; add end-to-end checksum on uncompressed data.
    BRT_LAYOUT_VERSION_15 = 15, // Diff from 14 to 15: TODO
    BRT_NEXT_VERSION,           // the version after the current version
    BRT_LAYOUT_VERSION   = BRT_NEXT_VERSION-1, // A hack so I don't have to change this line.
    BRT_LAYOUT_MIN_SUPPORTED_VERSION = BRT_LAYOUT_VERSION_13, // Minimum version supported

    // Define these symbolically so the knowledge of exactly which layout version got rid of fingerprints isn't spread all over the code.
    BRT_LAST_LAYOUT_VERSION_WITH_FINGERPRINT = BRT_LAYOUT_VERSION_13,
    BRT_FIRST_LAYOUT_VERSION_WITH_END_TO_END_CHECKSUM = BRT_LAYOUT_VERSION_14,
    BRT_FIRST_LAYOUT_VERSION_WITH_BASEMENT_NODES = BRT_LAYOUT_VERSION_15,
};

#endif
