/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ifndef TOKU_CHECKPOINT_H
#define TOKU_CHECKPOINT_H

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

#ident "Copyright (c) 2009-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
#ident "$Id$"

#include "cachetable.h"

#include <stdint.h>

void toku_set_checkpoint_period(CACHETABLE ct, uint32_t new_period);
//Effect: Change [end checkpoint (n) - begin checkpoint (n+1)] delay to
//        new_period seconds.  0 means disable.

uint32_t toku_get_checkpoint_period_unlocked(CACHETABLE ct);


/******
 *
 * NOTE: checkpoint_safe_lock is highest level lock
 *       multi_operation_lock is next level lock
 *       ydb_big_lock is next level lock
 *
 *       Locks must always be taken in this sequence (highest level first).
 *
 */


/****** 
 * Client code must hold the checkpoint_safe lock during the following operations:
 *       - delete a dictionary via DB->remove
 *       - delete a dictionary via DB_TXN->abort(txn) (where txn created a dictionary)
 *       - rename a dictionary //TODO: Handlerton rename needs to take this
 *                             //TODO: Handlerton rename needs to be recoded for transaction recovery
 *****/

void toku_checkpoint_safe_client_lock(void);

void toku_checkpoint_safe_client_unlock(void);



/****** 
 * These functions are called from the ydb level.
 * Client code must hold the multi_operation lock during the following operations:
 *       - insertion into multiple indexes
 *       - replace into (simultaneous delete/insert on a single key)
 *****/

void toku_multi_operation_client_lock(void);
void toku_low_priority_multi_operation_client_lock(void);

void toku_multi_operation_client_unlock(void);
void toku_low_priority_multi_operation_client_unlock(void);


// Initialize the checkpoint mechanism, must be called before any client operations.
// Must pass in function pointers to take/release ydb lock.
void toku_checkpoint_init(void);

void toku_checkpoint_destroy(void);

typedef enum {SCHEDULED_CHECKPOINT  = 0,   // "normal" checkpoint taken on checkpoint thread
              CLIENT_CHECKPOINT     = 1,   // induced by client, such as FLUSH LOGS or SAVEPOINT
              INDEXER_CHECKPOINT    = 2,
              STARTUP_CHECKPOINT    = 3,
              UPGRADE_CHECKPOINT    = 4,
              RECOVERY_CHECKPOINT   = 5,
              SHUTDOWN_CHECKPOINT   = 6} checkpoint_caller_t;

// Take a checkpoint of all currently open dictionaries
// Callbacks are called during checkpoint procedure while checkpoint_safe lock is still held.
// Callbacks are primarily intended for use in testing.
// caller_id identifies why the checkpoint is being taken.
int toku_checkpoint(CHECKPOINTER cp, TOKULOGGER logger,
                    void (*callback_f)(void*),  void * extra,
                    void (*callback2_f)(void*), void * extra2,
                    checkpoint_caller_t caller_id);



/******
 * These functions are called from the ydb level.
 * They return status information and have no side effects.
 * Some status information may be incorrect because no locks are taken to collect status.
 * (If checkpoint is in progress, it may overwrite status info while it is being read.)
 *****/
typedef enum {
    CP_PERIOD,
    CP_FOOTPRINT,
    CP_TIME_LAST_CHECKPOINT_BEGIN,
    CP_TIME_LAST_CHECKPOINT_BEGIN_COMPLETE,
    CP_TIME_LAST_CHECKPOINT_END,
    CP_LAST_LSN,
    CP_CHECKPOINT_COUNT,
    CP_CHECKPOINT_COUNT_FAIL,
    CP_WAITERS_NOW,          // how many threads are currently waiting for the checkpoint_safe lock to perform a checkpoint
    CP_WAITERS_MAX,          // max threads ever simultaneously waiting for the checkpoint_safe lock to perform a checkpoint
    CP_CLIENT_WAIT_ON_MO,    // how many times a client thread waited to take the multi_operation lock, not for checkpoint
    CP_CLIENT_WAIT_ON_CS,    // how many times a client thread waited for the checkpoint_safe lock, not for checkpoint
    CP_BEGIN_TIME,
    CP_LONG_BEGIN_TIME,
    CP_LONG_BEGIN_COUNT,
    CP_STATUS_NUM_ROWS       // number of rows in this status array.  must be last.
} cp_status_entry;

typedef struct {
    bool initialized;
    TOKU_ENGINE_STATUS_ROW_S status[CP_STATUS_NUM_ROWS];
} CHECKPOINT_STATUS_S, *CHECKPOINT_STATUS;

void toku_checkpoint_get_status(CACHETABLE ct, CHECKPOINT_STATUS stat);


#endif
