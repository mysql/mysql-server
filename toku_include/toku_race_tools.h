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
#ifndef TOKU_RACE_TOOLS_H
#define TOKU_RACE_TOOLS_H

#include "toku_config.h"

#if defined(__linux__) && USE_VALGRIND

# include <valgrind/helgrind.h>
# include <valgrind/drd.h>

# define TOKU_ANNOTATE_NEW_MEMORY(p, size) ANNOTATE_NEW_MEMORY(p, size)
# define TOKU_VALGRIND_HG_ENABLE_CHECKING(p, size) VALGRIND_HG_ENABLE_CHECKING(p, size)
# define TOKU_VALGRIND_HG_DISABLE_CHECKING(p, size) VALGRIND_HG_DISABLE_CHECKING(p, size)
# define TOKU_DRD_IGNORE_VAR(v) DRD_IGNORE_VAR(v)
# define TOKU_DRD_STOP_IGNORING_VAR(v) DRD_STOP_IGNORING_VAR(v)

/*
 * How to make helgrind happy about tree rotations and new mutex orderings:
 *
 * // Tell helgrind that we unlocked it so that the next call doesn't get a "destroyed a locked mutex" error.
 * // Tell helgrind that we destroyed the mutex.
 * VALGRIND_HG_MUTEX_UNLOCK_PRE(&locka);
 * VALGRIND_HG_MUTEX_DESTROY_PRE(&locka);
 *
 * // And recreate it.  It would be better to simply be able to say that the order on these two can now be reversed, because this code forgets all the ordering information for this mutex.
 * // Then tell helgrind that we have locked it again.
 * VALGRIND_HG_MUTEX_INIT_POST(&locka, 0);
 * VALGRIND_HG_MUTEX_LOCK_POST(&locka);
 *
 * When the ordering of two locks changes, we don't need tell Helgrind about do both locks.  Just one is good enough.
 */

# define TOKU_VALGRIND_RESET_MUTEX_ORDERING_INFO(mutex)  \
    VALGRIND_HG_MUTEX_UNLOCK_PRE(mutex); \
    VALGRIND_HG_MUTEX_DESTROY_PRE(mutex); \
    VALGRIND_HG_MUTEX_INIT_POST(mutex, 0); \
    VALGRIND_HG_MUTEX_LOCK_POST(mutex);

#else // !defined(__linux__) || !USE_VALGRIND

# define NVALGRIND 1
# define TOKU_ANNOTATE_NEW_MEMORY(p, size) ((void) 0)
# define TOKU_VALGRIND_HG_ENABLE_CHECKING(p, size) ((void) 0)
# define TOKU_VALGRIND_HG_DISABLE_CHECKING(p, size) ((void) 0)
# define TOKU_DRD_IGNORE_VAR(v)
# define TOKU_DRD_STOP_IGNORING_VAR(v)
# define TOKU_VALGRIND_RESET_MUTEX_ORDERING_INFO(mutex)

#endif

#endif // TOKU_RACE_TOOLS_H
