/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ifndef FTTYPES_H
#define FTTYPES_H

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

  TokuDB, Tokutek Fractal Tree Indexing Library.
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

#include <sys/types.h>
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 500
#endif
#define _FILE_OFFSET_BITS 64

#include "toku_assert.h"
#include <db.h>
#include <inttypes.h>


// Use the C++ bool and constants (true false), rather than BOOL, TRUE, and FALSE.

typedef struct ft_handle *FT_HANDLE;
typedef struct ftnode *FTNODE;
typedef struct ftnode_disk_data *FTNODE_DISK_DATA;
typedef struct ftnode_leaf_basement_node *BASEMENTNODE;
typedef struct ftnode_nonleaf_childinfo *NONLEAF_CHILDINFO;
typedef struct sub_block *SUB_BLOCK;
typedef struct ft *FT;
typedef struct ft_header *FT_HEADER;
typedef struct ft_options *FT_OPTIONS;

typedef unsigned int ITEMLEN;
typedef const void *bytevec;

typedef int64_t DISKOFF;  /* Offset in a disk. -1 is the NULL pointer. */
typedef uint64_t TXNID;

typedef struct txnid_pair_s {
    TXNID parent_id64;
    TXNID child_id64;
} TXNID_PAIR;


#define TXNID_NONE_LIVING ((TXNID)0)
#define TXNID_NONE        ((TXNID)0)
#define TXNID_MAX         ((TXNID)-1)

static const TXNID_PAIR TXNID_PAIR_NONE = { .parent_id64 = TXNID_NONE, .child_id64 = TXNID_NONE };

typedef struct blocknum_s { int64_t b; } BLOCKNUM; // make a struct so that we will notice type problems.
typedef struct gid_s { uint8_t *gid; } GID; // the gid is of size [DB_GID_SIZE]
typedef TOKU_XA_XID *XIDP; // this is the type that's passed to the logger code (so that we don't have to copy all 152 bytes when only a subset are even valid.)
#define ROLLBACK_NONE     ((BLOCKNUM){0})

static inline BLOCKNUM make_blocknum(int64_t b) { BLOCKNUM result={b}; return result; }

typedef struct {
    uint32_t len;
    char *data;
} BYTESTRING;

/* Log Sequence Number (LSN)
 * Make the LSN be a struct instead of an integer so that we get better type checking. */
typedef struct __toku_lsn { uint64_t lsn; } LSN;
#define ZERO_LSN ((LSN){0})
#define MAX_LSN  ((LSN){UINT64_MAX})

/* Message Sequence Number (MSN)
 * Make the MSN be a struct instead of an integer so that we get better type checking. */
typedef struct __toku_msn { uint64_t msn; } MSN;
#define ZERO_MSN ((MSN){0})                 // dummy used for message construction, to be filled in when msg is applied to tree
#define MIN_MSN  ((MSN){(uint64_t)1 << 62})  // first 2^62 values reserved for messages created before Dr. No (for upgrade)
#define MAX_MSN  ((MSN){UINT64_MAX})

typedef struct {
    int64_t numrows;           // delta versions in basements could be negative
    int64_t numbytes;
} STAT64INFO_S, *STAT64INFO;

static const STAT64INFO_S ZEROSTATS = {0,0};

/* At the ft layer, a FILENUM uniquely identifies an open file.
 * At the ydb layer, a DICTIONARY_ID uniquely identifies an open dictionary.
 * With the introduction of the loader (ticket 2216), it is possible for the file that holds
 * an open dictionary to change, so these are now separate and independent unique identifiers.
 */
typedef struct {uint32_t fileid;} FILENUM;
#define FILENUM_NONE ((FILENUM){UINT32_MAX})

typedef struct {uint64_t dictid;} DICTIONARY_ID;
#define DICTIONARY_ID_NONE ((DICTIONARY_ID){0})

typedef struct {
    uint32_t num;
    FILENUM  *filenums;
} FILENUMS;

typedef struct tokulogger *TOKULOGGER;
typedef struct txn_manager *TXN_MANAGER;
typedef struct tokutxn    *TOKUTXN;

typedef struct xids_t *XIDS;

typedef int (*ft_compare_func)(DB *, const DBT *, const DBT *);
typedef void (*setval_func)(const DBT *, void *);
typedef int (*ft_update_func)(DB *, const DBT *, const DBT *, const DBT *, setval_func, void *);
typedef void (*remove_ft_ref_callback)(FT, void*);
typedef void (*on_redirect_callback)(FT_HANDLE, void*);

#define UU(x) x __attribute__((__unused__))

#endif
