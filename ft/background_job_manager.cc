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

#ident "Copyright (c) 2011-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "background_job_manager.h"
#include "toku_config.h"
#include <memory.h>
#include <toku_pthread.h>

struct background_job_manager_struct {
    bool accepting_jobs;
    uint32_t num_jobs;
    toku_cond_t jobs_wait;
    toku_mutex_t jobs_lock;
};

void bjm_init(BACKGROUND_JOB_MANAGER* pbjm) {
    BACKGROUND_JOB_MANAGER XCALLOC(bjm);
    toku_mutex_init(&bjm->jobs_lock, 0);    
    toku_cond_init(&bjm->jobs_wait, NULL);
    bjm->accepting_jobs = true;
    bjm->num_jobs = 0;
    *pbjm = bjm;
}

void bjm_destroy(BACKGROUND_JOB_MANAGER bjm) {
    assert(bjm->num_jobs == 0);
    toku_cond_destroy(&bjm->jobs_wait);
    toku_mutex_destroy(&bjm->jobs_lock);
    toku_free(bjm);
}

void bjm_reset(BACKGROUND_JOB_MANAGER bjm) {
    toku_mutex_lock(&bjm->jobs_lock);
    assert(bjm->num_jobs == 0);
    bjm->accepting_jobs = true;
    toku_mutex_unlock(&bjm->jobs_lock);
}

int bjm_add_background_job(BACKGROUND_JOB_MANAGER bjm) {
    int ret_val;
    toku_mutex_lock(&bjm->jobs_lock);
    if (bjm->accepting_jobs) {
        bjm->num_jobs++;
        ret_val = 0;
    }
    else {
        ret_val = -1;
    }
    toku_mutex_unlock(&bjm->jobs_lock);
    return ret_val;
}
void bjm_remove_background_job(BACKGROUND_JOB_MANAGER bjm){
    toku_mutex_lock(&bjm->jobs_lock);
    assert(bjm->num_jobs > 0);
    bjm->num_jobs--;
    if (bjm->num_jobs == 0 && !bjm->accepting_jobs) {
        toku_cond_broadcast(&bjm->jobs_wait);
    }
    toku_mutex_unlock(&bjm->jobs_lock);
}

void bjm_wait_for_jobs_to_finish(BACKGROUND_JOB_MANAGER bjm) {
    toku_mutex_lock(&bjm->jobs_lock);
    bjm->accepting_jobs = false;
    while (bjm->num_jobs > 0) {
        toku_cond_wait(&bjm->jobs_wait, &bjm->jobs_lock);
    }
    toku_mutex_unlock(&bjm->jobs_lock);
}

