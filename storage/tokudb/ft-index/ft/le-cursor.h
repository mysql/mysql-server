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

#ident "Copyright (c) 2010-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#ifndef LE_CURSOR_H
#define LE_CURSOR_H

#include "ft-ops.h"

// A leaf entry cursor (LE_CURSOR) is a special type of FT_CURSOR that visits all of the leaf entries in a tree
// and returns the leaf entry to the caller.  It maintains a copy of the key that it was last positioned over to
// speed up key comparisions with a given key.  For example, the hot indexing could use the _key_right_of_cursor
// function to determine where a given key sits relative to the LE_CURSOR position.

// When _next and _key_right_of_cursor functions are run on multiple threads, they must be protected by a lock.  This
// lock is assumed to exist outside of the LE_CURSOR.

typedef struct le_cursor *LE_CURSOR;

// Create a leaf cursor for a tree (brt) within a transaction (txn)
// Success: returns 0, stores the LE_CURSOR in the le_cursor_result
// Failure: returns a non-zero error number
int toku_le_cursor_create(LE_CURSOR *le_cursor_result, FT_HANDLE brt, TOKUTXN txn);

// Close and free the LE_CURSOR
void toku_le_cursor_close(LE_CURSOR le_cursor);

// Move to the next leaf entry under the LE_CURSOR
// Success: returns zero, calls the getf callback with the getf_v parameter
// Failure: returns a non-zero error number
int toku_le_cursor_next(LE_CURSOR le_cursor, FT_GET_CALLBACK_FUNCTION getf, void *getf_v);

// Return true if the key is to the right of the LE_CURSOR position. that is, current cursor key < given key
// Otherwise returns false when the key is at or to the left of the LE_CURSOR position. that is, current cursor key >= given key
// The LE_CURSOR position is intialized to -infinity. Any key comparision with -infinity returns true.
// When the cursor runs off the right edge of the tree, the LE_CURSOR position is set to +infinity.  Any key comparision with +infinity
// returns false.
bool toku_le_cursor_is_key_greater_or_equal(LE_CURSOR le_cursor, const DBT *key);

// extracts position of le_cursor into estimate. Responsibility of caller to handle
// thread safety. Caller (the indexer), does so by ensuring indexer lock is held
void toku_le_cursor_update_estimate(LE_CURSOR le_cursor, DBT* estimate);

#endif
