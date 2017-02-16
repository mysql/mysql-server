/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

/* Purpose of this file is to provide the test programs with internal 
 * ule mechanisms that do not belong in the public interface.
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


//1 does much slower debugging
#define ULE_DEBUG 0

/////////////////////////////////////////////////////////////////////////////////
// Following data structures are the unpacked format of a leafentry. 
//   * ule is the unpacked leaf entry, that contains an array of unpacked
//     transaction records
//   * uxr is the unpacked transaction record
//


//Types of transaction records.
enum {XR_INSERT      = 1,
      XR_DELETE      = 2,
      XR_PLACEHOLDER = 3};

typedef struct uxr {     // unpacked transaction record
    uint8_t   type;     // delete/insert/placeholder
    uint32_t  vallen;   // number of bytes in value
    void *    valp;     // pointer to value  (Where is value really stored?)
    TXNID     xid;      // transaction id
    // Note: when packing ule into a new leafentry, will need
    //       to copy actual data from valp to new leafentry
} UXR_S, *UXR;


// Unpacked Leaf Entry is of fixed size because it's just on the 
// stack and we care about ease of access more than the memory footprint.
typedef struct ule {     // unpacked leaf entry
    uint32_t  num_puxrs;   // how many of uxrs[] are provisional
    uint32_t  num_cuxrs;   // how many of uxrs[] are committed
    UXR_S     uxrs_static[MAX_TRANSACTION_RECORDS*2];    // uxrs[0] is oldest committed (txn commit time, not txn start time), uxrs[num_cuxrs] is outermost provisional value (if any exist/num_puxrs > 0)
    UXR       uxrs;                                      //If num_cuxrs < MAX_TRANSACTION_RECORDS then &uxrs_static[0].
                                                         //Otherwise we use a dynamically allocated array of size num_cuxrs + 1 + MAX_TRANSATION_RECORD.
} ULE_S, *ULE;



void test_msg_modify_ule(ULE ule, const ft_msg &msg);


//////////////////////////////////////////////////////////////////////////////////////
//Functions exported for test purposes only (used internally for non-test purposes).
void le_unpack(ULE ule,  LEAFENTRY le);
int
le_pack(ULE ule, // data to be packed into new leafentry
        bn_data* data_buffer,
        uint32_t idx,
        void* keyp,
        uint32_t keylen,
        uint32_t old_keylen,
        uint32_t old_le_size,
        LEAFENTRY * const new_leafentry_p, // this is what this function creates
        void **const maybe_free
        );


size_t le_memsize_from_ule (ULE ule);
void ule_cleanup(ULE ule);
