/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

/* Purpose of this file is to provide the world with everything necessary
 * to use the xids and nothing else.  
 * Internal requirements of the xids logic do not belong here.
 *
 * xids is (abstractly) an immutable list of nested transaction ids, accessed only
 * via the functions in this file.  
 *
 * See design documentation for nested transactions at
 * TokuWiki/Imp/TransactionsOverview.
 */

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

#pragma once

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "ft/txn/txn.h"
#include "ft/serialize/rbuf.h"
#include "ft/serialize/wbuf.h"

/* The number of transaction ids stored in the xids structure is 
 * represented by an 8-bit value.  The value 255 is reserved. 
 * The constant MAX_NESTED_TRANSACTIONS is one less because
 * one slot in the packed leaf entry is used for the implicit
 * root transaction (id 0).
 */
enum {
    MAX_NESTED_TRANSACTIONS = 253,
    MAX_TRANSACTION_RECORDS = MAX_NESTED_TRANSACTIONS + 1
};

// Variable size list of transaction ids (known in design doc as xids<>).
// ids[0] is the outermost transaction.
// ids[num_xids - 1] is the innermost transaction.
// Should only be accessed by accessor functions toku_xids_xxx, not directly.

// If the xids struct is unpacked, the compiler aligns the ids[] and we waste a lot of space
struct __attribute__((__packed__)) XIDS_S {
    // maximum value of MAX_TRANSACTION_RECORDS - 1 because transaction 0 is implicit
    uint8_t num_xids; 
    TXNID ids[];
};
typedef struct XIDS_S *XIDS;

// Retrieve an XIDS representing the root transaction.
XIDS toku_xids_get_root_xids(void);

bool toku_xids_can_create_child(XIDS xids);

void toku_xids_cpy(XIDS target, XIDS source);

//Creates an XIDS representing this transaction.
//You must pass in an XIDS representing the parent of this transaction.
int toku_xids_create_child(XIDS parent_xids, XIDS *xids_p, TXNID this_xid);

// The following two functions (in order) are equivalent to toku_xids_create child,
// but allow you to do most of the work without knowing the new xid.
int toku_xids_create_unknown_child(XIDS parent_xids, XIDS *xids_p);
void toku_xids_finalize_with_child(XIDS xids, TXNID this_xid);

void toku_xids_create_from_buffer(struct rbuf *rb, XIDS *xids_p);

void toku_xids_destroy(XIDS *xids_p);

TXNID toku_xids_get_xid(XIDS xids, uint8_t index);

uint8_t toku_xids_get_num_xids(XIDS xids);

TXNID toku_xids_get_innermost_xid(XIDS xids);
TXNID toku_xids_get_outermost_xid(XIDS xids);

// return size in bytes
uint32_t toku_xids_get_size(XIDS xids);

uint32_t toku_xids_get_serialize_size(XIDS xids);

unsigned char *toku_xids_get_end_of_array(XIDS xids);

void wbuf_nocrc_xids(struct wbuf *wb, XIDS xids);

void toku_xids_fprintf(FILE* fp, XIDS xids);
