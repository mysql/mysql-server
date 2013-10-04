/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ifndef TOKURECOVER_H
#define TOKURECOVER_H

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

#include <toku_portability.h>
#include <errno.h>

#include <db.h>
#include "fttypes.h"
#include "memory.h"
#include "x1764.h"


typedef void (*prepared_txn_callback_t)(DB_ENV*, TOKUTXN);
typedef void (*keep_cachetable_callback_t)(DB_ENV*, CACHETABLE);

// Run tokudb recovery from the log
// Returns 0 if success
int tokudb_recover (DB_ENV *env,
		    prepared_txn_callback_t    prepared_txn_callback,
		    keep_cachetable_callback_t keep_cachetable_callback,
		    TOKULOGGER logger,
		    const char *env_dir, const char *log_dir,
                    ft_compare_func bt_compare,
                    ft_update_func update_function,
                    generate_row_for_put_func       generate_row_for_put,
                    generate_row_for_del_func       generate_row_for_del,
                    size_t cachetable_size);

// Effect: Check the tokudb logs to determine whether or not we need to run recovery.
// If the log is empty or if there is a clean shutdown at the end of the log, then we
// dont need to run recovery.
// Returns: true if we need recovery, otherwise false.
int tokudb_needs_recovery(const char *logdir, bool ignore_empty_log);

// Return 0 if recovery log exists, ENOENT if log is missing
int tokudb_recover_log_exists(const char * log_dir);

// For test only - set callbacks for recovery testing
void toku_recover_set_callback (void (*)(void*), void*);
void toku_recover_set_callback2 (void (*)(void*), void*);

extern int tokudb_recovery_trace;

int toku_recover_lock (const char *lock_dir, int *lockfd);

int toku_recover_unlock(int lockfd);

static const prepared_txn_callback_t NULL_prepared_txn_callback         __attribute__((__unused__)) = NULL;
static const keep_cachetable_callback_t  NULL_keep_cachetable_callback  __attribute__((__unused__)) = NULL;


#endif // TOKURECOVER_H
