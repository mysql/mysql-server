/* -*- mode: C; c-basic-offset: 4 -*- */

#ifndef XIDS_INTERNAL_H
#define XIDS_INTERNAL_H

#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#if defined(__cplusplus) || defined(__cilkplusplus)
extern "C" {
#endif

// Variable size list of transaction ids (known in design doc as xids<>).
// ids[0] is the outermost transaction.
// ids[num_xids - 1] is the innermost transaction.
// Should only be accessed by accessor functions xids_xxx, not directly.

// If the xids struct is unpacked, the compiler aligns the ids[] and we waste a lot of space
#if TOKU_WINDOWS
#pragma pack(push, 1)
#endif

typedef struct __attribute__((__packed__)) xids_t {
    u_int8_t  num_stored_xids;    // maximum value of MAX_TRANSACTION_RECORDS - 1 ...
				  // ... because transaction 0 is implicit
    TXNID     ids[];
} XIDS_S;

#if TOKU_WINDOWS
#pragma pack(pop)
#endif

#if defined(__cplusplus) || defined(__cilkplusplus)
};
#endif

#endif
