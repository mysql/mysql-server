/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2011 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <config.h>
#include <stdbool.h>
#include <toku_pthread.h>
#include "kibbutz.h"
#include "background_job_manager.h"
#include "includes.h"

struct background_job_manager_struct {
    bool accepting_jobs;
    u_int32_t num_jobs;
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

